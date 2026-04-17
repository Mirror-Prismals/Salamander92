#pragma once
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <thread>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(__linux__)
#include <unistd.h>
#endif
#include "Host/PlatformInput.h"
#include "Host/Vst3Host.h"
#include "../Render/WebGPUBackend.h"

namespace {
    std::string toLowerCopy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    GraphicsAPI parseGraphicsApi(const std::map<std::string, std::variant<bool, std::string>>& registry) {
        auto parseValue = [](const std::string& raw) -> GraphicsAPI {
            const std::string value = toLowerCopy(raw);
            if (value == "webgpu" || value == "wgpu") return GraphicsAPI::WebGPU;
            // Runtime backend selection is WebGPU-only.
            return GraphicsAPI::WebGPU;
        };

        auto it = registry.find("GraphicsAPI");
        if (it != registry.end() && std::holds_alternative<std::string>(it->second)) {
            return parseValue(std::get<std::string>(it->second));
        }
        it = registry.find("graphics_api");
        if (it != registry.end() && std::holds_alternative<std::string>(it->second)) {
            return parseValue(std::get<std::string>(it->second));
        }
        return GraphicsAPI::WebGPU;
    }

    std::unique_ptr<IRenderBackend> createRenderBackend(GraphicsAPI api) {
        (void)api;
        return std::make_unique<WebGPUBackend>();
    }

    bool consumeRegistryFlag(std::map<std::string, std::variant<bool, std::string>>& registry,
                             const std::string& key) {
        auto it = registry.find(key);
        if (it == registry.end()) return false;
        bool requested = false;
        if (std::holds_alternative<bool>(it->second)) {
            requested = std::get<bool>(it->second);
        } else if (std::holds_alternative<std::string>(it->second)) {
            const std::string raw = toLowerCopy(std::get<std::string>(it->second));
            requested = (raw == "1" || raw == "true" || raw == "yes" || raw == "on");
        }
        if (requested) {
            it->second = false;
        }
        return requested;
    }

    bool isShaderReloadShortcutDown(PlatformWindowHandle window) {
        if (!window) return false;
        const bool commandDown = PlatformInput::IsKeyDown(window, PlatformInput::Key::LeftSuper)
            || PlatformInput::IsKeyDown(window, PlatformInput::Key::RightSuper);
        const bool controlDown = PlatformInput::IsKeyDown(window, PlatformInput::Key::LeftControl)
            || PlatformInput::IsKeyDown(window, PlatformInput::Key::RightControl);
        const bool shiftDown = PlatformInput::IsKeyDown(window, PlatformInput::Key::LeftShift)
            || PlatformInput::IsKeyDown(window, PlatformInput::Key::RightShift);
        if ((!commandDown && !controlDown) || shiftDown) return false;
        return PlatformInput::IsKeyDown(window, PlatformInput::Key::R);
    }

    bool shouldRunInHeadlessPerf(const std::string& stepName) {
        static const std::unordered_set<std::string> kHeadlessUpdateAllowlist = {
            "UpdateExpanseTerrain",
            "UpdateExpanseTrees",
            "UpdateVoxelLighting",
            "UpdateVoxelMeshing",
            "UpdateVoxelMeshUpload",
            "UpdateVoxelMeshDebug",
            "UpdatePerf"
        };
        return kHeadlessUpdateAllowlist.count(stepName) > 0;
    }

    bool reloadActiveShaders(BaseSystem& baseSystem, std::string& outReport) {
        if (!baseSystem.renderer || !baseSystem.world) {
            outReport = "RendererContext or WorldContext unavailable.";
            return false;
        }

        std::string loadError;
        if (!HostLogic::ReloadShaderSources(baseSystem, &loadError)) {
            outReport = "Failed to reload shader files: " + loadError;
            return false;
        }

        RendererContext& renderer = *baseSystem.renderer;
        WorldContext& world = *baseSystem.world;
        struct ShaderBinding {
            const char* label = nullptr;
            std::unique_ptr<Shader>* shader = nullptr;
            const char* vertexKey = nullptr;
            const char* fragmentKey = nullptr;
        };
        const std::array<ShaderBinding, 17> bindings = {{
            {"blockShader", &renderer.blockShader, "BLOCK_VERTEX_SHADER", "BLOCK_FRAGMENT_SHADER"},
            {"faceShader", &renderer.faceShader, "FACE_VERTEX_SHADER", "FACE_FRAGMENT_SHADER"},
            {"skyboxShader", &renderer.skyboxShader, "SKYBOX_VERTEX_SHADER", "SKYBOX_FRAGMENT_SHADER"},
            {"sunMoonShader", &renderer.sunMoonShader, "SUNMOON_VERTEX_SHADER", "SUNMOON_FRAGMENT_SHADER"},
            {"starShader", &renderer.starShader, "STAR_VERTEX_SHADER", "STAR_FRAGMENT_SHADER"},
            {"selectionShader", &renderer.selectionShader, "SELECTION_VERTEX_SHADER", "SELECTION_FRAGMENT_SHADER"},
            {"hudShader", &renderer.hudShader, "HUD_VERTEX_SHADER", "HUD_FRAGMENT_SHADER"},
            {"crosshairShader", &renderer.crosshairShader, "CROSSHAIR_VERTEX_SHADER", "CROSSHAIR_FRAGMENT_SHADER"},
            {"colorEmotionShader", &renderer.colorEmotionShader, "COLOR_EMOTION_VERTEX_SHADER", "COLOR_EMOTION_FRAGMENT_SHADER"},
            {"uiShader", &renderer.uiShader, "UI_VERTEX_SHADER", "UI_FRAGMENT_SHADER"},
            {"uiColorShader", &renderer.uiColorShader, "UI_COLOR_VERTEX_SHADER", "UI_COLOR_FRAGMENT_SHADER"},
            {"glyphShader", &renderer.glyphShader, "GLYPH_VERTEX_SHADER", "GLYPH_FRAGMENT_SHADER"},
            {"fontShader", &renderer.fontShader, "FONT_VERTEX_SHADER", "FONT_FRAGMENT_SHADER"},
            {"audioRayShader", &renderer.audioRayShader, "AUDIORAY_VERTEX_SHADER", "AUDIORAY_FRAGMENT_SHADER"},
            {"audioRayVoxelShader", &renderer.audioRayVoxelShader, "AUDIORAY_VOXEL_VERTEX_SHADER", "AUDIORAY_VOXEL_FRAGMENT_SHADER"},
            {"godrayRadialShader", &renderer.godrayRadialShader, "GODRAY_VERTEX_SHADER", "GODRAY_RADIAL_FRAGMENT_SHADER"},
            {"godrayCompositeShader", &renderer.godrayCompositeShader, "GODRAY_VERTEX_SHADER", "GODRAY_COMPOSITE_FRAGMENT_SHADER"},
        }};

        int rebuilt = 0;
        int skipped = 0;
        int failed = 0;
        std::vector<std::string> failures;
        for (const ShaderBinding& binding : bindings) {
            if (!binding.shader || !(*binding.shader)) {
                ++skipped;
                continue;
            }
            const auto vertexIt = world.shaders.find(binding.vertexKey);
            const auto fragmentIt = world.shaders.find(binding.fragmentKey);
            if (vertexIt == world.shaders.end() || fragmentIt == world.shaders.end()) {
                ++failed;
                failures.push_back(std::string(binding.label) + ": missing shader source key(s)");
                continue;
            }
            std::string rebuildError;
            if (!(*binding.shader)->rebuild(vertexIt->second.c_str(), fragmentIt->second.c_str(), &rebuildError)) {
                ++failed;
                failures.push_back(std::string(binding.label) + ": " + rebuildError);
                continue;
            }
            ++rebuilt;
        }

        std::ostringstream summary;
        summary << "rebuilt=" << rebuilt << " skipped=" << skipped << " failed=" << failed;
        if (!failures.empty()) {
            summary << " [";
            for (size_t i = 0; i < failures.size(); ++i) {
                if (i > 0) summary << " | ";
                summary << failures[i];
            }
            summary << "]";
        }
        outReport = summary.str();
        return failed == 0;
    }
}

namespace HostPathingLogic {
    std::filesystem::path ResolveExecutableDirPath() {
#if defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        if (size > 0) {
            std::string buffer(static_cast<size_t>(size), '\0');
            if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
                return std::filesystem::path(buffer.c_str()).parent_path();
            }
        }
#elif defined(__linux__)
        char buffer[4096] = {0};
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0';
            return std::filesystem::path(buffer).parent_path();
        }
#endif
        return std::filesystem::current_path();
    }

    std::filesystem::path ResolvePackagedDataRootPath() {
        const std::filesystem::path execDir = ResolveExecutableDirPath();
        std::error_code ec;
        const std::filesystem::path bundled = execDir.parent_path() / "Resources" / "CardinalData";
        if (std::filesystem::exists(bundled, ec) && !ec) {
            return bundled;
        }
        return {};
    }

    void EnsureDataWorkingDirectory() {
        std::error_code ec;
        const std::filesystem::path bundledRoot = ResolvePackagedDataRootPath();
        if (!bundledRoot.empty()) {
            std::filesystem::current_path(bundledRoot, ec);
            if (ec) {
                std::cerr << "Host: failed to switch to bundled data root " << bundledRoot.string()
                          << " (" << ec.message() << ")" << std::endl;
            } else {
                std::cout << "Host: working directory = " << bundledRoot.string() << std::endl;
            }
            return;
        }
        std::cout << "Host: working directory = " << std::filesystem::current_path().string() << std::endl;
    }
}

