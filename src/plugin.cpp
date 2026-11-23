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
#include <hyprland/src/render/ElementRenderer.hpp>
#include <ribbon/Animation.hpp>
#include <chrono>
#include <map>
#include <vector>

namespace {
using Int = Config::Values::CIntValue;
SP<Int> enabled, duration, maximum, scaleDistance, easing, zoomOut;
HANDLE pluginHandle = nullptr;
CFunctionHook* textureHook = nullptr;
bool zoomHintQueued = false;
std::vector<CHyprSignalListener> listeners;
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct MonitorAnimation {
    PHLMONITORREF monitor;
    WORKSPACEID last = 0;
    ribbon::Animation animation;
    std::vector<PHLWORKSPACE> cells;
    float scale = 1.F;
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
    state.cells.clear();
    state.running = false;
    state.scale = 1.F;
}
bool eligible(PHLMONITOR monitor, double origin, WORKSPACEID destination, double scale = 1.) {
    if (!enabled->value() || destination <= 0 || monitor->m_activeSpecialWorkspace) return false;
    const double lo = std::min(origin + 1, double(destination)), hi = std::max(origin + 1, double(destination));
    for (const auto& ws : State::workspaceState()->workspaces()) {
        const double id = double(ws->m_id);
        const bool foreignOnPath = id >= lo && id <= hi;
        const bool foreignAtEdge = scale < 1. && ribbon::visible(id, std::clamp(id, lo, hi), scale);
        if (ws->m_id > 0 && ws->m_monitor != monitor && (foreignOnPath || foreignAtEdge)) return false;
        if (scale < 1. && ws->m_monitor == monitor && ws->m_isSpecialWorkspace && ws->m_alpha->value() > 0.F) return false;
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
    if (!sample.active || !eligible(monitor, sample.position, state.last, sample.scale)) { finish(state); return; }
    state.scale = static_cast<float>(sample.scale);
    // Retain newly created local numeric workspaces too. No allocation in normal frames.
    for (const auto& ref : State::workspaceState()->workspaces()) {
        if (ref->m_monitor != monitor || ref->m_id <= 0) continue;
        if (std::ranges::find(state.cells, ref.lock()) == state.cells.end()) state.cells.push_back(ref.lock());
    }
    for (const auto& ws : state.cells) {
        if (!valid(ws) || ws->m_monitor != monitor) continue;
        const double coordinate = double(ws->m_id) - 1;
        ws->m_forceRendering = ribbon::visible(coordinate, sample.position, state.scale);
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
    const int zoomPercent = textureHook ? zoomOut->value() : 0;
    const double minimumScale = 1. - zoomPercent / 100.;
    if (previous <= 0 || !eligible(monitor, origin, ws->m_id, minimumScale)) { finish(state,previous,ws->m_id); return; }
    if (!state.running) state.animation = ribbon::Animation(origin);
    ribbon::Options options;
    options.durationMs=duration->value(); options.maxDurationMs=maximum->value();
    options.scaleDistance=scaleDistance->value(); options.easing=static_cast<ribbon::Easing>(easing->value());
    options.zoomOutPercent = zoomPercent;
    state.animation.retarget(double(ws->m_id)-1,time,options); state.running=true;
    apply(state,time);
}
// Native occlusion bounds are calculated before renderer hints are applied.
class RibbonZoomHint final : public CRendererHintsPassElement {
public:
    using CRendererHintsPassElement::CRendererHintsPassElement;
    static constexpr auto NAME = "WorkspaceRibbonZoomHint";
    const char* passName() override { return NAME; }
    bool disableSimplification() override { return true; }
};

using RenderMods = Render::SRenderModifData;
Vector2D zoomTranslation(PHLMONITOR monitor, float scale) {
    return monitor->m_transformedSize * ((1. - scale) / 2.);
}

void renderStage(eRenderStage stage) {
    auto& renderer = *g_pHyprRenderer;
    if (stage == RENDER_POST) {
        // The main pass can outlive the plugin, so remove its plugin-owned element.
        if (zoomHintQueued) {
            renderer.m_renderPass.removeAllOfType(RibbonZoomHint::NAME);
            zoomHintQueued = false;
        }
        return;
    }
    if (stage != RENDER_PRE_WINDOWS && stage != RENDER_POST_WINDOWS) return;

    const auto monitor = renderer.m_renderData.pMonitor.lock();
    if (!monitor || !monitor->m_renderingActive || &renderer.currentPass() != &renderer.m_renderPass) return;
    if (stage == RENDER_POST_WINDOWS) {
        if (zoomHintQueued)
            renderer.addPassElement(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{RenderMods{}}));
        return;
    }
    if (zoomHintQueued) return;

    const auto it = monitors.find(monitor->m_id);
    if (it == monitors.end() || !it->second.running || it->second.scale == 1.F) return;

    const float scale = it->second.scale;
    RenderMods mods;
    mods.modifs.emplace_back(RenderMods::RMOD_TYPE_SCALE, scale);
    mods.modifs.emplace_back(RenderMods::RMOD_TYPE_TRANSLATE, zoomTranslation(monitor, scale));
    renderer.addPassElement(makeUnique<RibbonZoomHint>(CRendererHintsPassElement::SData{std::move(mods)}));
    zoomHintQueued = true;
}

bool isRibbonZoom(const Render::SRenderData& frame) {
    if (!zoomHintQueued || !frame.pMonitor || !frame.renderModif.enabled || frame.renderModif.modifs.size() != 2) return false;
    const auto it = monitors.find(frame.pMonitor->m_id);
    if (it == monitors.end() || it->second.scale == 1.F) return false;

    const auto& [scaleType, scaleValue] = frame.renderModif.modifs[0];
    const auto& [translateType, translateValue] = frame.renderModif.modifs[1];
    const auto scale = std::any_cast<float>(&scaleValue);
    const auto translation = std::any_cast<Vector2D>(&translateValue);
    return scaleType == RenderMods::RMOD_TYPE_SCALE && translateType == RenderMods::RMOD_TYPE_TRANSLATE &&
           scale && *scale == it->second.scale && translation && *translation == zoomTranslation(frame.pMonitor.lock(), *scale);
}

void drawZoomedTexture(Render::IElementRenderer* self, WP<CTexPassElement> element, const CRegion& damage) {
    const auto original = reinterpret_cast<void (*)(Render::IElementRenderer*, WP<CTexPassElement>, const CRegion&)>(textureHook->m_original);
    auto& frame = g_pHyprRenderer->m_renderData;
    if (!element || !isRibbonZoom(frame)) {
        original(self, element, damage);
        return;
    }

    // Hyprland intersects surface damage with the unscaled texture box before
    // applying hints. For textures, move both the box and clips into output
    // coordinates before the native damage/blur path, avoiding a second scale.
    auto& data = element->m_data;
    const auto box = data.box;
    const auto clipBox = data.clipBox;
    const auto rounding = data.round;
    auto clipRegion = data.clipRegion.copy();
    auto originalDamage = std::move(data.damage);
    data.damage = {};
    const auto inheritedClip = frame.clipBox;
    auto restore = Render::CScopeGuard([&] {
        data.box = box;
        data.clipBox = clipBox;
        data.clipRegion = std::move(clipRegion);
        data.round = rounding;
        data.damage = std::move(originalDamage);
        frame.clipBox = inheritedClip;
        frame.renderModif.enabled = true;
    });
    frame.renderModif.applyToBox(data.box);
    if (!data.clipBox.empty()) frame.renderModif.applyToBox(data.clipBox);
    if (!frame.clipBox.empty()) frame.renderModif.applyToBox(frame.clipBox);
    if (!data.clipRegion.empty()) frame.renderModif.applyToRegion(data.clipRegion);
    data.round = std::lround(data.round * frame.renderModif.combinedScale());
    frame.renderModif.enabled = false;
    const auto clippedDamage = frame.damage.copy().intersect(data.box);
    original(self, element, clippedDamage);
}

void installTextureHook(HANDLE handle) {
    if (g_pHyprRenderer->type() != Render::IHyprRenderer::RT_GL) return;
    // Exact signature from the ABI-checked 0.56.2 build.
    constexpr auto symbol = "_ZN6Render16IElementRenderer7drawTexEN9Hyprutils6Memory12CWeakPointerI15CTexPassElementEERKNS1_4Math7CRegionE";
    const auto matches = HyprlandAPI::findFunctionsByName(handle, symbol);
    if (matches.size() != 1 || matches[0].signature != symbol) return;
    textureHook = HyprlandAPI::createFunctionHook(handle, matches[0].address, reinterpret_cast<void*>(&drawZoomedTexture));
    if (textureHook && !textureHook->hook()) {
        HyprlandAPI::removeFunctionHook(handle, textureHook);
        textureHook = nullptr;
    }
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
    zoomOut = registerOption(handle, "plugin:workspace_ribbon:zoom_out_percent", "Maximum zoom out percent", 8, 0, 25);
    pluginHandle = handle;
    installTextureHook(handle);
    listeners.push_back(Event::bus()->m_events.render.stage.listen(renderStage));
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
    if (zoomHintQueued) g_pHyprRenderer->m_renderPass.removeAllOfType(RibbonZoomHint::NAME);
    zoomHintQueued = false;
    if (textureHook) HyprlandAPI::removeFunctionHook(pluginHandle, textureHook);
    textureHook = nullptr;
    pluginHandle = nullptr;
    zoomOut.reset();
    enabled.reset(); duration.reset(); maximum.reset(); scaleDistance.reset(); easing.reset();
}
