// PulseSourceRegistry.h: 按 id 解析脉冲来源。
#pragma once

#include "nonceserver/pulse/PulseSource.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace nonceserver {

class PulseSourceRegistry {
public:
    void Register(std::shared_ptr<IPulseSource> source);
    std::shared_ptr<IPulseSource> Get(const std::string& id) const;
    bool Contains(const std::string& id) const;

private:
    std::unordered_map<std::string, std::shared_ptr<IPulseSource>> sources_;
};

}  // namespace nonceserver