void Host::run() { init(); mainLoop(); cleanup(); }

void Host::configureAutoStop(double seconds) {
    if (seconds > 0.0) {
        runOptions.autoStopEnabled = true;
        runOptions.autoStopSeconds = seconds;
    } else {
        runOptions.autoStopEnabled = false;
        runOptions.autoStopSeconds = 0.0;
    }
}

void Host::configureHeadlessPerf(bool enabled) {
    runOptions.headlessPerf = enabled;
}

void Host::registerSystemFunctions() {
    functionRegistry["LoadProcedureAssets"] = HostLogic::LoadProcedureAssets;
    functionRegistry["SetPlayerSpawn"] = SpawnSystemLogic::SetPlayerSpawn;
    functionRegistry["InitializeAudio"] = AudioSystemLogic::InitializeAudio;
    functionRegistry["InitializeVst3"] = Vst3SystemLogic::InitializeVst3;
    functionRegistry["CleanupAudio"] = AudioSystemLogic::CleanupAudio;
    functionRegistry["CleanupVst3"] = Vst3SystemLogic::CleanupVst3;
    functionRegistry["UpdateSoundtracks"] = SoundtrackSystemLogic::UpdateSoundtracks;
    functionRegistry["ProcessRayTracedAudio"] = RayTracedAudioSystemLogic::ProcessRayTracedAudio;
    functionRegistry["ProcessPinkNoiseAudicle"] = PinkNoiseSystemLogic::ProcessPinkNoiseAudicle;
    functionRegistry["ProcessAudicles"] = AudicleSystemLogic::ProcessAudicles;
    functionRegistry["UpdateAudioVisualizerFollower"] = AudioVisualizerFollowerSystemLogic::UpdateAudioVisualizerFollower;
    functionRegistry["ResolveCollisions"] = CollisionSystemLogic::ResolveCollisions;
    functionRegistry["ProcessWalkMovement"] = WalkModeSystemLogic::ProcessWalkMovement;
    functionRegistry["ApplyGravity"] = GravitySystemLogic::ApplyGravity;
    functionRegistry["UpdateCameraMatrices"] = CameraSystemLogic::UpdateCameraMatrices;
    functionRegistry["ProcessKeyboardInput"] = KeyboardInputSystemLogic::ProcessKeyboardInput;
    functionRegistry["UpdateBookSystem"] = BookSystemLogic::UpdateBookSystem;
    functionRegistry["ProcessVolumeFills"] = VolumeFillSystemLogic::ProcessVolumeFills;
    functionRegistry["UpdateCameraRotationFromMouse"] = MouseInputSystemLogic::UpdateCameraRotationFromMouse;
    functionRegistry["ProcessUAVMovement"] = UAVSystemLogic::ProcessUAVMovement;
    functionRegistry["InitializeRenderer"] = RenderInitSystemLogic::InitializeRenderer;
    functionRegistry["CleanupRenderer"] = RenderInitSystemLogic::CleanupRenderer;
    functionRegistry["UpdateVoxelMeshInit"] = VoxelMeshInitSystemLogic::UpdateVoxelMeshInit;
    functionRegistry["UpdateVoxelLighting"] = VoxelLightingSystemLogic::UpdateVoxelLighting;
    functionRegistry["UpdateVoxelMeshing"] = VoxelMeshingSystemLogic::UpdateVoxelMeshing;
    functionRegistry["UpdateVoxelMeshUpload"] = VoxelMeshUploadSystemLogic::UpdateVoxelMeshUpload;
    functionRegistry["UpdateVoxelMeshDebug"] = VoxelMeshDebugSystemLogic::UpdateVoxelMeshDebug;
    functionRegistry["RenderWorld"] = WorldRenderSystemLogic::RenderWorld;
    functionRegistry["RenderOverlays"] = OverlayRenderSystemLogic::RenderOverlays;
    functionRegistry["GenerateTerrain"] = TerrainSystemLogic::GenerateTerrain;
    functionRegistry["UpdateExpanseTerrain"] = TerrainSystemLogic::UpdateExpanseTerrain;
    functionRegistry["UpdateDepthsCrystals"] = DepthsCrystalSystemLogic::UpdateDepthsCrystals;
    functionRegistry["LoadExpanseConfig"] = ExpanseBiomeSystemLogic::LoadExpanseConfig;
    functionRegistry["LoadLeyLines"] = LeyLineSystemLogic::LoadLeyLines;
    functionRegistry["UpdateExpanseTrees"] = TreeGenerationSystemLogic::UpdateExpanseTrees;
    functionRegistry["UpdateTreeSectionScheduler"] = TreeSectionSchedulerSystemLogic::UpdateTreeSectionScheduler;
    functionRegistry["UpdateTreeCanopyGeneration"] = TreeGenerationSystemLogic::UpdateTreeCanopyGeneration;
    functionRegistry["UpdateSurfaceFoliage"] = TreeGenerationSystemLogic::UpdateSurfaceFoliage;
    functionRegistry["UpdateWaterFoliage"] = TreeGenerationSystemLogic::UpdateWaterFoliage;
    functionRegistry["UpdateCaveDecor"] = TreeGenerationSystemLogic::UpdateCaveDecor;
    functionRegistry["ProcessStructurePlacement"] = StructurePlacementSystemLogic::ProcessStructurePlacement;
    functionRegistry["ProcessStructureCapture"] = StructureCaptureSystemLogic::ProcessStructureCapture;
    functionRegistry["UpdateBlockSelection"] = BlockSelectionSystemLogic::UpdateBlockSelection;
    functionRegistry["UpdateBlockCharge"] = BlockChargeSystemLogic::UpdateBlockCharge;
    functionRegistry["UpdateGroundCrafting"] = GroundCraftingSystemLogic::UpdateGroundCrafting;
    functionRegistry["UpdateOreMining"] = OreMiningSystemLogic::UpdateOreMining;
    functionRegistry["UpdateGemChisel"] = GemChiselSystemLogic::UpdateGemChisel;
    functionRegistry["UpdateGems"] = GemSystemLogic::UpdateGems;
    functionRegistry["UpdateFishing"] = FishingSystemLogic::UpdateFishing;
    functionRegistry["UpdateHUD"] = HUDSystemLogic::UpdateHUD;
    functionRegistry["UpdateColorEmotions"] = ColorEmotionSystemLogic::UpdateColorEmotions;
    functionRegistry["UpdateBuildMode"] = BuildSystemLogic::UpdateBuildMode;
    functionRegistry["UpdateUIScreen"] = UIScreenSystemLogic::UpdateUIScreen;
    functionRegistry["UpdateSecurityCamera"] = SecurityCameraSystemLogic::UpdateSecurityCamera;
    functionRegistry["UpdateDimension"] = DimensionSystemLogic::UpdateDimension;
    functionRegistry["UpdateGlyphs"] = GlyphSystemLogic::UpdateGlyphs;
    functionRegistry["UpdateDecibelMeters"] = DecibelMeterSystemLogic::UpdateDecibelMeters;
    functionRegistry["UpdateDawFaders"] = DawFaderSystemLogic::UpdateDawFaders;
    functionRegistry["UpdateDebugHud"] = DebugHudSystemLogic::UpdateDebugHud;
    functionRegistry["UpdateDebugWireframe"] = DebugWireframeSystemLogic::UpdateDebugWireframe;
    functionRegistry["UpdateFonts"] = FontSystemLogic::UpdateFonts;
    functionRegistry["RenderFontsTimeline"] = FontSystemLogic::RenderFontsTimeline;
    functionRegistry["RenderFontsSideButtons"] = FontSystemLogic::RenderFontsSideButtons;
    functionRegistry["RenderFontsTopButtons"] = FontSystemLogic::RenderFontsTopButtons;
    functionRegistry["01_RenderFontsTimeline"] = PanelRenderSystemLogic::Step01_RenderFontsTimeline;
    functionRegistry["04_RenderFontsSideButtons"] = PanelRenderSystemLogic::Step04_RenderFontsSideButtons;
    functionRegistry["07_RenderFontsTopButtons"] = PanelRenderSystemLogic::Step07_RenderFontsTopButtons;
    functionRegistry["CleanupFonts"] = PanelRenderSystemLogic::CleanupFonts;
    functionRegistry["UpdateComputerCursor"] = ComputerCursorSystemLogic::UpdateComputerCursor;
    functionRegistry["UpdateButtons"] = ButtonSystemLogic::UpdateButtons;
    functionRegistry["UpdateButtonInput"] = ButtonSystemLogic::UpdateButtonInput;
    functionRegistry["RenderButtonsSide"] = ButtonSystemLogic::RenderButtonsSide;
    functionRegistry["RenderButtonsTopBottom"] = ButtonSystemLogic::RenderButtonsTopBottom;
    functionRegistry["03_RenderButtonsSide"] = PanelRenderSystemLogic::Step03_RenderButtonsSide;
    functionRegistry["06_RenderButtonsTopBottom"] = PanelRenderSystemLogic::Step06_RenderButtonsTopBottom;
    functionRegistry["UpdatePanels"] = PanelSystemLogic::UpdatePanels;
    functionRegistry["RenderPanels"] = PanelSystemLogic::RenderPanels;
    functionRegistry["RenderSidePanels"] = PanelSystemLogic::RenderSidePanels;
    functionRegistry["RenderTopBottomPanels"] = PanelSystemLogic::RenderTopBottomPanels;
    functionRegistry["02_RenderSidePanels"] = PanelRenderSystemLogic::Step02_RenderSidePanels;
    functionRegistry["05_RenderTopBottomPanels"] = PanelRenderSystemLogic::Step05_RenderTopBottomPanels;
    functionRegistry["RenderDecibelMeters"] = DecibelMeterSystemLogic::RenderDecibelMeters;
    functionRegistry["RenderDawFaders"] = DawFaderSystemLogic::RenderDawFaders;
    functionRegistry["05a_RenderDecibelMeters"] = PanelRenderSystemLogic::Step05a_RenderDecibelMeters;
    functionRegistry["05b_RenderDawFaders"] = PanelRenderSystemLogic::Step05b_RenderDawFaders;
    functionRegistry["UpdateUIStamping"] = UIStampingSystemLogic::UpdateUIStamping;
    functionRegistry["UpdatePianoRollResources"] = PianoRollResourceSystemLogic::UpdatePianoRollResources;
    functionRegistry["UpdatePianoRollLayout"] = PianoRollLayoutSystemLogic::UpdatePianoRollLayout;
    functionRegistry["UpdatePianoRollInput"] = PianoRollInputSystemLogic::UpdatePianoRollInput;
    functionRegistry["UpdatePianoRollRender"] = PianoRollRenderSystemLogic::UpdatePianoRollRender;
    functionRegistry["UpdateChucK"] = ChucKSystemLogic::UpdateChucK;
    functionRegistry["UpdateVst3"] = Vst3SystemLogic::UpdateVst3;
    functionRegistry["UpdateVst3Browser"] = Vst3BrowserSystemLogic::UpdateVst3Browser;
    functionRegistry["UpdateAudioRayVisualizer"] = AudioRayVisualizerSystemLogic::UpdateAudioRayVisualizer;
    functionRegistry["UpdateSoundPhysics"] = SoundPhysicsSystemLogic::UpdateSoundPhysics;
    functionRegistry["UpdateRegistry"] = RegistryEditorSystemLogic::UpdateRegistry;
    functionRegistry["UpdateMirrors"] = MirrorSystemLogic::UpdateMirrors;
    functionRegistry["UpdateDawTracks"] = DawTrackSystemLogic::UpdateDawTracks;
    functionRegistry["CleanupDawTracks"] = DawTrackSystemLogic::CleanupDawTracks;
    functionRegistry["UpdateDawTransport"] = DawTransportSystemLogic::UpdateDawTransport;
    functionRegistry["UpdateDawClips"] = DawClipSystemLogic::UpdateDawClips;
    functionRegistry["UpdateDawWaveforms"] = DawWaveformSystemLogic::UpdateDawWaveforms;
    functionRegistry["UpdateDawUi"] = DawUiSystemLogic::UpdateDawUi;
    functionRegistry["UpdateDawIO"] = DawIOSystemLogic::UpdateDawIO;
    functionRegistry["UpdateDawLaneTimeline"] = DawLaneTimelineSystemLogic::UpdateDawLaneTimeline;
    functionRegistry["UpdateDawLaneModal"] = DawLaneInputSystemLogic::UpdateDawLaneModal;
    functionRegistry["UpdateDawLaneShortcuts"] = DawLaneInputSystemLogic::UpdateDawLaneShortcuts;
    functionRegistry["UpdateDawLaneGestures"] = DawLaneInputSystemLogic::UpdateDawLaneGestures;
    functionRegistry["UpdateDawLaneInput"] = DawLaneInputSystemLogic::UpdateDawLaneInput;
    functionRegistry["UpdateDawLaneResources"] = DawLaneResourceSystemLogic::UpdateDawLaneResources;
    functionRegistry["UpdateDawLaneRender"] = DawLaneRenderSystemLogic::UpdateDawLaneRender;
    functionRegistry["UpdateMidiTracks"] = MidiTrackSystemLogic::UpdateMidiTracks;
    functionRegistry["CleanupMidiTracks"] = MidiTrackSystemLogic::CleanupMidiTracks;
    functionRegistry["UpdateAutomationTracks"] = AutomationTrackSystemLogic::UpdateAutomationTracks;
    functionRegistry["UpdateMidiTransport"] = MidiTransportSystemLogic::UpdateMidiTransport;
    functionRegistry["UpdateMidiWaveforms"] = MidiWaveformSystemLogic::UpdateMidiWaveforms;
    functionRegistry["UpdateMidiUi"] = MidiUiSystemLogic::UpdateMidiUi;
    functionRegistry["UpdateMidiStamping"] = MidiStampingSystemLogic::UpdateMidiStamping;
    functionRegistry["UpdateMidiIO"] = MidiIOSystemLogic::UpdateMidiIO;
    functionRegistry["UpdateMidiLane"] = MidiLaneSystemLogic::UpdateMidiLane;
    functionRegistry["UpdateAutomationLane"] = AutomationLaneSystemLogic::UpdateAutomationLane;
    functionRegistry["UpdateMicrophoneBlocks"] = MicrophoneBlockSystemLogic::UpdateMicrophoneBlocks;
    functionRegistry["UpdatePerf"] = PerfSystemLogic::UpdatePerf;
    functionRegistry["UpdateBootSequence"] = BootSequenceSystemLogic::UpdateBootSequence;
    functionRegistry["LoadBlockTextures"] = BlockTextureSystemLogic::LoadBlockTextures;
}

