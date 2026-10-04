#include "nonceserver/http/HttpServer.h"

#include "nonceserver/log/Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace nonceserver {

namespace {

constexpr std::size_t kMaxHeaderBytes = 64 * 1024;
constexpr std::size_t kMaxBodyBytes = 1024 * 1024;

std::string Trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(begin, end - begin));
}

std::string ToLower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

int HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string PercentDecode(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const int hi = HexDigit(text[i + 1]);
            const int lo = HexDigit(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(text[i] == '+' ? ' ' : text[i]);
    }
    return out;
}

void ParseQuery(std::string_view query, std::map<std::string, std::string>& out) {
    std::size_t index = 0;
    while (index < query.size()) {
        std::size_t amp = query.find('&', index);
        if (amp == std::string_view::npos) amp = query.size();
        const std::string_view pair = query.substr(index, amp - index);
        const std::size_t eq = pair.find('=');
        if (eq == std::string_view::npos) {
            out[PercentDecode(pair)] = std::string();
        } else {
            out[PercentDecode(pair.substr(0, eq))] = PercentDecode(pair.substr(eq + 1));
        }
        index = amp + 1;
    }
}

class SocketTransport : public ITransport {
public:
    explicit SocketTransport(SOCKET socket) : socket_(socket) {}
    ~SocketTransport() override {
        if (socket_ != INVALID_SOCKET) closesocket(socket_);
    }

    Result<std::size_t> Read(uint8_t* buffer, std::size_t length) override {
        const int chunk = static_cast<int>(std::min<std::size_t>(length, 0x7FFFFFFF));
        const int received = recv(socket_, reinterpret_cast<char*>(buffer), chunk, 0);
        if (received == 0) return std::size_t{0};
        if (received == SOCKET_ERROR) {
            return Status::Error(ErrorCode::Unavailable, "recv failed");
        }
        return static_cast<std::size_t>(received);
    }

    Status Write(const uint8_t* data, std::size_t length) override {
        std::size_t sent = 0;
        while (sent < length) {
            const int chunk =
                static_cast<int>(std::min<std::size_t>(length - sent, 0x7FFFFFFF));
            const int written =
                send(socket_, reinterpret_cast<const char*>(data + sent), chunk, 0);
            if (written == SOCKET_ERROR) {
                return Status::Error(ErrorCode::Unavailable, "send failed");
            }
            sent += static_cast<std::size_t>(written);
        }
        return Status::Ok();
    }

private:
    SOCKET socket_;
};

Status ReadRequest(ITransport& transport, HttpRequest& request) {
    std::string buffer;
    std::size_t headerEnd = std::string::npos;
    uint8_t chunk[4096];

    while (headerEnd == std::string::npos) {
        if (buffer.size() > kMaxHeaderBytes) {
            return Status::Error(ErrorCode::InvalidArgument, "request header too large");
        }
        const auto read = transport.Read(chunk, sizeof(chunk));
        if (!read.ok()) return read.status();
        if (read.value() == 0) {
            return Status::Error(ErrorCode::Unavailable, "connection closed before request");
        }
        buffer.append(reinterpret_cast<const char*>(chunk), read.value());
        headerEnd = buffer.find("\r\n\r\n");
    }

    const std::string head = buffer.substr(0, headerEnd);
    std::istringstream stream(head);
    std::string requestLine;
    if (!std::getline(stream, requestLine)) {
        return Status::Error(ErrorCode::InvalidArgument, "missing request line");
    }
    if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

    std::istringstream requestLineStream(requestLine);
    std::string version;
    if (!(requestLineStream >> request.method >> request.target >> version)) {
        return Status::Error(ErrorCode::InvalidArgument, "malformed request line");
    }

    const std::size_t queryPos = request.target.find('?');
    if (queryPos == std::string::npos) {
        request.path = PercentDecode(request.target);
    } else {
        request.path = PercentDecode(request.target.substr(0, queryPos));
        ParseQuery(request.target.substr(queryPos + 1), request.query);
    }

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        request.headers[ToLower(Trim(line.substr(0, colon)))] = Trim(line.substr(colon + 1));
    }

    std::size_t contentLength = 0;
    const auto lengthIt = request.headers.find("content-length");
    if (lengthIt != request.headers.end()) {
        contentLength = static_cast<std::size_t>(std::strtoull(lengthIt->second.c_str(), nullptr, 10));
    }
    if (contentLength > kMaxBodyBytes) {
        return Status::Error(ErrorCode::InvalidArgument, "request body too large");
    }

    std::string body = buffer.substr(headerEnd + 4);
    while (body.size() < contentLength) {
        const auto read = transport.Read(chunk, sizeof(chunk));
        if (!read.ok()) return read.status();
        if (read.value() == 0) break;
        body.append(reinterpret_cast<const char*>(chunk), read.value());
    }
    if (body.size() > contentLength) body.resize(contentLength);
    request.body = std::move(body);
    return Status::Ok();
}

