// SPDX-License-Identifier: GPL-3.0-only
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <stdexcept>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <ribbon/Animation.hpp>
#include <chrono>
#include <map>
#include <vector>

namespace {
using Int = Config::Values::CIntValue;
SP<Int> enabled, duration, maximum, scaleDistance, easing;
std::vector<CHyprSignalListener> listeners;
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct MonitorAnimation {
    PHLMONITORREF monitor;
    WORKSPACEID last = 0;
    ribbon::Animation animation;
    std::vector<PHLWORKSPACE> cells;
    bool running = false;
};
std::map<MONITORID, MonitorAnimation> monitors;
void finish(MonitorAnimation& state, WORKSPACEID preserveA = 0, WORKSPACEID preserveB = 0) {
    for (auto& ws : state.cells) {
        if (!valid(ws)) continue;
        ws->m_forceRendering = false;
        if (ws->m_id == preserveA || ws->m_id == preserveB) continue;
        ws->m_renderOffset->setValueAndWarp(Vector2D{});
        ws->m_alpha->setValueAndWarp(1.F);
    }
    if (state.running && state.monitor) g_pHyprRenderer->damageMonitor(state.monitor.lock());
    state.cells.clear(); state.running = false;
}
bool eligible(PHLMONITOR monitor, double origin, WORKSPACEID destination) {
    if (!enabled->value() || destination <= 0 || monitor->m_activeSpecialWorkspace) return false;
    const double lo = std::min(origin + 1, double(destination)), hi = std::max(origin + 1, double(destination));
    for (const auto& ws : State::workspaceState()->workspaces()) {
        if (ws->m_id >= lo && ws->m_id <= hi && ws->m_monitor != monitor) return false;
        if (ws->m_monitor == monitor && ws->m_id > 0 && Fullscreen::controller()->hasFullscreen(ws.lock())) return false;
    }
    for (const auto& w : Desktop::windowState()->windows())
        if (w->m_monitor == monitor && w->m_isMapped && (w->m_isFloating || w->m_pinned)) return false;
    return true;
}
void apply(MonitorAnimation& state, double time) {
    if (!state.monitor) { finish(state); return; }
    auto monitor = state.monitor.lock();
    const auto sample = state.animation.sample(time);
    if (!sample.active || !eligible(monitor, sample.position, state.last)) { finish(state); return; }
    // Retain newly created local numeric workspaces too. No allocation in normal frames.
    for (const auto& ref : State::workspaceState()->workspaces()) {
        if (ref->m_monitor != monitor || ref->m_id <= 0) continue;
        if (std::ranges::find(state.cells, ref.lock()) == state.cells.end()) state.cells.push_back(ref.lock());
    }
    for (const auto& ws : state.cells) {
        if (!valid(ws) || ws->m_monitor != monitor) continue;
        const double coordinate = double(ws->m_id) - 1;
        ws->m_forceRendering = ribbon::visible(coordinate, sample.position);
        ws->m_alpha->setValueAndWarp(1.F);
        ws->m_renderOffset->setValueAndWarp(Vector2D{(coordinate-sample.position)*monitor->m_size.x, 0.0});
    }
    g_pHyprRenderer->damageMonitor(monitor);
}
void changed(PHLWORKSPACE ws) {
    if (!valid(ws) || !ws->m_monitor) return;
    auto monitor = ws->m_monitor.lock();
    auto& state = monitors[monitor->m_id];
    const auto previous = state.last;
    state.monitor = monitor; state.last = ws->m_id;
    const auto time = now();
    const auto origin = state.running ? state.animation.sample(time).position : double(previous)-1;
    if (previous <= 0 || !eligible(monitor, origin, ws->m_id)) { finish(state,previous,ws->m_id); return; }
    if (!state.running) state.animation = ribbon::Animation(origin);
    ribbon::Options options;
    options.durationMs=duration->value(); options.maxDurationMs=maximum->value();
    options.scaleDistance=scaleDistance->value(); options.easing=static_cast<ribbon::Easing>(easing->value());
    state.animation.retarget(double(ws->m_id)-1,time,options); state.running=true;
    apply(state,time);
}
SP<Int> registerOption(HANDLE handle, const char* name, const char* description, int value, int min, int max) {
    auto result=makeShared<Int>(name,description,value,Config::Values::SIntValueOptions{.min=min,.max=max});
    if (!HyprlandAPI::addConfigValueV2(handle,result)) throw std::runtime_error("workspace-ribbon: option registration failed");
    return result;
}
}
APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }
APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    if (std::string(__hyprland_api_get_hash()) != __hyprland_api_get_client_hash())
        throw std::runtime_error("workspace-ribbon: compositor/header ABI mismatch; rebuild");
    enabled=registerOption(handle,"plugin:workspace_ribbon:enabled","Enable ribbon",1,0,1);
    duration=registerOption(handle,"plugin:workspace_ribbon:duration_ms","Transition duration",400,1,10000);
    maximum=registerOption(handle,"plugin:workspace_ribbon:max_duration_ms","Duration cap",1200,1,10000);
    scaleDistance=registerOption(handle,"plugin:workspace_ribbon:scale_distance","Scale duration by distance",0,0,1);
    easing=registerOption(handle,"plugin:workspace_ribbon:easing","0 linear, 1 ease-out, 2 critically damped spring",1,0,2);
    for (const auto& monitor : State::monitorState()->monitors()) {
        auto& state=monitors[monitor->m_id]; state.monitor=monitor; state.last=monitor->activeWorkspaceID();
    }
    listeners.push_back(Event::bus()->m_events.workspace.active.listen([](PHLWORKSPACE ws) { changed(ws); }));
    listeners.push_back(Event::bus()->m_events.render.preChecks.listen([](PHLMONITOR monitor) {
        auto it=monitors.find(monitor->m_id);
        if (it!=monitors.end() && it->second.running) apply(it->second,now());
    }));
    listeners.push_back(Event::bus()->m_events.monitor.removed.listen([](PHLMONITOR monitor) {
        auto it=monitors.find(monitor->m_id); if(it!=monitors.end()) { finish(it->second); monitors.erase(it); }
    }));
    listeners.push_back(Event::bus()->m_events.workspace.moveToMonitor.listen([](PHLWORKSPACE,PHLMONITOR) {
        for(auto& [id,state]:monitors) finish(state);
    }));
    listeners.push_back(Event::bus()->m_events.config.reloaded.listen([]() {
        for(auto& [id,state]:monitors) finish(state);
    }));
    return {"workspace-ribbon", "Continuous workspace ribbon prototype", "villevaisanen", "0.1.0-dev"};
}
APICALL EXPORT void PLUGIN_EXIT() {
    listeners.clear();
    for(auto& [id,state]:monitors) finish(state);
    monitors.clear();
    enabled.reset(); duration.reset(); maximum.reset(); scaleDistance.reset(); easing.reset();
}