void Host::init() {
    HostPathingLogic::EnsureDataWorkingDirectory();
    loadRegistry();
    if (!std::get<bool>(registry["Program"])) { std::cerr << "FATAL: Program not installed. Halting." << std::endl; return; }

    baseSystem.level = std::make_unique<LevelContext>();
    baseSystem.app = std::make_unique<AppContext>();
    baseSystem.world = std::make_unique<WorldContext>();
    baseSystem.voxelWorld = std::make_unique<VoxelWorldContext>();
    baseSystem.voxelRender = std::make_unique<VoxelRenderContext>();
    baseSystem.player = std::make_unique<PlayerContext>();
    baseSystem.instance = std::make_unique<InstanceContext>();
    baseSystem.renderer = std::make_unique<RendererContext>();
    baseSystem.audio = std::make_unique<AudioContext>();
    baseSystem.vst3 = std::make_unique<Vst3Context>();
    baseSystem.rayTracedAudio = std::make_unique<RayTracedAudioContext>();
    baseSystem.hud = std::make_unique<HUDContext>();
    baseSystem.colorEmotion = std::make_unique<ColorEmotionContext>();
    baseSystem.fishing = std::make_unique<FishingContext>();
    baseSystem.gems = std::make_unique<GemContext>();
    baseSystem.ui = std::make_unique<UIContext>();
    baseSystem.securityCamera = std::make_unique<SecurityCameraContext>();
    baseSystem.uiStamp = std::make_unique<UIStampingContext>();
    baseSystem.panel = std::make_unique<PanelContext>();
    baseSystem.decibelMeter = std::make_unique<DecibelMeterContext>();
    baseSystem.fader = std::make_unique<DawFaderContext>();
    baseSystem.mirror = std::make_unique<MirrorContext>();
    baseSystem.font = std::make_unique<FontContext>();
    baseSystem.daw = std::make_unique<DawContext>();
    baseSystem.midi = std::make_unique<MidiContext>();
    baseSystem.perf = std::make_unique<PerfContext>();
    baseSystem.registry = &registry;
    baseSystem.reloadRequested = &reloadRequested;
    baseSystem.reloadTarget = &reloadTarget;
    auto readRegistryInt = [&](const char* key, int fallback) {
        auto it = registry.find(key);
        if (it == registry.end() || !std::holds_alternative<std::string>(it->second)) return fallback;
        try { return std::stoi(std::get<std::string>(it->second)); } catch (...) { return fallback; }
    };
    auto readRegistryBool = [&](const char* key, bool fallback) {
        auto it = registry.find(key);
        if (it == registry.end() || !std::holds_alternative<bool>(it->second)) return fallback;
        return std::get<bool>(it->second);
    };
    auto readRegistryString = [&](const char* key, const char* fallback) {
        auto it = registry.find(key);
        if (it == registry.end() || !std::holds_alternative<std::string>(it->second)) {
            return std::string(fallback ? fallback : "");
        }
        return std::get<std::string>(it->second);
    };
    auto normalizeVoxelSectionSize = [](int requested, int fallback) {
        // Voxel section code assumes a power-of-two section size.
        auto isPow2 = [](int v) { return v > 0 && (v & (v - 1)) == 0; };
        if (requested < 16) requested = 16;
        if (!isPow2(requested)) return fallback;
        return requested;
    };
    if (baseSystem.voxelWorld) {
        int fallbackSize = baseSystem.voxelWorld->sectionSize;
        int sectionSize = normalizeVoxelSectionSize(readRegistryInt("voxelSectionSize", fallbackSize), fallbackSize);
        baseSystem.voxelWorld->sectionSize = sectionSize;
    }

    if (baseSystem.level && registry.count("spawn") && std::holds_alternative<std::string>(registry["spawn"])) {
        baseSystem.level->spawnKey = std::get<std::string>(registry["spawn"]);
    }
    if (registry.count("gamemode") && std::holds_alternative<std::string>(registry["gamemode"])) {
        baseSystem.gamemode = std::get<std::string>(registry["gamemode"]);
    }
    registry["spawn_ready"] = false;

    registerSystemFunctions();
    loadSystems();
    HostLogic::LoadProcedureAssets(baseSystem, entityPrototypes, 0.0f, nullptr);

    if (runOptions.headlessPerf) {
        std::cout << "[HeadlessPerf] running without renderer/window initialization." << std::endl;
        renderBackend.reset();
        window = nullptr;
        baseSystem.renderBackend = nullptr;
        Shader::SetRenderBackend(nullptr);
    } else {
        graphicsAPI = parseGraphicsApi(registry);
        renderBackend = createRenderBackend(graphicsAPI);
        if (!renderBackend) {
            std::cerr << "Failed to create render backend." << std::endl;
            exit(-1);
        }
        std::string presentPreference = readRegistryString("WebGPUPresentMode", "auto");
        if (const char* envPresent = std::getenv("SALAMANDER_WEBGPU_PRESENT_MODE")) {
            if (*envPresent) {
                presentPreference = envPresent;
            }
        }
        if (presentPreference == "auto" || presentPreference == "Auto" || presentPreference.empty()) {
            const bool vsyncEnabled = readRegistryBool("VSyncEnabled", true);
            presentPreference = vsyncEnabled ? "vsync-on" : "vsync-off";
        }
        renderBackend->setPresentationPreference(presentPreference);
        if (!renderBackend->initialize(static_cast<int>(baseSystem.app->windowWidth),
                                       static_cast<int>(baseSystem.app->windowHeight),
                                       "Cardinal EDS",
                                       window)) {
            std::cerr << "Failed to initialize render backend." << std::endl;
            exit(-1);
        }
        std::cout << "[Graphics] Using backend: " << renderBackend->name() << std::endl;
        baseSystem.renderBackend = renderBackend.get();
        Shader::SetRenderBackend(renderBackend.get());

        PlatformInput::SetCursorMode(window, PlatformInput::CursorMode::Disabled);
        PlatformInput::SetWindowUserPointer(window, this);
        PlatformInput::SetFramebufferSizeCallback(window, [](PlatformWindowHandle w, int width, int height){
            Host* host = static_cast<Host*>(PlatformInput::GetWindowUserPointer(w));
            if (host && host->renderBackend) host->renderBackend->onFramebufferResize(width, height);
        });
        PlatformInput::SetCursorPositionCallback(window, [](PlatformWindowHandle w, double x, double y){
            static_cast<Host*>(PlatformInput::GetWindowUserPointer(w))->processMouseInput(x, y);
        });
        PlatformInput::SetScrollCallback(window, [](PlatformWindowHandle w, double xoff, double yoff){
            static_cast<Host*>(PlatformInput::GetWindowUserPointer(w))->processScroll(xoff, yoff);
        });
    }

    HostPathingLogic::EnsureDataWorkingDirectory();
    PopulateWorldsFromLevel();

    // Load atlas + texture mappings before any init step that may generate terrain/chunks.
    // systems_order currently runs GenerateTerrain before LoadBlockTextures, which can
    // produce startup chunks with fallback flat colors until a later reload.
    if (baseSystem.renderBackend) {
        BlockTextureSystemLogic::LoadBlockTextures(baseSystem, entityPrototypes, 0.0f, window);
    }

    for(const auto& step : initFunctions) {
        if (runOptions.headlessPerf) {
            if (step.name == "InitializeRenderer" ||
                step.name == "InitializeAudio" ||
                step.name == "InitializeVst3") {
                continue;
            }
        }
        if (step.name == "InitializeRenderer" && rendererInitialized) continue;
        if (step.name == "InitializeAudio" && audioInitialized) continue;
        if(functionRegistry.count(step.name)) {
            if (checkDependencies(step.dependencies)) {
                functionRegistry[step.name](baseSystem, entityPrototypes, 0.0f, window);
                if (step.name == "InitializeRenderer") rendererInitialized = true;
                if (step.name == "InitializeAudio") audioInitialized = true;
            }
        }
    }
}

