#include "nonceserver/pulse/PulseSourceRegistry.h"

namespace nonceserver {

void PulseSourceRegistry::Register(std::shared_ptr<IPulseSource> source) {
    if (!source) return;
    sources_[source->Id()] = std::move(source);
}

std::shared_ptr<IPulseSource> PulseSourceRegistry::Get(const std::string& id) const {
    const auto it = sources_.find(id);
    return it == sources_.end() ? nullptr : it->second;
}

bool PulseSourceRegistry::Contains(const std::string& id) const {
    return sources_.find(id) != sources_.end();
}

}  // namespace nonceserver
