#define GLM_ENABLE_EXPERIMENTAL
#include "Host.h"
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_set>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

// --- Include System Implementations for Single Translation Unit Build ---
#include "BaseEntity.cpp"
#include "BaseSystem/SpawnSystem.cpp"
#include "BaseSystem/CollisionSystem.cpp"
#include "BaseSystem/WalkModeSystem.cpp"
#include "BaseSystem/GravitySystem.cpp"
#include "BaseSystem/SkyboxSystem.cpp" // <-- ADD THIS LINE
#include "BaseSystem/KeyboardInputSystem.cpp"
#include "BaseSystem/BookSystem.cpp"
#include "BaseSystem/MouseInputSystem.cpp"
#include "BaseSystem/UAVSystem.cpp"
#include "BaseSystem/AudioSystem.cpp"
#include "BaseSystem/SoundtrackSystem.cpp"
#include "BaseSystem/MirrorSystem.cpp"
#include "BaseSystem/PanelSystem.cpp"
#include "BaseSystem/PanelRenderSystem.cpp"
#include "BaseSystem/UIStampingSystem.cpp"
#include "BaseSystem/DawTrackSystem.cpp"
#include "BaseSystem/DawTransportSystem.cpp"
#include "BaseSystem/DawClipSystem.cpp"
#include "BaseSystem/DawWaveformSystem.cpp"
#include "BaseSystem/DawUiSystem.cpp"
#include "BaseSystem/DawIOSystem.cpp"
#include "BaseSystem/MidiTrackSystem.cpp"
#include "BaseSystem/AutomationTrackSystem.cpp"
#include "BaseSystem/MidiTransportSystem.cpp"
#include "BaseSystem/MidiWaveformSystem.cpp"
#include "BaseSystem/MidiUiSystem.cpp"
#include "BaseSystem/MidiStampingSystem.cpp"
#include "BaseSystem/MidiIOSystem.cpp"
#include "BaseSystem/DecibelMeterSystem.cpp"
#include "BaseSystem/DawFaderSystem.cpp"
#include "BaseSystem/MicrophoneBlockSystem.cpp"
#include "BaseSystem/RayTracedAudioSystem.cpp"
#include "BaseSystem/SoundPhysicsSystem.cpp"
#include "BaseSystem/AudioRayVisualizerSystem.cpp"
#include "BaseSystem/PinkNoiseSystem.cpp"
#include "BaseSystem/AudicleSystem.cpp"
#include "BaseSystem/AudioVisualizerFollowerSystem.cpp"
#include "BaseSystem/CameraSystem.cpp"
#include "BaseSystem/RenderInitSystem.cpp"
#include "BaseSystem/VoxelMeshInitSystem.cpp"
#include "BaseSystem/VoxelLightingSystem.cpp"
#include "BaseSystem/VoxelMeshingSystem.cpp"
#include "BaseSystem/VoxelMeshUploadSystem.cpp"
#include "BaseSystem/VoxelMeshDebugSystem.cpp"
#include "BaseSystem/WorldRenderHeldItemSystem.cpp"
#include "BaseSystem/MiniVoxelParticleSystem.cpp"
#include "BaseSystem/WorldRenderSystem.cpp"
#include "BaseSystem/OverlayRenderSystem.cpp"
#include "BaseSystem/VolumeFillSystem.cpp"
#include "BaseSystem/GlyphSystem.cpp"
#include "BaseSystem/DebugHudSystem.cpp"
#include "BaseSystem/DebugWireframeSystem.cpp"
#include "BaseSystem/PerfSystem.cpp"
#include "BaseSystem/FontSystem.cpp"
#include "BaseSystem/BlockSelectionSystem.cpp"
#include "BaseSystem/GemBlockInteractionSystem.cpp"
#include "BaseSystem/ChalkCraftSystem.cpp"
#include "BaseSystem/BlockDamageSystem.cpp"
#include "BaseSystem/BlockWorldEditSystem.cpp"
#include "BaseSystem/HeldThrowSystem.cpp"
#include "BaseSystem/BlockChargeSystem.cpp"
#include "BaseSystem/GroundCraftingSystem.cpp"
#include "BaseSystem/OreMiningSystem.cpp"
#include "BaseSystem/GemChiselSystem.cpp"
#include "BaseSystem/GemSystem.cpp"
#include "BaseSystem/BuildSystem.cpp"
#include "BaseSystem/FishingSystem.cpp"
#include "BaseSystem/ColorEmotionSystem.cpp"
#include "BaseSystem/StructurePlacementSystem.cpp"
#include "BaseSystem/StructureCaptureSystem.cpp"
#include "BaseSystem/HUDSystem.cpp"
#include "BaseSystem/ExpanseBiomeSystem.cpp"
#include "BaseSystem/LeyLineSystem.cpp"
#include "BaseSystem/TerrainGenerationSystem.cpp"
#include "BaseSystem/TerrainStreamingPlannerSystem.cpp"
#include "BaseSystem/TerrainSurfaceHydrologySystem.cpp"
#include "BaseSystem/TerrainDepthFeaturesSystem.cpp"
#include "BaseSystem/DepthsCrystalSystem.cpp"
#include "BaseSystem/TreeGenerationSystem.cpp"
#include "BaseSystem/TreeSectionSchedulerSystem.cpp"
#include "BaseSystem/UIScreenSystem.cpp"
#include "BaseSystem/SecurityCameraSystem.cpp"
#include "BaseSystem/DimensionSystem.cpp"
#include "BaseSystem/DawLaneTimelineSystem.cpp"
#include "BaseSystem/DawLaneModalSystem.cpp"
#include "BaseSystem/DawLaneShortcutSystem.cpp"
#include "BaseSystem/DawLaneGestureSystem.cpp"
#include "BaseSystem/DawLaneInputSystem.cpp"
#include "BaseSystem/DawLaneResourceSystem.cpp"
#include "BaseSystem/DawLaneRenderSystem.cpp"
#include "BaseSystem/MidiLaneSystem.cpp"
#include "BaseSystem/AutomationLaneSystem.cpp"
#include "BaseSystem/PianoRollResourceSystem.cpp"
#include "BaseSystem/PianoRollLayoutSystem.cpp"
#include "BaseSystem/PianoRollInputSystem.cpp"
#include "BaseSystem/PianoRollRenderSystem.cpp"
#include "BaseSystem/ComputerCursorSystem.cpp"
#include "BaseSystem/ButtonSystem.cpp"
#include "BaseSystem/RegistryEditorSystem.cpp"
#include "BaseSystem/BootSequenceSystem.cpp"
#include "BaseSystem/ChucKSystem.cpp"
#include "BaseSystem/Vst3System.cpp"
#include "BaseSystem/Vst3BrowserSystem.cpp"
#include "BaseSystem/BlockTextureSystem.cpp"
#include "Structures/VoxelWorld.cpp"
#include "Render/WebGPUBackend.cpp"
#include "Render/Shader.cpp"
#include "Host/PlatformInput.cpp"
#include "Host/HostUtilities.cpp"
#include "Host/Startup.cpp"
#include "Host/HostInput.cpp"
#include "Host/HostLoader.cpp"
#include "Host/Host.cpp"