void Host::runCleanupSteps() {
    std::vector<SystemStep> reversedCleanup = cleanupFunctions;
    std::reverse(reversedCleanup.begin(), reversedCleanup.end());
    for(const auto& step : reversedCleanup) {
        if(functionRegistry.count(step.name) && checkDependencies(step.dependencies)) {
            functionRegistry[step.name](baseSystem, entityPrototypes, 0.0f, window);
        }
    }
}

void Host::reloadLevel(const std::string& levelName) {
    // In-place level reload: keep contexts alive, just reload worlds and reset caches/state.
    if (!baseSystem.level) baseSystem.level = std::make_unique<LevelContext>();
    baseSystem.level->worlds.clear();
    baseSystem.level->activeWorldIndex = 0;
    if (registry.count("spawn") && std::holds_alternative<std::string>(registry["spawn"])) {
        baseSystem.level->spawnKey = std::get<std::string>(registry["spawn"]);
    }
    registry["spawn_ready"] = false;

    // Reset per-level caches/contexts.
    if (baseSystem.voxelWorld) baseSystem.voxelWorld = std::make_unique<VoxelWorldContext>();
    if (baseSystem.voxelRender) baseSystem.voxelRender = std::make_unique<VoxelRenderContext>();
    if (baseSystem.voxelWorld) {
        auto readRegistryInt = [&](const char* key, int fallback) {
            auto it = registry.find(key);
            if (it == registry.end() || !std::holds_alternative<std::string>(it->second)) return fallback;
            try { return std::stoi(std::get<std::string>(it->second)); } catch (...) { return fallback; }
        };
        auto normalizeVoxelSectionSize = [](int requested, int fallback) {
            auto isPow2 = [](int v) { return v > 0 && (v & (v - 1)) == 0; };
            if (requested < 16) requested = 16;
            if (!isPow2(requested)) return fallback;
            return requested;
        };
        int fallbackSize = baseSystem.voxelWorld->sectionSize;
        int sectionSize = normalizeVoxelSectionSize(readRegistryInt("voxelSectionSize", fallbackSize), fallbackSize);
        baseSystem.voxelWorld->sectionSize = sectionSize;
    }
    if (baseSystem.instance) baseSystem.instance = std::make_unique<InstanceContext>();
    if (baseSystem.hud) baseSystem.hud = std::make_unique<HUDContext>();
    if (baseSystem.colorEmotion) baseSystem.colorEmotion = std::make_unique<ColorEmotionContext>();
    if (baseSystem.fishing) baseSystem.fishing = std::make_unique<FishingContext>();
    if (baseSystem.gems) baseSystem.gems = std::make_unique<GemContext>();
    if (baseSystem.font) baseSystem.font = std::make_unique<FontContext>();
    if (baseSystem.uiStamp) baseSystem.uiStamp = std::make_unique<UIStampingContext>();
    if (baseSystem.panel) baseSystem.panel = std::make_unique<PanelContext>();
    if (baseSystem.rayTracedAudio) {
        baseSystem.rayTracedAudio->sourceCacheBuilt = false;
        baseSystem.rayTracedAudio->sourceInstances.clear();
        baseSystem.rayTracedAudio->sourceStates.clear();
        baseSystem.rayTracedAudio->debugSegments.clear();
        baseSystem.rayTracedAudio->lastDebugTime = -1.0;
        baseSystem.rayTracedAudio->batch = RayTraceBatch{};
        baseSystem.rayTracedAudio->lastBatchCompleteTime = -1.0;
    }
    // Keep UIContext but clear runtime flags.
    if (baseSystem.ui) {
        baseSystem.ui->active = false;
        baseSystem.ui->fullscreenActive = false;
        baseSystem.ui->loadingActive = false;
        baseSystem.ui->levelSwitchPending = false;
        baseSystem.ui->bootLoadingStarted = false;
        baseSystem.ui->cursorReleased = false;
        baseSystem.ui->uiLeftDown = baseSystem.ui->uiLeftPressed = baseSystem.ui->uiLeftReleased = false;
        baseSystem.ui->computerCacheBuilt = false;
        baseSystem.ui->computerInstances.clear();
        baseSystem.ui->mainScrollDelta = 0.0;
        baseSystem.ui->panelScrollDelta = 0.0;
    }
    if (baseSystem.securityCamera) {
        baseSystem.securityCamera->cacheBuilt = false;
        baseSystem.securityCamera->cameraInstances.clear();
        baseSystem.securityCamera->dawViewActive = false;
        baseSystem.securityCamera->activeWorldIndex = -1;
        baseSystem.securityCamera->activeInstanceID = -1;
        baseSystem.securityCamera->viewPosition = glm::vec3(0.0f);
        baseSystem.securityCamera->viewForward = glm::vec3(0.0f, 0.0f, -1.0f);
        baseSystem.securityCamera->viewYaw = -90.0f;
        baseSystem.securityCamera->viewPitch = 0.0f;
        baseSystem.securityCamera->wasUiActive = false;
        baseSystem.securityCamera->cycleKeyDownLast = false;
        baseSystem.securityCamera->orientationByInstance.clear();
    }
    if (baseSystem.mirror) {
        baseSystem.mirror->mirrors.clear();
        baseSystem.mirror->activeMirrorIndex = -1;
        baseSystem.mirror->activeDeviceInstanceID = -1;
        baseSystem.mirror->uiOffset = glm::vec2(0.0f);
        baseSystem.mirror->uiScale = 1.0f;
        baseSystem.mirror->expandedMirrorIndex = -1;
        baseSystem.mirror->expanded = false;
        baseSystem.mirror->expandedWorldIndices.clear();
        baseSystem.mirror->deviceMirrorIndex.clear();
    }
    if (baseSystem.daw) {
        baseSystem.daw->uiCacheBuilt = false;
    }

    // Rebuild world prototypes and instances for the new level.
    HostPathingLogic::EnsureDataWorkingDirectory();
    baseSystem.world = std::make_unique<WorldContext>();
    entityPrototypes.clear();
    initFunctions.clear();
    updateFunctions.clear();
    cleanupFunctions.clear();
    loadSystems();
    HostLogic::LoadProcedureAssets(baseSystem, entityPrototypes, 0.0f, nullptr);
    // Override registry level if an explicit target is provided.
    if (!levelName.empty()) {
        registry["level"] = levelName;
    }
    if (registry.count("level") && std::holds_alternative<std::string>(registry["level"])) {
        std::cout << "Host: active level after reload = " << std::get<std::string>(registry["level"]) << std::endl;
    }
    // Ensure level-specific terrain config is loaded immediately for the new level key.
    ExpanseBiomeSystemLogic::LoadExpanseConfig(baseSystem, entityPrototypes, 0.0f, window);
    HostPathingLogic::EnsureDataWorkingDirectory();
    PopulateWorldsFromLevel();

    // Force a texture atlas reload after level swap so chunkable voxel prototypes
    // always resolve to the active atlas map/texture for the new level.
    BlockTextureSystemLogic::LoadBlockTextures(baseSystem, entityPrototypes, 0.0f, window);

    for(const auto& step : initFunctions) {
        if (step.name == "InitializeRenderer" && rendererInitialized) continue;
        if (step.name == "InitializeAudio" && audioInitialized) continue;
        if(functionRegistry.count(step.name)) {
            if (checkDependencies(step.dependencies)) {
                functionRegistry[step.name](baseSystem, entityPrototypes, 0.0f, window);
                if (step.name == "InitializeRenderer") rendererInitialized = true;
                if (step.name == "InitializeAudio") audioInitialized = true;
            }
        }
    }

    // Recapture cursor after reload if UI is not active.
    if (window && baseSystem.ui && !baseSystem.ui->active) {
        PlatformInput::SetCursorMode(window, PlatformInput::CursorMode::Disabled);
        baseSystem.ui->cursorReleased = false;
    }
}

