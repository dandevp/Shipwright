#include "CoopSettings.h"

#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/ShipInit.hpp"

namespace CoopSettings {

namespace {
struct Registry {
    std::vector<std::string> cvars;
    std::vector<std::function<void()>> onChange;
};

// Filled during static initialization by the features' Shared objects, so it can't be a plain static
Registry& GetRegistry() {
    static Registry registry;
    return registry;
}

const nlohmann::json* GetOwnerSettings() {
    Anchor* anchor = Anchor::Instance;
    if (anchor == nullptr || !anchor->isConnected || !anchor->roomState.coopSync ||
        !anchor->roomState.coopSettings.is_object()) {
        return nullptr;
    }
    return &anchor->roomState.coopSettings;
}
} // namespace

Shared::Shared(std::initializer_list<std::string> cvars, std::function<void()> onChange)
    : Shared(std::vector<std::string>(cvars), std::move(onChange)) {
}

Shared::Shared(std::vector<std::string> cvars, std::function<void()> onChange) {
    Registry& registry = GetRegistry();
    registry.cvars.insert(registry.cvars.end(), cvars.begin(), cvars.end());
    if (onChange) {
        registry.onChange.push_back(std::move(onChange));
    }
}

bool IsEnforced() {
    return GetOwnerSettings() != nullptr;
}

int32_t GetInteger(const char* cvarName, int32_t defaultValue) {
    const nlohmann::json* ownerSettings = GetOwnerSettings();
    if (ownerSettings == nullptr) {
        return CVarGetInteger(cvarName, defaultValue);
    }

    auto it = ownerSettings->find(cvarName);
    return it != ownerSettings->end() && it->is_number_integer() ? it->get<int32_t>() : defaultValue;
}

uint32_t GetEnemySeed() {
    return Anchor::Instance != nullptr ? Anchor::Instance->roomState.coopEnemySeed : 0;
}

const std::vector<std::string>& GetSharedCVars() {
    return GetRegistry().cvars;
}

void Update() {
    // What the settings in effect come down to: our own CVars (which the menu re-inits on its own when changed), or
    // the owner's values and seed
    static std::string lastApplied = "";
    const nlohmann::json* ownerSettings = GetOwnerSettings();
    std::string applied =
        ownerSettings != nullptr ? ownerSettings->dump() + std::to_string(GetEnemySeed()) : std::string();
    if (applied == lastApplied) {
        return;
    }
    lastApplied = applied;

    Registry& registry = GetRegistry();
    for (const std::string& cvar : registry.cvars) {
        ShipInit::Init(cvar);
    }
    for (const auto& onChange : registry.onChange) {
        onChange();
    }
}

} // namespace CoopSettings