namespace {
    struct PerfCaptureCliOptions {
        bool enabled = false;
        double runSeconds = 30.0;
        std::string outputPath;
        std::unordered_set<std::string> categories;
        bool teeConsole = false;
        bool headless = false;
        std::string presentModePreference;
    };

    std::string cliToLowerCopy(std::string value) {
        for (char& c : value) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    std::string trimCopy(const std::string& value) {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
            ++start;
        }
        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }
        return value.substr(start, end - start);
    }

    bool parseDoubleStrict(const std::string& raw, double& outValue) {
        try {
            size_t consumed = 0;
            const double parsed = std::stod(raw, &consumed);
            if (consumed != raw.size()) return false;
            outValue = parsed;
            return true;
        } catch (...) {
            return false;
        }
    }

    std::unordered_set<std::string> parseCategorySet(const std::string& csv) {
        std::unordered_set<std::string> categories;
        std::stringstream ss(csv);
        std::string item;
        while (std::getline(ss, item, ',')) {
            const std::string lowered = cliToLowerCopy(trimCopy(item));
            if (!lowered.empty()) categories.insert(lowered);
        }
        return categories;
    }

    std::string defaultPerfCapturePath() {
        namespace fs = std::filesystem;
        const auto now = std::chrono::system_clock::now();
        const auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
        fs::path path = fs::temp_directory_path() / ("salamander_perf_capture_" + std::to_string(epochMs) + ".log");
        return path.string();
    }

    void printUsage() {
        std::cout
            << "Usage: ./cardinal [options]\n"
            << "  --perf-capture                 Enable timed perf capture mode.\n"
            << "  --perf-seconds <N>             Auto-stop after N seconds (default 30).\n"
            << "  --perf-log-file <path>         Output log file path (default: temp file).\n"
            << "  --perf-logs <csv>              Log categories: perf,frame,step,terrain,voxel,hitch,other,all.\n"
            << "                                 Default: perf,frame,step,terrain,voxel,hitch\n"
            << "  --perf-headless                Skip renderer/window init; run terrain perf steps only.\n"
            << "  --perf-tee-console             Also keep matching captured logs in console output.\n"
            << "  --vsync <on|off>               Present mode shortcut (on=fifo, off=immediate/mailbox).\n"
            << "  --present-mode <mode>          WebGPU present mode: auto,fifo,mailbox,immediate.\n"
            << "  --help                         Show this help.\n";
    }

    void setProcessEnvVar(const char* key, const std::string& value) {
        if (!key || value.empty()) return;
#if defined(_WIN32)
        _putenv_s(key, value.c_str());
#else
        setenv(key, value.c_str(), 1);
#endif
    }

    bool startsWith(const std::string& value, const char* prefix) {
        const size_t len = std::char_traits<char>::length(prefix);
        return value.size() >= len && value.compare(0, len, prefix) == 0;
    }

    std::string detectCategory(const std::string& line) {
        if (startsWith(line, "[Perf]")) return "perf";
        if (startsWith(line, "FrameTiming:")) return "frame";
        if (startsWith(line, "FramePacing:")) return "frame";
        if (startsWith(line, "FramePeaks:")) return "frame";
        if (startsWith(line, "StepProfile:")) return "step";
        if (startsWith(line, "TerrainGeneration:")) return "terrain";
        if (startsWith(line, "TerrainSnapshot:")) return "terrain";
        if (startsWith(line, "TerrainCaptureSummary:")) return "terrain";
        if (startsWith(line, "TerrainCaptureScorecard:")) return "terrain";
        if (startsWith(line, "[VoxelPerf]")) return "voxel";
        if (startsWith(line, "[DebugVoxelMesh]")) return "voxel";
        if (startsWith(line, "FrameHitch:")) return "hitch";
        if (startsWith(line, "FrameHitchSummary:")) return "hitch";
        return "other";
    }

    class PerfCaptureSink {
    public:
        bool open(const PerfCaptureCliOptions& options, std::string* outError) {
            opts = options;
            if (opts.outputPath.empty()) {
                opts.outputPath = defaultPerfCapturePath();
            }
            std::error_code ec;
            const std::filesystem::path path(opts.outputPath);
            const std::filesystem::path parent = path.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
                std::filesystem::create_directories(parent, ec);
                if (ec) {
                    if (outError) *outError = "failed to create output directory: " + ec.message();
                    return false;
                }
            }
            file.open(opts.outputPath, std::ios::out | std::ios::trunc);
            if (!file.is_open()) {
                if (outError) *outError = "failed to open output file";
                return false;
            }
            return true;
        }

        const std::string& outputPath() const { return opts.outputPath; }

        void writeLine(const std::string& line,
                       std::streambuf* originalConsoleBuf,
                       bool lineTerminated) {
            std::lock_guard<std::mutex> guard(mu);
            const std::string category = detectCategory(line);
            const bool allowAll = opts.categories.count("all") > 0;
            const bool shouldWrite = allowAll || opts.categories.count(category) > 0;
            if (shouldWrite && file.is_open()) {
                file << line;
                if (lineTerminated) file << '\n';
                file.flush();
            }
            if (opts.teeConsole && originalConsoleBuf && shouldWrite) {
                originalConsoleBuf->sputn(line.data(), static_cast<std::streamsize>(line.size()));
                if (lineTerminated) originalConsoleBuf->sputc('\n');
            }
        }

    private:
        PerfCaptureCliOptions opts;
        std::ofstream file;
        std::mutex mu;
    };

    class PerfCaptureStreamBuf : public std::streambuf {
    public:
        PerfCaptureStreamBuf(PerfCaptureSink& sink, std::streambuf* originalConsoleBuf)
            : sinkRef(sink), originalBuf(originalConsoleBuf) {}

        int overflow(int ch) override {
            if (ch == traits_type::eof()) return traits_type::not_eof(ch);
            const char c = static_cast<char>(ch);
            if (c == '\n') {
                flushCurrentLine(true);
            } else {
                line.push_back(c);
            }
            return ch;
        }

        int sync() override {
            flushCurrentLine(false);
            if (originalBuf) originalBuf->pubsync();
            return 0;
        }

    private:
        void flushCurrentLine(bool hasNewline) {
            if (line.empty() && !hasNewline) return;
            sinkRef.writeLine(line, originalBuf, hasNewline);
            line.clear();
        }

        PerfCaptureSink& sinkRef;
        std::streambuf* originalBuf = nullptr;
        std::string line;
    };

    bool parsePerfCaptureArgs(int argc,
                              char** argv,
                              PerfCaptureCliOptions& outOptions,
                              bool& outPrintedHelp) {
        outPrintedHelp = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                printUsage();
                outPrintedHelp = true;
                return false;
            } else if (arg == "--perf-capture") {
                outOptions.enabled = true;
            } else if (arg == "--perf-tee-console") {
                outOptions.enabled = true;
                outOptions.teeConsole = true;
            } else if (arg == "--perf-headless") {
                outOptions.enabled = true;
                outOptions.headless = true;
            } else if (arg == "--perf-seconds") {
                if (i + 1 >= argc) {
                    std::cerr << "Missing value for --perf-seconds" << std::endl;
                    return false;
                }
                double value = 0.0;
                if (!parseDoubleStrict(argv[++i], value) || value <= 0.0) {
                    std::cerr << "Invalid --perf-seconds value (must be > 0)." << std::endl;
                    return false;
                }
                outOptions.enabled = true;
                outOptions.runSeconds = value;
            } else if (startsWith(arg, "--perf-seconds=")) {
                double value = 0.0;
                if (!parseDoubleStrict(arg.substr(std::string("--perf-seconds=").size()), value) || value <= 0.0) {
                    std::cerr << "Invalid --perf-seconds value (must be > 0)." << std::endl;
                    return false;
                }
                outOptions.enabled = true;
                outOptions.runSeconds = value;
            } else if (arg == "--perf-log-file") {
                if (i + 1 >= argc) {
                    std::cerr << "Missing value for --perf-log-file" << std::endl;
                    return false;
                }
                outOptions.enabled = true;
                outOptions.outputPath = argv[++i];
            } else if (startsWith(arg, "--perf-log-file=")) {
                outOptions.enabled = true;
                outOptions.outputPath = arg.substr(std::string("--perf-log-file=").size());
            } else if (arg == "--perf-logs") {
                if (i + 1 >= argc) {
                    std::cerr << "Missing value for --perf-logs" << std::endl;
                    return false;
                }
                outOptions.enabled = true;
                outOptions.categories = parseCategorySet(argv[++i]);
            } else if (startsWith(arg, "--perf-logs=")) {
                outOptions.enabled = true;
                outOptions.categories = parseCategorySet(arg.substr(std::string("--perf-logs=").size()));
            } else if (arg == "--vsync") {
                if (i + 1 >= argc) {
                    std::cerr << "Missing value for --vsync" << std::endl;
                    return false;
                }
                const std::string value = cliToLowerCopy(argv[++i]);
                if (value == "on" || value == "true" || value == "1") {
                    outOptions.presentModePreference = "vsync-on";
                } else if (value == "off" || value == "false" || value == "0") {
                    outOptions.presentModePreference = "vsync-off";
                } else {
                    std::cerr << "Invalid --vsync value (expected on/off)." << std::endl;
                    return false;
                }
            } else if (startsWith(arg, "--vsync=")) {
                const std::string value = cliToLowerCopy(arg.substr(std::string("--vsync=").size()));
                if (value == "on" || value == "true" || value == "1") {
                    outOptions.presentModePreference = "vsync-on";
                } else if (value == "off" || value == "false" || value == "0") {
                    outOptions.presentModePreference = "vsync-off";
                } else {
                    std::cerr << "Invalid --vsync value (expected on/off)." << std::endl;
                    return false;
                }
            } else if (arg == "--present-mode") {
                if (i + 1 >= argc) {
                    std::cerr << "Missing value for --present-mode" << std::endl;
                    return false;
                }
                outOptions.presentModePreference = cliToLowerCopy(argv[++i]);
            } else if (startsWith(arg, "--present-mode=")) {
                outOptions.presentModePreference = cliToLowerCopy(
                    arg.substr(std::string("--present-mode=").size())
                );
            } else {
                std::cerr << "Warning: ignoring unknown argument '" << arg << "'." << std::endl;
            }
        }

        if (outOptions.enabled && outOptions.categories.empty()) {
            outOptions.categories = {"perf", "frame", "step", "terrain", "voxel", "hitch"};
        }
        return true;
    }
}