namespace {
    glm::vec2 readVec2(const json& value, const glm::vec2& fallback) {
        if (!value.is_array() || value.size() < 2) return fallback;
        return glm::vec2(value[0].get<float>(), value[1].get<float>());
    }

    glm::vec3 readVec3(const json& value, const glm::vec3& fallback) {
        if (!value.is_array() || value.size() < 3) return fallback;
        return glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
    }

    MirrorDefinition parseMirrorDefinition(const json& data, const std::string& fallbackName) {
        MirrorDefinition mirror;
        mirror.name = fallbackName;

        if (data.contains("name")) {
            mirror.name = data["name"].get<std::string>();
        }
        if (data.contains("uiTransform") && data["uiTransform"].is_object()) {
            const auto& t = data["uiTransform"];
            if (t.contains("scale")) {
                mirror.uiScale = t["scale"].get<float>();
            }
            if (t.contains("offset")) {
                mirror.uiOffset = readVec2(t["offset"], mirror.uiOffset);
            }
        }

        if (data.contains("worldInstances") && data["worldInstances"].is_array()) {
            for (const auto& instData : data["worldInstances"]) {
                if (!instData.is_object() || !instData.contains("world")) continue;
                MirrorWorldInstance inst;
                inst.worldName = instData["world"].get<std::string>();

                if (instData.contains("repeat") && instData["repeat"].is_object()) {
                    const auto& repeat = instData["repeat"];
                    if (repeat.contains("count")) {
                        inst.repeatCount = std::max(1, repeat["count"].get<int>());
                    }
                    if (repeat.contains("offset")) {
                        inst.repeatOffset = readVec3(repeat["offset"], inst.repeatOffset);
                    }
                }

                if (instData.contains("overrides") && instData["overrides"].is_array()) {
                    for (const auto& ovData : instData["overrides"]) {
                        if (!ovData.is_object()) continue;
                        MirrorOverride ov;
                        if (ovData.contains("match") && ovData["match"].is_object()) {
                            const auto& match = ovData["match"];
                            if (match.contains("controlId")) ov.matchControlId = match["controlId"].get<std::string>();
                            if (match.contains("controlRole")) ov.matchControlRole = match["controlRole"].get<std::string>();
                            if (match.contains("name")) ov.matchName = match["name"].get<std::string>();
                        }
                        if (ovData.contains("set")) {
                            ov.set = ovData["set"];
                        }
                        inst.overrides.push_back(std::move(ov));
                    }
                }
                if (instData.contains("rowOverrides") && instData["rowOverrides"].is_array()) {
                    for (const auto& rowData : instData["rowOverrides"]) {
                        if (!rowData.is_object()) continue;
                        MirrorRowOverride rowOv;
                        if (rowData.contains("row")) rowOv.row = rowData["row"].get<int>();
                        else if (rowData.contains("track")) rowOv.row = rowData["track"].get<int>();
                        if (rowOv.row < 0) continue;
                        if (rowData.contains("match") && rowData["match"].is_object()) {
                            const auto& match = rowData["match"];
                            if (match.contains("controlId")) rowOv.matchControlId = match["controlId"].get<std::string>();
                            if (match.contains("controlRole")) rowOv.matchControlRole = match["controlRole"].get<std::string>();
                            if (match.contains("name")) rowOv.matchName = match["name"].get<std::string>();
                        }
                        if (rowData.contains("set")) {
                            rowOv.set = rowData["set"];
                        }
                        inst.rowOverrides.push_back(std::move(rowOv));
                    }
                }
                mirror.worldInstances.push_back(std::move(inst));
            }
        }

        return mirror;
    }
}