std::string BuildResponseBytes(const HttpResponse& response) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << ' ' << ReasonPhrase(response.status) << "\r\n";
    bool hasContentType = false;
    for (const auto& [name, value] : response.headers) {
        if (ToLower(name) == "content-type") hasContentType = true;
        if (ToLower(name) == "content-length" || ToLower(name) == "connection") continue;
        out << name << ": " << value << "\r\n";
    }
    if (!hasContentType) out << "Content-Type: application/octet-stream\r\n";
    out << "Content-Length: " << response.body.size() << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << response.body;
    return out.str();
}

}  // namespace

HttpServer::HttpServer(ServerConfig config, const Router& router,
                       std::shared_ptr<ITlsContext> tlsContext)
    : config_(std::move(config)),
      router_(router),
      tlsContext_(std::move(tlsContext)) {}

HttpServer::~HttpServer() { Stop(); }

void HttpServer::HandleConnection(std::uintptr_t clientSocket) {
    const SOCKET socket = static_cast<SOCKET>(clientSocket);

    std::unique_ptr<ITransport> transport;
    if (tlsContext_) {
        auto wrapped = tlsContext_->WrapServerSocket(clientSocket);
        if (!wrapped.ok()) {
            LogWarn("TLS handshake failed: " + wrapped.status().message());
            closesocket(socket);
            return;
        }
        transport = std::move(wrapped.value());
    } else {
        transport = std::make_unique<SocketTransport>(socket);
    }

    HttpRequest request;
    const Status read = ReadRequest(*transport, request);
    if (!read.ok()) {
        const HttpResponse response =
            HttpResponse::Error(400, "bad_request", read.message());
        const std::string bytes = BuildResponseBytes(response);
        transport->Write(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
        return;
    }

    HttpResponse response;
    const DispatchResult result = router_.Dispatch(request, response);
    if (result == DispatchResult::NotFound) {
        response = HttpResponse::Error(404, "not_found", "no route for " + request.path);
    } else if (result == DispatchResult::MethodNotAllowed) {
        response = HttpResponse::Error(405, "method_not_allowed", "method not allowed");
    }

    const std::string bytes = BuildResponseBytes(response);
    transport->Write(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}

void HttpServer::AcceptLoop() {
    while (running_) {
        sockaddr_in clientAddress{};
        int addressLength = sizeof(clientAddress);
        const SOCKET client =
            accept(static_cast<SOCKET>(listenSocket_), reinterpret_cast<sockaddr*>(&clientAddress),
                   &addressLength);
        if (client == INVALID_SOCKET) {
            if (running_) {
                LogWarn("accept failed");
            }
            continue;
        }
        // 设置接收超时，使关闭时工作线程有界退出（不会永久阻塞在 recv）。
        DWORD timeoutMs = 5000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs),
                   sizeof(timeoutMs));
        std::lock_guard<std::mutex> lock(workersMutex_);
        workers_.emplace_back(
            [this, client]() { HandleConnection(static_cast<std::uintptr_t>(client)); });
    }
}

Status HttpServer::Start() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return Status::Error(ErrorCode::Unavailable, "WSAStartup failed");
    }

    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        WSACleanup();
        return Status::Error(ErrorCode::Unavailable, "socket creation failed");
    }

    BOOL reuse = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
               sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config_.port);
    if (InetPtonA(AF_INET, config_.host.c_str(), &address.sin_addr) != 1) {
        closesocket(listener);
        WSACleanup();
        return Status::Error(ErrorCode::InvalidArgument, "invalid bind host: " + config_.host);
    }

    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        closesocket(listener);
        WSACleanup();
        return Status::Error(ErrorCode::Unavailable, "bind failed on " + config_.host + ":" +
                                                         std::to_string(config_.port));
    }
    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listener);
        WSACleanup();
        return Status::Error(ErrorCode::Unavailable, "listen failed");
    }

    listenSocket_ = static_cast<std::uintptr_t>(listener);
    running_ = true;
    LogInfo(std::string("listening on ") + (config_.tls.enabled ? "https://" : "http://") +
            config_.host + ":" + std::to_string(config_.port));
    if (!config_.tls.enabled) {
        LogWarn("TLS is disabled: plaintext HTTP is for local development only");
    }

    AcceptLoop();

    {
        std::lock_guard<std::mutex> lock(workersMutex_);
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
        workers_.clear();
    }

    closesocket(listener);
    listenSocket_ = static_cast<std::uintptr_t>(~0ull);
    WSACleanup();
    LogInfo("http server stopped");
    return Status::Ok();
}

void HttpServer::Stop() {
    if (!running_.exchange(false)) return;
    const std::uintptr_t socket = listenSocket_;
    if (socket != static_cast<std::uintptr_t>(~0ull)) {
        closesocket(static_cast<SOCKET>(socket));
    }
}

}  // namespace nonceserver