int main(int argc, char** argv) {
    PerfCaptureCliOptions captureOptions;
    bool printedHelp = false;
    if (!parsePerfCaptureArgs(argc, argv, captureOptions, printedHelp)) {
        return printedHelp ? 0 : 1;
    }

    if (!captureOptions.presentModePreference.empty()) {
        setProcessEnvVar("SALAMANDER_WEBGPU_PRESENT_MODE", captureOptions.presentModePreference);
    }

    Host cardinal;
    if (captureOptions.enabled) {
        cardinal.configureAutoStop(captureOptions.runSeconds);
        cardinal.configureHeadlessPerf(captureOptions.headless);

        PerfCaptureSink sink;
        std::string openError;
        if (!sink.open(captureOptions, &openError)) {
            std::cerr << "Perf capture setup failed: " << openError << std::endl;
            return 1;
        }

        std::streambuf* originalCout = std::cout.rdbuf();
        std::streambuf* originalCerr = std::cerr.rdbuf();
        PerfCaptureStreamBuf captureCoutBuf(sink, originalCout);
        PerfCaptureStreamBuf captureCerrBuf(sink, originalCerr);
        std::cout.rdbuf(&captureCoutBuf);
        std::cerr.rdbuf(&captureCerrBuf);

        cardinal.run();

        std::cout.flush();
        std::cerr.flush();
        std::cout.rdbuf(originalCout);
        std::cerr.rdbuf(originalCerr);
        std::cout << "Perf capture log saved to: " << sink.outputPath() << std::endl;
        return 0;
    }

    cardinal.run();
    return 0;
}