void Host::PopulateWorldsFromLevel() {
    const std::vector<std::string> entityFiles = {
        "Entities/Block.json", "Entities/Leaf.json", "Entities/Branch.json", "Entities/TexturedBlock.json", "Entities/Star.json", "Entities/Water.json",
        "Entities/World.json", "Entities/DebugWorldGenerator.json",
        "Entities/Audicles/UAV.json", "Entities/AudioVisualizer.json", "Entities/Microphone.json", "Entities/Computer.json", "Entities/SecurityCamera.json",
        "Entities/ScaffoldBlock.json",
        "Entities/Faces.json",
        "Entities/Foliage.json"
    };
    auto loadEntityFile = [&](const std::string& filePath) {
        std::ifstream f(filePath);
        if (!f.is_open()) {
            std::cerr << "Warning: Could not open entity file " << filePath << std::endl;
            return;
        }
        json data = json::parse(f);
        if (data.is_array()) {
            for (const auto& item : data) {
                Entity newProto = item.get<Entity>();
                newProto.prototypeID = entityPrototypes.size();
                entityPrototypes.push_back(newProto);
            }
        } else {
            Entity newProto = data.get<Entity>();
            newProto.prototypeID = entityPrototypes.size();
            entityPrototypes.push_back(newProto);
        }
    };
    auto loadEntityDirectory = [&](const std::string& dirPath) {
        std::error_code ec;
        if (!std::filesystem::exists(dirPath, ec)) return;
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            if (path.extension() == ".json") files.push_back(path);
        }
        std::sort(files.begin(), files.end());
        for (const auto& path : files) {
            loadEntityFile(path.string());
        }
    };
    for (const auto& filePath : entityFiles) {
        loadEntityFile(filePath);
    }
    loadEntityDirectory("Entities/UI");

    std::string levelName = std::get<std::string>(registry["level"]);
    std::string levelPath = "Levels/" + levelName + "_level.json";
    std::ifstream levelFile(levelPath);
    if (!levelFile.is_open()) { std::cerr << "FATAL: Could not open level file " << levelPath << std::endl; exit(-1); }
    json levelData = json::parse(levelFile);

    if (baseSystem.mirror) {
        baseSystem.mirror->mirrors.clear();
        baseSystem.mirror->activeMirrorIndex = -1;
        baseSystem.mirror->activeDeviceInstanceID = -1;
        baseSystem.mirror->uiOffset = glm::vec2(0.0f);
        baseSystem.mirror->uiScale = 1.0f;
        baseSystem.mirror->expandedMirrorIndex = -1;
        baseSystem.mirror->expanded = false;
        baseSystem.mirror->expandedWorldIndices.clear();
        if (levelData.contains("mirrors") && levelData["mirrors"].is_array()) {
            for (const auto& mirrorFilename : levelData["mirrors"]) {
                if (!mirrorFilename.is_string()) continue;
                std::string mirrorPath = "Mirrors/" + mirrorFilename.get<std::string>();
                std::ifstream mirrorFile(mirrorPath);
                if (!mirrorFile.is_open()) {
                    std::cerr << "Warning: Could not open mirror file " << mirrorPath << std::endl;
                    continue;
                }
                json mirrorData = json::parse(mirrorFile);
                MirrorDefinition mirror = parseMirrorDefinition(mirrorData, mirrorFilename.get<std::string>());
                baseSystem.mirror->mirrors.push_back(std::move(mirror));
            }
        }
    }

    for (const auto& worldFilename : levelData["worlds"]) {
        std::string path_str = worldFilename.get<std::string>();
        std::vector<std::string> searchPaths = {
            "Entities/Worlds/" + path_str,
            "Entities/Audicles/Worlds/" + path_str,
            "Entities/Worlds/Audicles/" + path_str // Adding your new path
        };

        std::ifstream worldFile;
        std::string foundPath;
        for(const auto& path : searchPaths) {
            worldFile.open(path);
            if(worldFile.is_open()) {
                foundPath = path;
                break;
            }
        }

        if(worldFile.is_open()){
            Entity worldProto = json::parse(worldFile).get<Entity>();
            baseSystem.level->worlds.push_back(worldProto);
            worldFile.close();
        } else {
            std::cerr << "Warning: Could not find world file '" << path_str << "' in any known directory." << std::endl;
        }
    }

    // Process instance declarations in non-volume worlds
    for (auto& worldProto : baseSystem.level->worlds) {
        if (!worldProto.isVolume && !worldProto.instances.empty()) {
            std::vector<EntityInstance> processedInstances;
            std::vector<EntityInstance> templates = worldProto.instances;

            for (const auto& instTemplate : templates) {
                const Entity* entityProto = HostLogic::findPrototype(instTemplate.name, entityPrototypes);
                if (!entityProto) {
                    std::cerr << "Warning: Could not find prototype '" << instTemplate.name << "' for instance in world '" << worldProto.name << "'." << std::endl;
                    continue;
                }

                int count = entityProto->count > 1 ? entityProto->count : 1;
                // Allow instance definition in JSON to override prototype's count
                if (instTemplate.prototypeID != 0 && instTemplate.prototypeID != -1) {
                     // This is a placeholder for a better way to handle instance-specific data
                }

                if (entityProto->name == "Star") count = 2000;

                for (int i = 0; i < count; ++i) {
                    glm::vec3 pos = instTemplate.position;
                    glm::vec3 instColor = instTemplate.color;
                    if (baseSystem.world) {
                        if (!instTemplate.colorName.empty()) {
                            auto it = baseSystem.world->colorLibrary.find(instTemplate.colorName);
                            if (it != baseSystem.world->colorLibrary.end()) instColor = it->second;
                        }
                    }
                    if (entityProto->isStar) {
                        std::random_device rd; std::mt19937 gen(rd()); std::uniform_real_distribution<> distrib(0, 1);
                        float theta = distrib(gen) * 2.0f * 3.14159f; float phi = distrib(gen) * 3.14159f;
                        pos = glm::vec3(sin(phi)*cos(theta), cos(phi), sin(phi)*sin(theta)) * 200.0f;
                    }
                    EntityInstance inst = HostLogic::CreateInstance(baseSystem, entityProto->prototypeID, pos, instColor);
                    inst.name = instTemplate.name;
                    inst.size = instTemplate.size;
                    inst.rotation = instTemplate.rotation;
                    inst.text = instTemplate.text;
                    inst.textType = instTemplate.textType;
                    inst.textKey = instTemplate.textKey;
                    inst.font = instTemplate.font;
                    inst.colorName = instTemplate.colorName;
                    inst.topColorName = instTemplate.topColorName;
                    inst.sideColorName = instTemplate.sideColorName;
                    inst.actionType = instTemplate.actionType;
                    inst.actionKey = instTemplate.actionKey;
                    inst.actionValue = instTemplate.actionValue;
                    inst.buttonMode = instTemplate.buttonMode;
                    inst.controlId = instTemplate.controlId;
                    inst.controlRole = instTemplate.controlRole;
                    inst.styleId = instTemplate.styleId;
                    inst.uiState = instTemplate.uiState;
                    inst.uiStates = instTemplate.uiStates;
                    processedInstances.push_back(inst);
                }
            }
            worldProto.instances = processedInstances;
        }
    }
}


