// SPDX-License-Identifier: GPL-3.0-only
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <stdexcept>
APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }
APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    if (std::string(__hyprland_api_get_hash()) != __hyprland_api_get_client_hash())
        throw std::runtime_error("workspace-ribbon: compositor/header ABI mismatch; rebuild");
    return {"workspace-ribbon", "Continuous workspace ribbon prototype", "villevaisanen", "0.1.0-dev"};
}
APICALL EXPORT void PLUGIN_EXIT() {}