void Host::mainLoop() {
    if (!std::get<bool>(registry["Program"])) { return; }
    if (!runOptions.headlessPerf && (!renderBackend || !window)) { return; }
    PerfContext* perf = baseSystem.perf ? baseSystem.perf.get() : nullptr;
    const auto runStart = std::chrono::steady_clock::now();
    struct TerrainRunStatsSample {
        uint64_t totalStepped = 0;
        uint64_t totalBuilt = 0;
        uint64_t totalConsumed = 0;
        size_t pending = 0;
        size_t desired = 0;
        size_t generated = 0;
        size_t jobs = 0;
        double stallForSeconds = 0.0;
        double maxStallSeconds = 0.0;
        uint64_t snapshotCount = 0;
        uint64_t snapshotZeroBuildCount = 0;
        uint64_t snapshotStalledBuildCount = 0;
        uint64_t snapshotStallBurstCount = 0;
        double snapshotBuiltPerSecMin = 0.0;
        double snapshotBuiltPerSecMax = 0.0;
        float lastPrepMs = 0.0f;
        float lastGenerationMs = 0.0f;
    };
    auto readTerrainRunStats = []() {
        TerrainRunStatsSample sample;
        TerrainSystemLogic::GetTerrainStreamingRunStats(
            sample.totalStepped,
            sample.totalBuilt,
            sample.totalConsumed,
            sample.pending,
            sample.desired,
            sample.generated,
            sample.jobs,
            sample.stallForSeconds,
            sample.maxStallSeconds,
            sample.snapshotCount,
            sample.snapshotZeroBuildCount,
            sample.snapshotStalledBuildCount,
            sample.snapshotStallBurstCount,
            sample.snapshotBuiltPerSecMin,
            sample.snapshotBuiltPerSecMax,
            sample.lastPrepMs,
            sample.lastGenerationMs
        );
        return sample;
    };
    const TerrainRunStatsSample terrainRunStartStats = readTerrainRunStats();
    while (!PlatformInput::WindowShouldClose(window)) {
        if (runOptions.autoStopEnabled && runOptions.autoStopSeconds > 0.0) {
            const double elapsedSeconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - runStart
            ).count();
            if (elapsedSeconds >= runOptions.autoStopSeconds) {
                std::cout << "AutoRun: reached " << runOptions.autoStopSeconds
                          << "s, shutting down." << std::endl;
                PlatformInput::SetWindowShouldClose(window, true);
                break;
            }
        }
        if (!runOptions.headlessPerf && renderBackend) {
            renderBackend->beginFrame();
        }
        float currentFrame = static_cast<float>(PlatformInput::GetTimeSeconds());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        baseSystem.frameIndex += 1;
        static auto lastFrameLog = std::chrono::steady_clock::now();
        static double frameMsAccum = 0.0;
        static double swapMsAccum = 0.0;
        static int frameSampleCount = 0;
        static auto lastStepLog = std::chrono::steady_clock::now();
        static auto lastHitchSummaryLog = std::chrono::steady_clock::now();
        static auto lastFramePacingLog = std::chrono::steady_clock::now();
        static std::chrono::steady_clock::time_point lastHitchDetailLog{};
        static int frameHitchCountWindow = 0;
        static double frameHitchWorstMsWindow = 0.0;
        static double frameHitchWorstSwapMsWindow = 0.0;
        static std::unordered_map<std::string, double> stepTotalsMs;
        static std::unordered_map<std::string, int> stepCounts;
        static std::vector<std::pair<std::string, double>> stepScratch;
        static std::unordered_map<std::string, double> frameStepMs;
        static std::vector<std::pair<std::string, double>> frameStepScratch;
        static std::vector<double> framePacingSamplesMs;
        static double framePacingSumMs = 0.0;
        static double framePacingSumSqMs = 0.0;
        static int framePacingOver16Count = 0;
        static int framePacingOver33Count = 0;
        static int framePacingOver50Count = 0;
        struct FramePacingPeakSample {
            uint64_t frameIndex = 0;
            double frameMs = 0.0;
            double swapMs = 0.0;
            std::vector<std::pair<std::string, double>> steps;
        };
        static std::vector<FramePacingPeakSample> framePacingPeakSamples;

        auto frameStart = std::chrono::steady_clock::now();
        if (!runOptions.headlessPerf) {
            PlatformInput::PollEvents();
            if (PlatformInput::IsKeyDown(window, PlatformInput::Key::Escape)) {
                PlatformInput::SetWindowShouldClose(window, true);
            }
        }
        static bool shaderReloadShortcutWasDown = false;
        bool shaderReloadShortcutDown = runOptions.headlessPerf ? false : isShaderReloadShortcutDown(window);
        if (shaderReloadShortcutDown && !shaderReloadShortcutWasDown) {
            registry["ReloadShaders"] = true;
        }
        shaderReloadShortcutWasDown = shaderReloadShortcutDown;
        if (consumeRegistryFlag(registry, "ReloadShaders") || consumeRegistryFlag(registry, "reload_shaders")) {
            std::string reloadReport;
            bool reloadOk = reloadActiveShaders(baseSystem, reloadReport);
            registry["ReloadShadersLastOK"] = reloadOk;
            registry["ReloadShadersLastReport"] = reloadReport;
            std::cout << "[Shaders] " << (reloadOk ? "reload ok: " : "reload failed: ") << reloadReport << std::endl;
        }
        if (reloadRequested) {
            std::string target = reloadTarget;
            reloadRequested = false;
            std::cout << "Host: reloading level request -> " << target << std::endl;
            reloadLevel(target);
        }
        bool perfEnabled = perf && perf->enabled;
        bool profileAllSteps = false;
        if (baseSystem.registry) {
            auto it = baseSystem.registry->find("profileAllSteps");
            if (it != baseSystem.registry->end() && std::holds_alternative<bool>(it->second)) {
                profileAllSteps = std::get<bool>(it->second);
            }
        }
        bool trackFrameHitches = perfEnabled && perf && (perf->frameHitchThresholdMs > 0.0);
        bool trackFramePacing = perfEnabled && perf
            && perf->framePacingEnabled
            && perf->framePacingReportIntervalSeconds > 0.0;
        bool trackFramePacingPeaks = trackFramePacing && perf->framePacingTopPeaks > 0;
        if (trackFrameHitches || trackFramePacingPeaks) {
            frameStepMs.clear();
        }
        for(const auto& step : updateFunctions) {
            if(functionRegistry.count(step.name)) {
                if (runOptions.headlessPerf && !shouldRunInHeadlessPerf(step.name)) {
                    continue;
                }
                if (checkDependencies(step.dependencies)) {
                    bool trackPerf = perfEnabled && !perf->allowlist.empty() &&
                                     perf->allowlist.count(step.name) > 0;
                    bool captureStepTiming = trackPerf
                        || profileAllSteps
                        || trackFrameHitches
                        || trackFramePacingPeaks;
                    if (captureStepTiming) {
                        auto start = std::chrono::steady_clock::now();
                        functionRegistry[step.name](baseSystem, entityPrototypes, deltaTime, window);
                        double elapsedMs = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - start
                        ).count();
                        if (trackPerf) {
                            perf->totalsMs[step.name] += elapsedMs;
                            if (elapsedMs > perf->maxMs[step.name]) {
                                perf->maxMs[step.name] = elapsedMs;
                            }
                            if (perf->hitchThresholdMs > 0.0 && elapsedMs >= perf->hitchThresholdMs) {
                                perf->hitchCounts[step.name] += 1;
                            }
                            perf->counts[step.name] += 1;
                        }
                        if (profileAllSteps) {
                            stepTotalsMs[step.name] += elapsedMs;
                            stepCounts[step.name] += 1;
                        }
                        if (trackFrameHitches || trackFramePacingPeaks) {
                            frameStepMs[step.name] = elapsedMs;
                        }
                    } else {
                        functionRegistry[step.name](baseSystem, entityPrototypes, deltaTime, window);
                    }
                }
            }
        }
        if (perfEnabled) {
            perf->frameCount += 1;
        }
        if (baseSystem.player) {
            PlayerContext& player = *baseSystem.player;
            player.mouseOffsetX = 0.0f;
            player.mouseOffsetY = 0.0f;
            player.scrollYOffset = 0.0;
            player.rightMousePressed = false;
            player.leftMousePressed = false;
            player.middleMousePressed = false;
            player.rightMouseReleased = false;
            player.leftMouseReleased = false;
            player.middleMouseReleased = false;
        }
        auto swapStart = std::chrono::steady_clock::now();
        if (!runOptions.headlessPerf && renderBackend) {
            renderBackend->endFrame(window);
        }
        auto now = std::chrono::steady_clock::now();
        double frameMs = std::chrono::duration<double, std::milli>(now - frameStart).count();
        double swapMs = std::chrono::duration<double, std::milli>(now - swapStart).count();

        frameMsAccum += frameMs;
        swapMsAccum += swapMs;
        frameSampleCount += 1;

        if (trackFramePacing) {
            framePacingSamplesMs.push_back(frameMs);
            framePacingSumMs += frameMs;
            framePacingSumSqMs += frameMs * frameMs;
            if (frameMs > 16.6667) {
                framePacingOver16Count += 1;
            }
            if (frameMs > 33.3333) {
                framePacingOver33Count += 1;
            }
            if (frameMs > 50.0) {
                framePacingOver50Count += 1;
            }
            if (trackFramePacingPeaks) {
                FramePacingPeakSample peakSample;
                peakSample.frameIndex = static_cast<uint64_t>(baseSystem.frameIndex);
                peakSample.frameMs = frameMs;
                peakSample.swapMs = swapMs;
                peakSample.steps.reserve(frameStepMs.size());
                for (const auto& [name, stepMs] : frameStepMs) {
                    peakSample.steps.emplace_back(name, stepMs);
                }
                framePacingPeakSamples.emplace_back(std::move(peakSample));
            }
        }

        if (trackFrameHitches && frameMs >= perf->frameHitchThresholdMs) {
            frameHitchCountWindow += 1;
            frameHitchWorstMsWindow = std::max(frameHitchWorstMsWindow, frameMs);
            frameHitchWorstSwapMsWindow = std::max(frameHitchWorstSwapMsWindow, swapMs);
            const double minDetailIntervalS = std::max(0.0, perf->frameHitchMinReportIntervalSeconds);
            const bool hasPriorDetail = lastHitchDetailLog.time_since_epoch().count() != 0;
            const bool canEmitDetail = !hasPriorDetail
                || std::chrono::duration<double>(now - lastHitchDetailLog).count() >= minDetailIntervalS;
            if (canEmitDetail) {
                frameStepScratch.clear();
                frameStepScratch.reserve(frameStepMs.size());
                for (const auto& [name, stepMs] : frameStepMs) {
                    frameStepScratch.emplace_back(name, stepMs);
                }
                std::sort(frameStepScratch.begin(), frameStepScratch.end(),
                          [](const auto& a, const auto& b) { return a.second > b.second; });
                const int topSteps = std::max(1, perf->frameHitchTopSteps);
                std::cout << "FrameHitch: frame " << frameMs
                          << " ms (swap " << swapMs
                          << " ms, threshold " << perf->frameHitchThresholdMs
                          << " ms). TopSteps:";
                int printed = 0;
                for (const auto& [name, stepMs] : frameStepScratch) {
                    if (printed >= topSteps) break;
                    std::cout << " " << name << "=" << stepMs << "ms";
                    ++printed;
                }
                if (printed == 0) {
                    std::cout << " none";
                }
                std::cout << std::endl;
                lastHitchDetailLog = now;
            }
        }

        if (now - lastFrameLog >= std::chrono::seconds(1)) {
            double avgFrameMs = frameSampleCount > 0 ? frameMsAccum / frameSampleCount : 0.0;
            double avgSwapMs = frameSampleCount > 0 ? swapMsAccum / frameSampleCount : 0.0;
            std::cout << "FrameTiming: avg frame " << avgFrameMs
                      << " ms, avg swap " << avgSwapMs << " ms." << std::endl;
            lastFrameLog = now;
            frameMsAccum = 0.0;
            swapMsAccum = 0.0;
            frameSampleCount = 0;
        }

        if (trackFrameHitches && now - lastHitchSummaryLog >= std::chrono::seconds(1)) {
            std::cout << "FrameHitchSummary: count " << frameHitchCountWindow
                      << ", worstFrame " << frameHitchWorstMsWindow
                      << " ms, worstSwap " << frameHitchWorstSwapMsWindow
                      << " ms, threshold " << perf->frameHitchThresholdMs
                      << " ms." << std::endl;
            frameHitchCountWindow = 0;
            frameHitchWorstMsWindow = 0.0;
            frameHitchWorstSwapMsWindow = 0.0;
            lastHitchSummaryLog = now;
        }

        if (trackFramePacing) {
            const double reportIntervalSeconds = std::max(0.1, perf->framePacingReportIntervalSeconds);
            if (std::chrono::duration<double>(now - lastFramePacingLog).count() >= reportIntervalSeconds) {
                if (!framePacingSamplesMs.empty()) {
                    std::vector<double> sortedFrameMs = framePacingSamplesMs;
                    std::sort(sortedFrameMs.begin(), sortedFrameMs.end());
                    const size_t sampleCount = sortedFrameMs.size();
                    auto percentile = [&](double p) -> double {
                        if (sortedFrameMs.empty()) return 0.0;
                        if (sortedFrameMs.size() == 1) return sortedFrameMs.front();
                        const double clamped = std::max(0.0, std::min(1.0, p));
                        const double pos = clamped * static_cast<double>(sortedFrameMs.size() - 1);
                        const size_t lo = static_cast<size_t>(std::floor(pos));
                        const size_t hi = static_cast<size_t>(std::ceil(pos));
                        if (lo == hi) return sortedFrameMs[lo];
                        const double t = pos - static_cast<double>(lo);
                        return sortedFrameMs[lo] + (sortedFrameMs[hi] - sortedFrameMs[lo]) * t;
                    };
                    const double meanMs = framePacingSumMs / static_cast<double>(sampleCount);
                    const double varianceMs = std::max(
                        0.0,
                        (framePacingSumSqMs / static_cast<double>(sampleCount)) - (meanMs * meanMs)
                    );
                    const double stddevMs = std::sqrt(varianceMs);
                    const double over16Pct = (100.0 * static_cast<double>(framePacingOver16Count))
                        / static_cast<double>(sampleCount);
                    const double over33Pct = (100.0 * static_cast<double>(framePacingOver33Count))
                        / static_cast<double>(sampleCount);
                    const double over50Pct = (100.0 * static_cast<double>(framePacingOver50Count))
                        / static_cast<double>(sampleCount);

                    std::cout << "FramePacing: samples " << sampleCount
                              << ", p50 " << percentile(0.50)
                              << " ms, p95 " << percentile(0.95)
                              << " ms, p99 " << percentile(0.99)
                              << " ms, max " << sortedFrameMs.back()
                              << " ms, stddev " << stddevMs
                              << " ms, >16.7ms " << over16Pct
                              << "%, >33.3ms " << over33Pct
                              << "%, >50ms " << over50Pct
                              << "%." << std::endl;
                }

                if (trackFramePacingPeaks
                    && perf->framePacingTopStepsPerPeak > 0
                    && !framePacingPeakSamples.empty()) {
                    std::vector<FramePacingPeakSample> sortedPeaks = framePacingPeakSamples;
                    std::sort(sortedPeaks.begin(), sortedPeaks.end(),
                              [](const auto& a, const auto& b) { return a.frameMs > b.frameMs; });
                    const int maxPeaks = std::max(0, perf->framePacingTopPeaks);
                    const int peaksToPrint = std::min(
                        maxPeaks,
                        static_cast<int>(sortedPeaks.size())
                    );
                    const int topStepsPerPeak = std::max(1, perf->framePacingTopStepsPerPeak);
                    std::cout << "FramePeaks:";
                    for (int i = 0; i < peaksToPrint; ++i) {
                        auto& peak = sortedPeaks[static_cast<size_t>(i)];
                        auto steps = peak.steps;
                        std::sort(steps.begin(), steps.end(),
                                  [](const auto& a, const auto& b) { return a.second > b.second; });
                        std::cout << " #" << (i + 1)
                                  << " f" << peak.frameIndex
                                  << "=" << peak.frameMs << "ms"
                                  << " (swap " << peak.swapMs << "ms, steps:";
                        int printed = 0;
                        for (const auto& [name, stepMs] : steps) {
                            if (printed >= topStepsPerPeak) break;
                            std::cout << " " << name << "=" << stepMs << "ms";
                            ++printed;
                        }
                        if (printed == 0) {
                            std::cout << " none";
                        }
                        std::cout << ")";
                    }
                    std::cout << std::endl;
                }

                framePacingSamplesMs.clear();
                framePacingPeakSamples.clear();
                framePacingSumMs = 0.0;
                framePacingSumSqMs = 0.0;
                framePacingOver16Count = 0;
                framePacingOver33Count = 0;
                framePacingOver50Count = 0;
                lastFramePacingLog = now;
            }
        }

        if (profileAllSteps && now - lastStepLog >= std::chrono::seconds(1)) {
            stepScratch.clear();
            stepScratch.reserve(stepTotalsMs.size());
            for (const auto& [name, total] : stepTotalsMs) {
                stepScratch.emplace_back(name, total);
            }
            std::sort(stepScratch.begin(), stepScratch.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });
            std::cout << "StepProfile:";
            int printed = 0;
            for (const auto& [name, total] : stepScratch) {
                if (printed >= 6) break;
                int count = stepCounts[name];
                double avg = count > 0 ? total / static_cast<double>(count) : 0.0;
                std::cout << " " << name << "=" << avg << "ms";
                ++printed;
            }
            std::cout << std::endl;
            stepTotalsMs.clear();
            stepCounts.clear();
            lastStepLog = now;
        }

        if (runOptions.headlessPerf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    if (runOptions.autoStopEnabled) {
        const TerrainRunStatsSample terrainRunEndStats = readTerrainRunStats();
        const double elapsedSeconds = std::max(
            0.0001,
            std::chrono::duration<double>(std::chrono::steady_clock::now() - runStart).count()
        );
        const uint64_t builtDelta = (terrainRunEndStats.totalBuilt >= terrainRunStartStats.totalBuilt)
            ? (terrainRunEndStats.totalBuilt - terrainRunStartStats.totalBuilt)
            : terrainRunEndStats.totalBuilt;
        const uint64_t steppedDelta = (terrainRunEndStats.totalStepped >= terrainRunStartStats.totalStepped)
            ? (terrainRunEndStats.totalStepped - terrainRunStartStats.totalStepped)
            : terrainRunEndStats.totalStepped;
        const uint64_t consumedDelta = (terrainRunEndStats.totalConsumed >= terrainRunStartStats.totalConsumed)
            ? (terrainRunEndStats.totalConsumed - terrainRunStartStats.totalConsumed)
            : terrainRunEndStats.totalConsumed;
        const size_t generatedDelta = (terrainRunEndStats.generated >= terrainRunStartStats.generated)
            ? (terrainRunEndStats.generated - terrainRunStartStats.generated)
            : terrainRunEndStats.generated;
        const size_t desiredGap = terrainRunEndStats.desired > terrainRunEndStats.generated
            ? (terrainRunEndStats.desired - terrainRunEndStats.generated)
            : 0;
        const double builtPerSec = static_cast<double>(builtDelta) / elapsedSeconds;
        std::cout << "TerrainCaptureSummary: dt " << elapsedSeconds
                  << "s, built " << builtDelta
                  << " (" << builtPerSec << "/s)"
                  << ", stepped " << steppedDelta
                  << ", consumed " << consumedDelta
                  << ", generatedDelta " << generatedDelta
                  << ", pending " << terrainRunEndStats.pending
                  << ", desiredGap " << desiredGap
                  << ", jobs " << terrainRunEndStats.jobs
                  << ", stallFor " << terrainRunEndStats.stallForSeconds
                  << "s, lastPrepMs " << terrainRunEndStats.lastPrepMs
                  << ", lastGenMs " << terrainRunEndStats.lastGenerationMs
                  << "." << std::endl;
        const uint64_t snapshotDelta = (terrainRunEndStats.snapshotCount >= terrainRunStartStats.snapshotCount)
            ? (terrainRunEndStats.snapshotCount - terrainRunStartStats.snapshotCount)
            : terrainRunEndStats.snapshotCount;
        const uint64_t zeroBuildSnapshotDelta = (terrainRunEndStats.snapshotZeroBuildCount
            >= terrainRunStartStats.snapshotZeroBuildCount)
            ? (terrainRunEndStats.snapshotZeroBuildCount - terrainRunStartStats.snapshotZeroBuildCount)
            : terrainRunEndStats.snapshotZeroBuildCount;
        const uint64_t stalledSnapshotDelta = (terrainRunEndStats.snapshotStalledBuildCount
            >= terrainRunStartStats.snapshotStalledBuildCount)
            ? (terrainRunEndStats.snapshotStalledBuildCount - terrainRunStartStats.snapshotStalledBuildCount)
            : terrainRunEndStats.snapshotStalledBuildCount;
        const uint64_t stallBurstDelta = (terrainRunEndStats.snapshotStallBurstCount
            >= terrainRunStartStats.snapshotStallBurstCount)
            ? (terrainRunEndStats.snapshotStallBurstCount - terrainRunStartStats.snapshotStallBurstCount)
            : terrainRunEndStats.snapshotStallBurstCount;
        const double zeroBuildPct = snapshotDelta > 0
            ? (100.0 * static_cast<double>(zeroBuildSnapshotDelta) / static_cast<double>(snapshotDelta))
            : 0.0;
        const double stalledPct = snapshotDelta > 0
            ? (100.0 * static_cast<double>(stalledSnapshotDelta) / static_cast<double>(snapshotDelta))
            : 0.0;
        std::cout << "TerrainCaptureScorecard: snapshots " << snapshotDelta
                  << ", zeroBuildWindows " << zeroBuildSnapshotDelta
                  << " (" << zeroBuildPct << "%)"
                  << ", stalledWindows " << stalledSnapshotDelta
                  << " (" << stalledPct << "%)"
                  << ", stallBursts " << stallBurstDelta
                  << ", builtPerSec[min,max] " << terrainRunEndStats.snapshotBuiltPerSecMin
                  << "/" << terrainRunEndStats.snapshotBuiltPerSecMax
                  << ", maxStall " << terrainRunEndStats.maxStallSeconds
                  << "s." << std::endl;
    }
}

void Host::cleanup() {
    runCleanupSteps();
    if (!renderBackend) return;
    renderBackend->shutdown(window);
    window = nullptr;
    Shader::SetRenderBackend(nullptr);
    baseSystem.renderBackend = nullptr;
}
