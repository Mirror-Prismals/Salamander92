#pragma once

#include "Host/PlatformInput.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "stb_image.h"

namespace DawClipSystemLogic {
    void TrimClipsForNewClip(DawTrack& track, const DawClip& clip);
    void RebuildTrackCacheFromClips(DawContext& daw, DawTrack& track);
    bool CycleTrackLoopTake(DawContext& daw, int trackIndex, int direction);
}
namespace DawTrackSystemLogic {
    bool MoveTrack(BaseSystem& baseSystem, int fromIndex, int toIndex);
}
namespace MidiTransportSystemLogic {
    bool CycleTrackLoopTake(MidiContext& midi, int trackIndex, int direction);
}
namespace DawLaneTimelineSystemLogic {
    struct LaneLayout;
    bool hasDawUiWorld(const LevelContext& level);
    LaneLayout ComputeLaneLayout(const BaseSystem& baseSystem, const DawContext& daw, PlatformWindowHandle win);
    std::vector<int> BuildAudioLaneIndex(const DawContext& daw, int audioTrackCount);
    void ClampTimelineOffset(DawContext& daw);
    double GridSecondsForZoom(double secondsPerScreen, double secondsPerBeat);
    uint64_t MaxTimelineSamples(const DawContext& daw);
}
namespace DawTimelineRebaseLogic { void ShiftTimelineRight(BaseSystem& baseSystem, uint64_t shiftSamples); }
namespace DawIOSystemLogic {
    bool OpenExportFolderDialog(std::string& ioPath);
    bool StartStemExport(BaseSystem& baseSystem);
    std::string ThemeColorToHex(const glm::vec4& color);
    bool ApplyThemeByIndex(BaseSystem& baseSystem, int themeIndex, bool persistToDisk);
    bool RemoveThemeByIndex(BaseSystem& baseSystem, int themeIndex, std::string& outMessage);
    bool SaveThemeFromDraft(BaseSystem& baseSystem,
                            const std::string& rawName,
                            const std::string& backgroundHex,
                            const std::string& panelHex,
                            const std::string& buttonHex,
                            const std::string& pianoRollHex,
                            const std::string& pianoRollAccentHex,
                            const std::string& laneHex,
                            std::string& outMessage);
    void BeginThemeDraftFromDefault(BaseSystem& baseSystem);
    void EnsureThemeState(BaseSystem& baseSystem);
}
namespace AudioSystemLogic {
    void InitializeAudio(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
    void CleanupAudio(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
    bool SupportsManagedJackServer(const BaseSystem&);
    void ListManagedJackDevices(const BaseSystem&,
                                std::vector<std::string>& outputLabels,
                                std::vector<std::string>& outputIds,
                                std::vector<std::string>& inputLabels,
                                std::vector<std::string>& inputIds);
    bool RestartManagedJackWithDevices(BaseSystem&,
                                       std::vector<Entity>&,
                                       float,
                                       PlatformWindowHandle,
                                       const std::string& captureDevice,
                                       const std::string& playbackDevice);
}

namespace DawLaneInputSystemLogic {
    namespace {
        constexpr const char* kDefaultSystemOutputOption = "Default System Output (No Input)";
        constexpr float kLoopHandleWidth = 8.0f;
        constexpr float kTrackHandleSize = 60.0f;
        constexpr float kTrackHandleInset = 12.0f;
        constexpr float kClipHorizontalPad = 2.0f;
        constexpr float kClipVerticalInset = 0.0f;
        constexpr float kClipMinHeight = 2.0f;
        constexpr float kClipLipMinHeight = 6.0f;
        constexpr float kClipLipMaxHeight = 12.0f;
        constexpr float kTrimEdgeHitWidth = 8.0f;
        constexpr uint64_t kMinClipSamples = 1;
        constexpr float kTakeRowGap = 4.0f;
        constexpr float kTakeRowSpacing = 2.0f;
        constexpr float kTakeRowMinHeight = 10.0f;
        constexpr float kTakeRowMaxHeight = 18.0f;

        struct Rect {
            float left = 0.0f;
            float right = 0.0f;
            float top = 0.0f;
            float bottom = 0.0f;
        };

        struct ExportDialogLayout {
            float x = 0.0f;
            float y = 0.0f;
            float w = 0.0f;
            float h = 0.0f;
            Rect closeBtn;
            Rect startMinus;
            Rect startPlus;
            Rect endMinus;
            Rect endPlus;
            Rect folderBtn;
            Rect exportBtn;
            Rect cancelBtn;
            Rect progressBar;
            std::array<Rect, DawContext::kBusCount> stemRows{};
        };

        struct SettingsDialogLayout {
            float x = 0.0f;
            float y = 0.0f;
            float w = 0.0f;
            float h = 0.0f;
            float listRowHeight = 24.0f;
            float listPad = 6.0f;
            Rect panelRect;
            Rect closeBtn;
            Rect tabTheme;
            Rect tabAudio;
            Rect listRect;
            Rect applyBtn;
            Rect createBtn;
            Rect editBtn;
            Rect deleteBtn;
            Rect backBtn;
            Rect saveBtn;
            Rect nameField;
            Rect bgField;
            Rect panelField;
            Rect buttonField;
            Rect pianoField;
            Rect pianoAccentField;
            Rect laneField;
            Rect audioOutField;
            Rect audioOutPrev;
            Rect audioOutNext;
            Rect audioInField;
            Rect audioInPrev;
            Rect audioInNext;
            Rect audioRefreshBtn;
            Rect audioRetryBtn;
        };

        struct AutomationParamMenuLayout {
            bool valid = false;
            Rect buttonRect;
            Rect menuRect;
            float rowHeight = 18.0f;
            float padding = 6.0f;
        };

        struct ClipTrimHit {
            bool valid = false;
            bool leftEdge = false;
            int track = -1;
            int clipIndex = -1;
            float edgeDistance = FLT_MAX;
            uint64_t clipStart = 0;
            uint64_t clipLength = 0;
            uint64_t clipSourceOffset = 0;
        };

        static PlatformInput::CursorHandle g_laneTrimCursor = nullptr;
        static bool g_laneTrimCursorLoaded = false;
        static bool g_laneTrimCursorApplied = false;
        static bool g_cmdEShortcutWasDown = false;
        static bool g_cmdShiftMShortcutWasDown = false;
        static bool g_cmdLShortcutWasDown = false;
        static bool g_spaceShortcutWasDown = false;
        static bool g_deleteShortcutWasDown = false;
        static bool g_cmdPrevTakeShortcutWasDown = false;
        static bool g_cmdNextTakeShortcutWasDown = false;
        static bool g_rightMouseWasDown = false;
        static std::unordered_map<int, uint8_t> g_exportKeyDown{};
        static std::unordered_map<int, uint8_t> g_settingsKeyDown{};

        bool isCommandDown(PlatformWindowHandle win) {
            if (!win) return false;
            return PlatformInput::IsKeyDown(win, PlatformInput::Key::LeftSuper)
                || PlatformInput::IsKeyDown(win, PlatformInput::Key::RightSuper);
        }

        bool isShiftDown(PlatformWindowHandle win) {
            if (!win) return false;
            return PlatformInput::IsKeyDown(win, PlatformInput::Key::LeftShift)
                || PlatformInput::IsKeyDown(win, PlatformInput::Key::RightShift);
        }

        Rect makeRect(float x, float y, float w, float h) {
            Rect rect;
            rect.left = x;
            rect.right = x + w;
            rect.top = y;
            rect.bottom = y + h;
            return rect;
        }

        ExportDialogLayout computeExportDialogLayout(const DawLaneTimelineSystemLogic::LaneLayout& layout) {
            ExportDialogLayout out;
            out.w = 540.0f;
            out.h = 360.0f;
            out.x = std::max(24.0f, static_cast<float>((layout.screenWidth - out.w) * 0.5));
            out.y = std::max(24.0f, static_cast<float>((layout.screenHeight - out.h) * 0.5) - 30.0f);
            const float closeSize = 24.0f;
            out.closeBtn = makeRect(out.x + out.w - closeSize - 12.0f, out.y + 10.0f, closeSize, closeSize);
            out.startMinus = makeRect(out.x + out.w - 180.0f, out.y + 58.0f, 22.0f, 22.0f);
            out.startPlus = makeRect(out.x + out.w - 52.0f, out.y + 58.0f, 22.0f, 22.0f);
            out.endMinus = makeRect(out.x + out.w - 180.0f, out.y + 90.0f, 22.0f, 22.0f);
            out.endPlus = makeRect(out.x + out.w - 52.0f, out.y + 90.0f, 22.0f, 22.0f);
            out.folderBtn = makeRect(out.x + out.w - 120.0f, out.y + 126.0f, 86.0f, 24.0f);
            for (int i = 0; i < DawContext::kBusCount; ++i) {
                out.stemRows[static_cast<size_t>(i)] = makeRect(out.x + 24.0f,
                                                                out.y + 164.0f + static_cast<float>(i) * 34.0f,
                                                                out.w - 48.0f,
                                                                26.0f);
            }
            out.progressBar = makeRect(out.x + 24.0f, out.y + out.h - 58.0f, out.w - 48.0f, 14.0f);
            out.cancelBtn = makeRect(out.x + out.w - 202.0f, out.y + out.h - 34.0f, 82.0f, 22.0f);
            out.exportBtn = makeRect(out.x + out.w - 106.0f, out.y + out.h - 34.0f, 82.0f, 22.0f);
            return out;
        }

        SettingsDialogLayout computeSettingsDialogLayout(const DawLaneTimelineSystemLogic::LaneLayout& layout) {
            SettingsDialogLayout out;
            out.w = 640.0f;
            out.h = 390.0f;
            out.x = std::max(24.0f, static_cast<float>((layout.screenWidth - out.w) * 0.5));
            out.y = std::max(24.0f, static_cast<float>((layout.screenHeight - out.h) * 0.5) - 24.0f);
            out.panelRect = makeRect(out.x, out.y, out.w, out.h);
            const float closeSize = 24.0f;
            out.closeBtn = makeRect(out.x + out.w - closeSize - 12.0f, out.y + 10.0f, closeSize, closeSize);
            out.tabTheme = makeRect(out.x + 18.0f, out.y + 38.0f, 92.0f, 24.0f);
            out.tabAudio = makeRect(out.x + 116.0f, out.y + 38.0f, 92.0f, 24.0f);

            const float bodyTop = out.y + 74.0f;
            const float bodyBottom = out.y + out.h - 46.0f;
            const float bodyHeight = std::max(0.0f, bodyBottom - bodyTop);
            out.listRect = makeRect(out.x + 18.0f, bodyTop, 260.0f, bodyHeight - 38.0f);

            const float btnW = 82.0f;
            const float btnGap = 10.0f;
            const float rowY = out.y + out.h - 34.0f;
            const float right = out.x + out.w - 24.0f;
            out.deleteBtn = makeRect(right - btnW, rowY, btnW, 22.0f);
            out.editBtn = makeRect(out.deleteBtn.left - btnGap - btnW, rowY, btnW, 22.0f);
            out.createBtn = makeRect(out.editBtn.left - btnGap - btnW, rowY, btnW, 22.0f);
            out.applyBtn = makeRect(out.createBtn.left - btnGap - btnW, rowY, btnW, 22.0f);
            out.backBtn = makeRect(out.x + out.w - 214.0f, out.y + out.h - 34.0f, 88.0f, 22.0f);
            out.saveBtn = makeRect(out.x + out.w - 112.0f, out.y + out.h - 34.0f, 88.0f, 22.0f);

            float fieldX = out.x + 314.0f;
            float fieldW = out.w - 338.0f;
            out.nameField = makeRect(fieldX, bodyTop + 18.0f, fieldW, 28.0f);
            out.bgField = makeRect(fieldX, bodyTop + 56.0f, fieldW, 28.0f);
            out.panelField = makeRect(fieldX, bodyTop + 94.0f, fieldW, 28.0f);
            out.buttonField = makeRect(fieldX, bodyTop + 132.0f, fieldW, 28.0f);
            out.pianoField = makeRect(fieldX, bodyTop + 170.0f, fieldW, 28.0f);
            out.pianoAccentField = makeRect(fieldX, bodyTop + 208.0f, fieldW, 28.0f);
            out.laneField = makeRect(fieldX, bodyTop + 246.0f, fieldW, 28.0f);

            const float audioLabelX = out.x + 48.0f;
            const float audioFieldX = out.x + 180.0f;
            const float audioFieldW = out.w - 248.0f;
            const float audioBtnY0 = bodyTop + 58.0f;
            const float audioBtnY1 = bodyTop + 118.0f;
            out.audioOutPrev = makeRect(audioLabelX, audioBtnY0, 26.0f, 24.0f);
            out.audioOutField = makeRect(audioFieldX, audioBtnY0, audioFieldW, 24.0f);
            out.audioOutNext = makeRect(out.audioOutField.right + 8.0f, audioBtnY0, 26.0f, 24.0f);
            out.audioInPrev = makeRect(audioLabelX, audioBtnY1, 26.0f, 24.0f);
            out.audioInField = makeRect(audioFieldX, audioBtnY1, audioFieldW, 24.0f);
            out.audioInNext = makeRect(out.audioInField.right + 8.0f, audioBtnY1, 26.0f, 24.0f);
            out.audioRefreshBtn = makeRect(out.x + out.w - 222.0f, out.y + out.h - 34.0f, 88.0f, 22.0f);
            out.audioRetryBtn = makeRect(out.x + out.w - 122.0f, out.y + out.h - 34.0f, 98.0f, 22.0f);
            return out;
        }

        Rect settingsThemeRowRect(const SettingsDialogLayout& layout, int rowIndex) {
            float y = layout.listRect.top + layout.listPad + static_cast<float>(rowIndex) * layout.listRowHeight;
            return makeRect(layout.listRect.left + 2.0f,
                            y,
                            (layout.listRect.right - layout.listRect.left) - 4.0f,
                            layout.listRowHeight - 2.0f);
        }

        bool cursorInRect(const UIContext& ui, const Rect& rect) {
            return ui.cursorX >= rect.left
                && ui.cursorX <= rect.right
                && ui.cursorY >= rect.top
                && ui.cursorY <= rect.bottom;
        }

        int parseTrackIndex(const std::string& value, int trackCount) {
            if (value.empty()) return -1;
            try {
                int idx = std::stoi(value);
                if (idx < 0 || idx >= trackCount) return -1;
                return idx;
            } catch (...) {
                return -1;
            }
        }

        int parseTrackIndexFromControlId(const std::string& controlId, int trackCount) {
            constexpr const char* kPrefix = "auto_track_";
            if (controlId.rfind(kPrefix, 0) != 0) return -1;
            size_t start = std::char_traits<char>::length(kPrefix);
            size_t end = controlId.find('_', start);
            if (end == std::string::npos) return -1;
            return parseTrackIndex(controlId.substr(start, end - start), trackCount);
        }

        bool findAutomationParamButtonRect(const DawContext& daw, int trackIndex, Rect& outRect) {
            int trackCount = static_cast<int>(daw.automationTracks.size());
            if (trackIndex < 0 || trackIndex >= trackCount) return false;
            for (auto* instPtr : daw.trackInstances) {
                if (!instPtr) continue;
                const EntityInstance& inst = *instPtr;
                if (inst.actionType != "DawAutomationTrack") continue;
                if (inst.actionKey != "target_param") continue;
                int instTrack = parseTrackIndex(inst.actionValue, trackCount);
                if (instTrack < 0) {
                    instTrack = parseTrackIndexFromControlId(inst.controlId, trackCount);
                }
                if (instTrack != trackIndex) continue;
                outRect = makeRect(inst.position.x - inst.size.x,
                                   inst.position.y - inst.size.y,
                                   inst.size.x * 2.0f,
                                   inst.size.y * 2.0f);
                return true;
            }
            return false;
        }

        AutomationParamMenuLayout computeAutomationParamMenuLayout(const DawContext& daw,
                                                                   const DawLaneTimelineSystemLogic::LaneLayout& layout) {
            AutomationParamMenuLayout out;
            if (!daw.automationParamMenuOpen) return out;
            if (daw.automationParamMenuTrack < 0
                || daw.automationParamMenuTrack >= static_cast<int>(daw.automationTracks.size())) return out;
            if (daw.automationParamMenuLabels.empty()) return out;
            if (!findAutomationParamButtonRect(daw, daw.automationParamMenuTrack, out.buttonRect)) return out;

            const float menuWidth = 180.0f;
            const float rowHeight = out.rowHeight;
            const float pad = out.padding;
            const float menuHeight = static_cast<float>(daw.automationParamMenuLabels.size()) * rowHeight + pad * 2.0f;
            float x = out.buttonRect.left;
            float y = out.buttonRect.bottom + 6.0f;
            if (x + menuWidth > layout.screenWidth - 8.0f) {
                x = static_cast<float>(layout.screenWidth) - menuWidth - 8.0f;
            }
            if (x < 8.0f) x = 8.0f;
            if (y + menuHeight > layout.screenHeight - 8.0f) {
                y = out.buttonRect.top - 6.0f - menuHeight;
            }
            if (y < 8.0f) y = 8.0f;
            out.menuRect = makeRect(x, y, menuWidth, menuHeight);
            out.valid = true;
            return out;
        }

        std::string deviceNameFromPort(const std::string& port) {
            size_t sep = port.find(':');
            if (sep == std::string::npos || sep == 0) return port;
            return port.substr(0, sep);
        }

        std::vector<std::string> queryJackPorts(jack_client_t* client, unsigned long flags) {
            std::vector<std::string> out;
            if (!client) return out;
            if (const char** ports = jack_get_ports(client, nullptr, JACK_DEFAULT_AUDIO_TYPE, flags)) {
                for (size_t i = 0; ports[i]; ++i) {
                    out.emplace_back(ports[i]);
                }
                jack_free(ports);
            }
            return out;
        }

        void buildDeviceOptions(const std::vector<std::string>& ports,
                                std::vector<std::string>& outNames,
                                std::vector<std::vector<std::string>>& outPortGroups) {
            outNames.clear();
            outPortGroups.clear();
            for (const auto& port : ports) {
                const std::string device = deviceNameFromPort(port);
                int index = -1;
                for (int i = 0; i < static_cast<int>(outNames.size()); ++i) {
                    if (outNames[static_cast<size_t>(i)] == device) {
                        index = i;
                        break;
                    }
                }
                if (index < 0) {
                    outNames.push_back(device);
                    outPortGroups.emplace_back();
                    index = static_cast<int>(outNames.size()) - 1;
                }
                outPortGroups[static_cast<size_t>(index)].push_back(port);
            }
        }

        bool reconnectTrackInputForSettings(AudioContext& audio, DawTrack& track);

        std::vector<std::string> resolveDefaultSystemOutputPorts(jack_client_t* client) {
            std::vector<std::string> names;
            std::vector<std::vector<std::string>> groups;
            buildDeviceOptions(queryJackPorts(client, JackPortIsInput), names, groups);
            if (names.empty() || groups.empty()) return {};

            std::string ownClientName;
            if (const char* rawName = jack_get_client_name(client)) {
                ownClientName = rawName;
            }
            int preferredIndex = -1;
            for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                const std::string& name = names[static_cast<size_t>(i)];
                if (!ownClientName.empty() && name == ownClientName) continue;
                if (name == "system") return groups[static_cast<size_t>(i)];
                if (preferredIndex < 0) preferredIndex = i;
            }
            if (preferredIndex < 0) return {};
            return groups[static_cast<size_t>(preferredIndex)];
        }

        void buildOutputDeviceOptions(jack_client_t* client,
                                      std::vector<std::string>& outNames,
                                      std::vector<std::vector<std::string>>& outGroups) {
            outNames.clear();
            outGroups.clear();
            if (!client) return;

            buildDeviceOptions(queryJackPorts(client, JackPortIsInput | JackPortIsPhysical),
                               outNames,
                               outGroups);
            const std::vector<std::string> defaultPorts = resolveDefaultSystemOutputPorts(client);
            if (!defaultPorts.empty()) {
                outNames.insert(outNames.begin(), kDefaultSystemOutputOption);
                outGroups.insert(outGroups.begin(), defaultPorts);
            }
        }

        bool isOutputOnlyModeSelected(const DawContext& daw) {
            return daw.settingsAudioOutputName == kDefaultSystemOutputOption;
        }

        void disableTrackPhysicalInputsForOutputOnly(DawContext& daw, AudioContext& audio) {
            if (!audio.client) return;
            audio.physicalInputPorts.clear();
            for (jack_port_t* inputPort : audio.input_ports) {
                if (!inputPort) continue;
                jack_port_disconnect(audio.client, inputPort);
            }
            for (auto& track : daw.tracks) {
                track.stereoInputPair12 = false;
                track.physicalInputIndex = 0;
                track.useVirtualInput.store(true, std::memory_order_relaxed);
                reconnectTrackInputForSettings(audio, track);
            }
            daw.settingsAudioInputName.clear();
            daw.settingsSelectedAudioInput = -1;
        }

        void refreshAudioDeviceLists(BaseSystem& baseSystem, DawContext& daw, AudioContext& audio) {
            daw.settingsAudioInputDevices.clear();
            daw.settingsAudioOutputDevices.clear();
            daw.settingsAudioInputDeviceIds.clear();
            daw.settingsAudioOutputDeviceIds.clear();
            daw.settingsSelectedAudioInput = -1;
            daw.settingsSelectedAudioOutput = -1;
            const bool managed = AudioSystemLogic::SupportsManagedJackServer(baseSystem);
            if (managed) {
                AudioSystemLogic::ListManagedJackDevices(baseSystem,
                                                         daw.settingsAudioOutputDevices,
                                                         daw.settingsAudioOutputDeviceIds,
                                                         daw.settingsAudioInputDevices,
                                                         daw.settingsAudioInputDeviceIds);

                auto resolveSelection = [](const std::vector<std::string>& ids,
                                           const std::string& selectedId,
                                           const std::string& preferredId) -> int {
                    if (ids.empty()) return -1;
                    if (!selectedId.empty()) {
                        for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
                            if (ids[static_cast<size_t>(i)] == selectedId) return i;
                        }
                    }
                    if (!preferredId.empty()) {
                        for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
                            if (ids[static_cast<size_t>(i)] == preferredId) return i;
                        }
                    }
                    return 0;
                };

                daw.settingsSelectedAudioOutput = resolveSelection(daw.settingsAudioOutputDeviceIds,
                                                                   daw.settingsAudioOutputName,
                                                                   "BuiltInSpeakerDevice");
                daw.settingsSelectedAudioInput = resolveSelection(daw.settingsAudioInputDeviceIds,
                                                                  daw.settingsAudioInputName,
                                                                  "BuiltInMicrophoneDevice");
                if (daw.settingsSelectedAudioOutput >= 0
                    && daw.settingsSelectedAudioOutput < static_cast<int>(daw.settingsAudioOutputDeviceIds.size())) {
                    daw.settingsAudioOutputName =
                        daw.settingsAudioOutputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioOutput)];
                }
                if (daw.settingsSelectedAudioInput >= 0
                    && daw.settingsSelectedAudioInput < static_cast<int>(daw.settingsAudioInputDeviceIds.size())) {
                    daw.settingsAudioInputName =
                        daw.settingsAudioInputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioInput)];
                }
                return;
            }

            if (!audio.client) return;

            std::vector<std::string> inputNames;
            std::vector<std::string> outputNames;
            std::vector<std::vector<std::string>> inputGroups;
            std::vector<std::vector<std::string>> outputGroups;
            buildDeviceOptions(queryJackPorts(audio.client, JackPortIsOutput | JackPortIsPhysical),
                               inputNames,
                               inputGroups);
            buildOutputDeviceOptions(audio.client, outputNames, outputGroups);

            daw.settingsAudioInputDevices = inputNames;
            daw.settingsAudioOutputDevices = outputNames;
            daw.settingsAudioInputDeviceIds = inputNames;
            daw.settingsAudioOutputDeviceIds = outputNames;

            auto resolveSelection = [](const std::vector<std::string>& ids,
                                       const std::string& selectedId) -> int {
                if (ids.empty()) return -1;
                if (selectedId.empty()) return 0;
                for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
                    if (ids[static_cast<size_t>(i)] == selectedId) {
                        return i;
                    }
                }
                return 0;
            };

            daw.settingsSelectedAudioInput = resolveSelection(daw.settingsAudioInputDeviceIds,
                                                              daw.settingsAudioInputName);
            daw.settingsSelectedAudioOutput = resolveSelection(daw.settingsAudioOutputDeviceIds,
                                                               daw.settingsAudioOutputName);
            if (daw.settingsSelectedAudioInput >= 0
                && daw.settingsSelectedAudioInput < static_cast<int>(daw.settingsAudioInputDeviceIds.size())) {
                daw.settingsAudioInputName =
                    daw.settingsAudioInputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioInput)];
            }
            if (daw.settingsSelectedAudioOutput >= 0
                && daw.settingsSelectedAudioOutput < static_cast<int>(daw.settingsAudioOutputDeviceIds.size())) {
                daw.settingsAudioOutputName =
                    daw.settingsAudioOutputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioOutput)];
            }
            if (isOutputOnlyModeSelected(daw)) {
                daw.settingsAudioInputDevices.clear();
                daw.settingsAudioInputDeviceIds.clear();
                daw.settingsSelectedAudioInput = -1;
                daw.settingsAudioInputName.clear();
            }
        }

        bool applyManagedAudioSelection(BaseSystem& baseSystem,
                                        std::vector<Entity>& prototypes,
                                        DawContext& daw,
                                        float dt,
                                        PlatformWindowHandle win) {
            if (daw.settingsSelectedAudioOutput < 0
                || daw.settingsSelectedAudioOutput >= static_cast<int>(daw.settingsAudioOutputDeviceIds.size())) {
                return false;
            }
            if (daw.settingsSelectedAudioInput < 0
                || daw.settingsSelectedAudioInput >= static_cast<int>(daw.settingsAudioInputDeviceIds.size())) {
                return false;
            }
            const std::string& playbackId =
                daw.settingsAudioOutputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioOutput)];
            const std::string& captureId =
                daw.settingsAudioInputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioInput)];
            const bool ok = AudioSystemLogic::RestartManagedJackWithDevices(baseSystem,
                                                                            prototypes,
                                                                            dt,
                                                                            win,
                                                                            captureId,
                                                                            playbackId);
            if (baseSystem.audio) {
                refreshAudioDeviceLists(baseSystem, daw, *baseSystem.audio);
            }
            return ok;
        }

        bool reconnectTrackInputForSettings(AudioContext& audio, DawTrack& track) {
            if (!audio.client) return false;
            if (track.inputIndex < 0 || track.inputIndex >= static_cast<int>(audio.input_ports.size())) return false;
            jack_port_t* inputPort = audio.input_ports[static_cast<size_t>(track.inputIndex)];
            if (!inputPort) return false;

            jack_port_disconnect(audio.client, inputPort);
            const int physicalCount = static_cast<int>(audio.physicalInputPorts.size());
            const bool stereoPair12 = track.stereoInputPair12 && physicalCount >= 2;
            const bool useVirtual = !stereoPair12
                && (track.physicalInputIndex < 0 || track.physicalInputIndex >= physicalCount);
            track.useVirtualInput.store(useVirtual, std::memory_order_relaxed);
            if (useVirtual || physicalCount <= 0) return true;

            const int srcIndex = stereoPair12
                ? 0
                : std::clamp(track.physicalInputIndex, 0, std::max(0, physicalCount - 1));
            const std::string& src = audio.physicalInputPorts[static_cast<size_t>(srcIndex)];
            const char* dst = jack_port_name(inputPort);
            if (!dst) return false;
            int rc = jack_connect(audio.client, src.c_str(), dst);
            return rc == 0 || rc == EEXIST;
        }

        bool applySelectedInputDevice(DawContext& daw, AudioContext& audio) {
            if (!audio.client) return false;
            if (daw.settingsSelectedAudioInput < 0
                || daw.settingsSelectedAudioInput >= static_cast<int>(daw.settingsAudioInputDevices.size())) {
                return false;
            }
            std::vector<std::string> names;
            std::vector<std::vector<std::string>> groups;
            buildDeviceOptions(queryJackPorts(audio.client, JackPortIsOutput | JackPortIsPhysical),
                               names,
                               groups);
            int selected = -1;
            const std::string& desired = daw.settingsAudioInputDevices[static_cast<size_t>(daw.settingsSelectedAudioInput)];
            for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                if (names[static_cast<size_t>(i)] == desired) {
                    selected = i;
                    break;
                }
            }
            if (selected < 0 || selected >= static_cast<int>(groups.size())) return false;

            audio.physicalInputPorts = groups[static_cast<size_t>(selected)];
            const int physicalCount = static_cast<int>(audio.physicalInputPorts.size());
            for (auto& track : daw.tracks) {
                if (track.physicalInputIndex < 0 || track.physicalInputIndex >= std::max(1, physicalCount + 1)) {
                    track.physicalInputIndex = 0;
                }
                if (track.stereoInputPair12 && physicalCount < 2) {
                    track.stereoInputPair12 = false;
                }
                track.useVirtualInput.store(physicalCount == 0, std::memory_order_relaxed);
                reconnectTrackInputForSettings(audio, track);
            }
            daw.settingsAudioInputName = desired;
            return true;
        }

        bool applySelectedOutputDevice(DawContext& daw, AudioContext& audio) {
            if (!audio.client) return false;
            if (daw.settingsSelectedAudioOutput < 0
                || daw.settingsSelectedAudioOutput >= static_cast<int>(daw.settingsAudioOutputDevices.size())) {
                return false;
            }
            std::vector<std::string> names;
            std::vector<std::vector<std::string>> groups;
            buildOutputDeviceOptions(audio.client, names, groups);
            int selected = -1;
            const std::string& desired = daw.settingsAudioOutputDevices[static_cast<size_t>(daw.settingsSelectedAudioOutput)];
            for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                if (names[static_cast<size_t>(i)] == desired) {
                    selected = i;
                    break;
                }
            }
            if (selected < 0 || selected >= static_cast<int>(groups.size())) return false;

            if (audio.output_ports.empty()) return false;
            const auto& ports = groups[static_cast<size_t>(selected)];
            if (ports.empty()) return false;

            const int outCount = std::min<int>(2, static_cast<int>(audio.output_ports.size()));
            bool anyConnected = false;
            for (int i = 0; i < outCount; ++i) {
                jack_port_t* outPort = audio.output_ports[static_cast<size_t>(i)];
                if (!outPort) continue;
                jack_port_disconnect(audio.client, outPort);
                const std::string& dst = ports[std::min<size_t>(static_cast<size_t>(i), ports.size() - 1)];
                const char* src = jack_port_name(outPort);
                if (!src) continue;
                int rc = jack_connect(audio.client, src, dst.c_str());
                if (rc == 0 || rc == EEXIST) {
                    anyConnected = true;
                }
            }
            if (desired == kDefaultSystemOutputOption) {
                disableTrackPhysicalInputsForOutputOnly(daw, audio);
            }
            daw.settingsAudioOutputName = desired;
            return anyConnected;
        }

        bool keyPressedEdge(PlatformWindowHandle win,
                            PlatformInput::Key key,
                            std::unordered_map<int, uint8_t>& cache) {
            if (!win) return false;
            bool down = PlatformInput::IsKeyDown(win, key);
            const int keyId = static_cast<int>(key);
            bool pressed = down && (cache[keyId] == 0u);
            cache[keyId] = down ? 1u : 0u;
            return pressed;
        }


    }

    namespace {
        bool settingsKeyPressed(PlatformWindowHandle win, PlatformInput::Key key);
        void updateStemNameTyping(PlatformWindowHandle win, DawContext& daw, bool shiftDown);
        void updateThemeDraftTyping(PlatformWindowHandle win, DawContext& daw, bool shiftDown);
        PlatformInput::CursorHandle ensureLaneTrimCursor();
    }

    void ApplyLaneResizeCursor(PlatformWindowHandle win, bool active) {
        if (!win) return;
        if (active) {
            PlatformInput::CursorHandle cursor = ensureLaneTrimCursor();
            if (cursor) {
                PlatformInput::SetCursor(win, cursor);
                g_laneTrimCursorApplied = true;
            }
        } else if (g_laneTrimCursorApplied) {
            PlatformInput::SetCursor(win, nullptr);
            g_laneTrimCursorApplied = false;
        }
    }

    void UpdateDawLaneModal(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        if (!baseSystem.ui || !baseSystem.daw || !baseSystem.level || !win) return;
        UIContext& ui = *baseSystem.ui;
        if (!ui.active || ui.loadingActive) return;
        if (baseSystem.midi && baseSystem.midi->pianoRollActive) return;
        if (!DawLaneTimelineSystemLogic::hasDawUiWorld(*baseSystem.level)) return;

        DawContext& daw = *baseSystem.daw;
        const auto layout = DawLaneTimelineSystemLogic::ComputeLaneLayout(baseSystem, daw, win);
        const bool exportInProgress = daw.exportInProgress.load(std::memory_order_relaxed);

        if (daw.exportMenuOpen || exportInProgress) {
            const ExportDialogLayout exportLayout = computeExportDialogLayout(layout);
            const Rect panelRect = makeRect(exportLayout.x, exportLayout.y, exportLayout.w, exportLayout.h);

            auto closeExportMenu = [&]() {
                daw.exportMenuOpen = false;
                daw.exportSelectedStem = -1;
                g_exportKeyDown.clear();
            };
            auto clampBars = [&]() {
                daw.exportStartBar = std::clamp(daw.exportStartBar, -4096, 4096);
                daw.exportEndBar = std::clamp(daw.exportEndBar, -4096, 4096);
                if (daw.exportEndBar <= daw.exportStartBar) {
                    daw.exportEndBar = daw.exportStartBar + 1;
                }
            };

            if (ui.uiLeftPressed) {
                bool clickedInside = cursorInRect(ui, panelRect);
                if (cursorInRect(ui, exportLayout.closeBtn) || cursorInRect(ui, exportLayout.cancelBtn)) {
                    if (!exportInProgress) {
                        closeExportMenu();
                    }
                    ui.consumeClick = true;
                } else if (!exportInProgress && cursorInRect(ui, exportLayout.startMinus)) {
                    daw.exportStartBar -= 1;
                    clampBars();
                    ui.consumeClick = true;
                } else if (!exportInProgress && cursorInRect(ui, exportLayout.startPlus)) {
                    daw.exportStartBar += 1;
                    clampBars();
                    ui.consumeClick = true;
                } else if (!exportInProgress && cursorInRect(ui, exportLayout.endMinus)) {
                    daw.exportEndBar -= 1;
                    clampBars();
                    ui.consumeClick = true;
                } else if (!exportInProgress && cursorInRect(ui, exportLayout.endPlus)) {
                    daw.exportEndBar += 1;
                    clampBars();
                    ui.consumeClick = true;
                } else if (!exportInProgress && cursorInRect(ui, exportLayout.folderBtn)) {
                    std::string folder = daw.exportFolderPath;
                    if (DawIOSystemLogic::OpenExportFolderDialog(folder)) {
                        daw.exportFolderPath = folder;
                        daw.exportStatusMessage.clear();
                    }
                    ui.consumeClick = true;
                } else if (!exportInProgress && cursorInRect(ui, exportLayout.exportBtn)) {
                    if (DawIOSystemLogic::StartStemExport(baseSystem)) {
                        daw.exportMenuOpen = true;
                        daw.exportStatusMessage = "Exporting stems...";
                    }
                    ui.consumeClick = true;
                } else {
                    int selectedStem = -1;
                    for (int i = 0; i < DawContext::kBusCount; ++i) {
                        if (cursorInRect(ui, exportLayout.stemRows[static_cast<size_t>(i)])) {
                            selectedStem = i;
                            break;
                        }
                    }
                    if (selectedStem >= 0) {
                        daw.exportSelectedStem = selectedStem;
                        ui.consumeClick = true;
                    } else if (!clickedInside && !exportInProgress) {
                        closeExportMenu();
                        ui.consumeClick = true;
                    } else {
                        ui.consumeClick = clickedInside;
                    }
                }
            }

            if (!exportInProgress) {
                updateStemNameTyping(win, daw, isShiftDown(win));
            } else {
                daw.exportSelectedStem = -1;
            }

            g_cmdEShortcutWasDown = false;
            g_cmdShiftMShortcutWasDown = false;
            g_cmdLShortcutWasDown = false;
            g_spaceShortcutWasDown = false;
            g_deleteShortcutWasDown = false;
            g_cmdPrevTakeShortcutWasDown = false;
            g_cmdNextTakeShortcutWasDown = false;
            g_rightMouseWasDown = (PlatformInput::IsMouseButtonDown(win, PlatformInput::MouseButton::Right));
            ApplyLaneResizeCursor(win, false);
            return;
        }

        if (daw.settingsMenuOpen) {
            DawIOSystemLogic::EnsureThemeState(baseSystem);
            const SettingsDialogLayout settingsLayout = computeSettingsDialogLayout(layout);

            auto closeSettingsMenu = [&]() {
                daw.settingsMenuOpen = false;
                daw.themeCreateMode = false;
                daw.themeEditField = -1;
                g_settingsKeyDown.clear();
            };
            auto selectedTheme = [&]() -> const DawThemePreset* {
                if (daw.settingsSelectedTheme < 0
                    || daw.settingsSelectedTheme >= static_cast<int>(daw.themes.size())) {
                    return nullptr;
                }
                return &daw.themes[static_cast<size_t>(daw.settingsSelectedTheme)];
            };
            auto selectedThemeDeleteProtected = [&]() -> bool {
                const DawThemePreset* preset = selectedTheme();
                if (!preset) return false;
                return preset->name == "Default"
                    || preset->name == "Default 2"
                    || preset->name == "Default 3";
            };
            auto cycleSelection = [](int& selected, int count, int delta) -> bool {
                if (count <= 0) return false;
                if (selected < 0 || selected >= count) {
                    selected = 0;
                } else {
                    selected = (selected + delta + count) % count;
                }
                return true;
            };
            auto refreshAudioSettings = [&]() {
                if (!baseSystem.audio) return;
                refreshAudioDeviceLists(baseSystem, daw, *baseSystem.audio);
            };
            auto applyAudioOutputSelection = [&]() {
                const bool managed = AudioSystemLogic::SupportsManagedJackServer(baseSystem);
                if (managed) {
                    if (applyManagedAudioSelection(baseSystem, prototypes, daw, dt, win)) {
                        daw.settingsAudioStatusMessage = "Audio device applied.";
                    } else {
                        daw.settingsAudioStatusMessage = "Failed to restart audio device.";
                    }
                    refreshAudioSettings();
                    return;
                }
                if (!baseSystem.audio || !baseSystem.audio->client) {
                    daw.settingsAudioStatusMessage = "JACK is offline. Press Retry Audio.";
                    return;
                }
                if (applySelectedOutputDevice(daw, *baseSystem.audio)) {
                    daw.settingsAudioStatusMessage = "Output device applied.";
                } else {
                    daw.settingsAudioStatusMessage = "Failed to apply output device.";
                }
                refreshAudioSettings();
            };
            auto applyAudioInputSelection = [&]() {
                const bool managed = AudioSystemLogic::SupportsManagedJackServer(baseSystem);
                if (managed) {
                    if (applyManagedAudioSelection(baseSystem, prototypes, daw, dt, win)) {
                        daw.settingsAudioStatusMessage = "Audio device applied.";
                    } else {
                        daw.settingsAudioStatusMessage = "Failed to restart audio device.";
                    }
                    refreshAudioSettings();
                    return;
                }
                if (!baseSystem.audio || !baseSystem.audio->client) {
                    daw.settingsAudioStatusMessage = "JACK is offline. Press Retry Audio.";
                    return;
                }
                if (isOutputOnlyModeSelected(daw)) {
                    disableTrackPhysicalInputsForOutputOnly(daw, *baseSystem.audio);
                    daw.settingsAudioStatusMessage = "Default output mode active. Input disabled.";
                    refreshAudioSettings();
                    return;
                }
                if (applySelectedInputDevice(daw, *baseSystem.audio)) {
                    daw.settingsAudioStatusMessage = "Input device applied.";
                } else {
                    daw.settingsAudioStatusMessage = "Failed to apply input device.";
                }
                refreshAudioSettings();
            };

            if (daw.settingsTab == 1 && baseSystem.audio) {
                const bool managed = AudioSystemLogic::SupportsManagedJackServer(baseSystem);
                if (!managed || daw.settingsAudioOutputDevices.empty() || daw.settingsAudioInputDevices.empty()) {
                    refreshAudioSettings();
                }
                if (daw.settingsAudioStatusMessage.empty()) {
                    daw.settingsAudioStatusMessage = baseSystem.audio->client
                        ? "JACK connected."
                        : "JACK offline. Game runs without realtime audio.";
                }
            }

            if (ui.uiLeftPressed) {
                const bool clickedInside = cursorInRect(ui, settingsLayout.panelRect);
                if (cursorInRect(ui, settingsLayout.closeBtn)) {
                    closeSettingsMenu();
                    ui.consumeClick = true;
                } else if (!clickedInside) {
                    closeSettingsMenu();
                    ui.consumeClick = true;
                } else if (cursorInRect(ui, settingsLayout.tabTheme)) {
                    daw.settingsTab = 0;
                    daw.themeCreateMode = false;
                    daw.themeEditField = -1;
                    g_settingsKeyDown.clear();
                    ui.consumeClick = true;
                } else if (cursorInRect(ui, settingsLayout.tabAudio)) {
                    daw.settingsTab = 1;
                    daw.themeCreateMode = false;
                    daw.themeEditField = -1;
                    g_settingsKeyDown.clear();
                    refreshAudioSettings();
                    if (AudioSystemLogic::SupportsManagedJackServer(baseSystem)) {
                        daw.settingsAudioStatusMessage = (baseSystem.audio && baseSystem.audio->client)
                            ? "Bundled JACK connected."
                            : "Select devices or press Retry Audio.";
                    } else {
                        daw.settingsAudioStatusMessage = (baseSystem.audio && baseSystem.audio->client)
                            ? "JACK connected."
                            : "JACK offline. Press Retry Audio.";
                    }
                    ui.consumeClick = true;
                } else if (daw.settingsTab == 1) {
                    const bool jackOnline = baseSystem.audio && baseSystem.audio->client;
                    const bool managed = AudioSystemLogic::SupportsManagedJackServer(baseSystem);
                    if (cursorInRect(ui, settingsLayout.audioRetryBtn)) {
                        if (managed) {
                            if (applyManagedAudioSelection(baseSystem, prototypes, daw, dt, win)) {
                                daw.settingsAudioStatusMessage = "Audio started.";
                            } else {
                                daw.settingsAudioStatusMessage = "Audio retry failed. JACK still unavailable.";
                            }
                        } else {
                            AudioSystemLogic::InitializeAudio(baseSystem, prototypes, dt, win);
                            refreshAudioSettings();
                            if (baseSystem.audio && baseSystem.audio->client) {
                                daw.settingsAudioStatusMessage = "Audio started.";
                                applyAudioOutputSelection();
                                applyAudioInputSelection();
                            } else {
                                daw.settingsAudioStatusMessage = "Audio retry failed. JACK still unavailable.";
                            }
                        }
                    } else if (!jackOnline && !managed) {
                        daw.settingsAudioStatusMessage = "JACK offline. Press Retry Audio.";
                    } else if (cursorInRect(ui, settingsLayout.audioRefreshBtn)) {
                        refreshAudioSettings();
                        daw.settingsAudioStatusMessage = "Device list refreshed.";
                    } else if (cursorInRect(ui, settingsLayout.audioOutPrev)) {
                        if (cycleSelection(daw.settingsSelectedAudioOutput,
                                           static_cast<int>(daw.settingsAudioOutputDevices.size()),
                                           -1)) {
                            daw.settingsAudioOutputName =
                                daw.settingsAudioOutputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioOutput)];
                            applyAudioOutputSelection();
                        }
                    } else if (cursorInRect(ui, settingsLayout.audioOutNext)) {
                        if (cycleSelection(daw.settingsSelectedAudioOutput,
                                           static_cast<int>(daw.settingsAudioOutputDevices.size()),
                                           1)) {
                            daw.settingsAudioOutputName =
                                daw.settingsAudioOutputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioOutput)];
                            applyAudioOutputSelection();
                        }
                    } else if (cursorInRect(ui, settingsLayout.audioInPrev)) {
                        if (cycleSelection(daw.settingsSelectedAudioInput,
                                           static_cast<int>(daw.settingsAudioInputDevices.size()),
                                           -1)) {
                            daw.settingsAudioInputName =
                                daw.settingsAudioInputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioInput)];
                            applyAudioInputSelection();
                        }
                    } else if (cursorInRect(ui, settingsLayout.audioInNext)) {
                        if (cycleSelection(daw.settingsSelectedAudioInput,
                                           static_cast<int>(daw.settingsAudioInputDevices.size()),
                                           1)) {
                            daw.settingsAudioInputName =
                                daw.settingsAudioInputDeviceIds[static_cast<size_t>(daw.settingsSelectedAudioInput)];
                            applyAudioInputSelection();
                        }
                    }
                    ui.consumeClick = true;
                } else if (daw.themeCreateMode) {
                    if (cursorInRect(ui, settingsLayout.nameField)) {
                        daw.themeEditField = 0;
                    } else if (cursorInRect(ui, settingsLayout.bgField)) {
                        daw.themeEditField = 1;
                    } else if (cursorInRect(ui, settingsLayout.panelField)) {
                        daw.themeEditField = 2;
                    } else if (cursorInRect(ui, settingsLayout.buttonField)) {
                        daw.themeEditField = 3;
                    } else if (cursorInRect(ui, settingsLayout.pianoField)) {
                        daw.themeEditField = 4;
                    } else if (cursorInRect(ui, settingsLayout.pianoAccentField)) {
                        daw.themeEditField = 5;
                    } else if (cursorInRect(ui, settingsLayout.laneField)) {
                        daw.themeEditField = 6;
                    } else if (cursorInRect(ui, settingsLayout.backBtn)) {
                        daw.themeCreateMode = false;
                        daw.themeEditField = -1;
                        daw.themeStatusMessage.clear();
                    } else if (cursorInRect(ui, settingsLayout.saveBtn)) {
                        std::string status;
                        if (DawIOSystemLogic::SaveThemeFromDraft(baseSystem,
                                                                 daw.themeDraftName,
                                                                 daw.themeDraftBackgroundHex,
                                                                 daw.themeDraftPanelHex,
                                                                 daw.themeDraftButtonHex,
                                                                 daw.themeDraftPianoRollHex,
                                                                 daw.themeDraftPianoRollAccentHex,
                                                                 daw.themeDraftLaneHex,
                                                                 status)) {
                            DawIOSystemLogic::ApplyThemeByIndex(baseSystem, daw.settingsSelectedTheme, true);
                            daw.themeCreateMode = false;
                            daw.themeEditField = -1;
                        }
                        daw.themeStatusMessage = status;
                    }
                    ui.consumeClick = true;
                } else {
                    int clickedTheme = -1;
                    if (cursorInRect(ui, settingsLayout.listRect)) {
                        const int maxRows = std::max(0, static_cast<int>((settingsLayout.listRect.bottom - settingsLayout.listRect.top
                            - settingsLayout.listPad * 2.0f) / settingsLayout.listRowHeight));
                        for (int i = 0; i < static_cast<int>(daw.themes.size()) && i < maxRows; ++i) {
                            if (cursorInRect(ui, settingsThemeRowRect(settingsLayout, i))) {
                                clickedTheme = i;
                                break;
                            }
                        }
                    }
                    if (clickedTheme >= 0) {
                        daw.settingsSelectedTheme = clickedTheme;
                        daw.themeStatusMessage.clear();
                    } else if (cursorInRect(ui, settingsLayout.applyBtn)) {
                        if (DawIOSystemLogic::ApplyThemeByIndex(baseSystem, daw.settingsSelectedTheme, true)) {
                            daw.themeStatusMessage = "Theme applied.";
                        } else {
                            daw.themeStatusMessage = "Apply failed.";
                        }
                    } else if (cursorInRect(ui, settingsLayout.createBtn)) {
                        DawIOSystemLogic::BeginThemeDraftFromDefault(baseSystem);
                        daw.themeCreateMode = true;
                        daw.themeEditField = 0;
                        daw.themeStatusMessage.clear();
                        g_settingsKeyDown.clear();
                    } else if (cursorInRect(ui, settingsLayout.editBtn)) {
                        const DawThemePreset* preset = selectedTheme();
                        if (!preset || preset->name == "Default") {
                            daw.themeStatusMessage = "Default theme cannot be edited.";
                        } else {
                            daw.themeDraftName = preset->name;
                            daw.themeDraftBackgroundHex = DawIOSystemLogic::ThemeColorToHex(preset->background);
                            daw.themeDraftPanelHex = DawIOSystemLogic::ThemeColorToHex(preset->panel);
                            daw.themeDraftButtonHex = DawIOSystemLogic::ThemeColorToHex(preset->button);
                            daw.themeDraftPianoRollHex = DawIOSystemLogic::ThemeColorToHex(preset->pianoRoll);
                            daw.themeDraftPianoRollAccentHex = DawIOSystemLogic::ThemeColorToHex(preset->pianoRollAccent);
                            daw.themeDraftLaneHex = DawIOSystemLogic::ThemeColorToHex(preset->lane);
                            daw.themeCreateMode = true;
                            daw.themeEditField = 0;
                            daw.themeStatusMessage.clear();
                            g_settingsKeyDown.clear();
                        }
                    } else if (cursorInRect(ui, settingsLayout.deleteBtn)) {
                        if (selectedThemeDeleteProtected()) {
                            daw.themeStatusMessage = "Default, Default 2, and Default 3 cannot be deleted.";
                        } else {
                            std::string status;
                            if (!DawIOSystemLogic::RemoveThemeByIndex(baseSystem,
                                                                      daw.settingsSelectedTheme,
                                                                      status)) {
                                daw.themeStatusMessage = status.empty() ? "Delete failed." : status;
                            } else {
                                daw.themeStatusMessage = status;
                            }
                        }
                    }
                    ui.consumeClick = true;
                }
            } else if (cursorInRect(ui, settingsLayout.panelRect)) {
                ui.consumeClick = true;
            }

            if (daw.settingsTab == 0 && daw.themeCreateMode) {
                const bool shiftDown = isShiftDown(win);
                if (settingsKeyPressed(win, PlatformInput::Key::Tab)) {
                    int next = daw.themeEditField;
                    if (next < 0 || next > 6) next = 0;
                    daw.themeEditField = (next + 1) % 7;
                }
                if (settingsKeyPressed(win, PlatformInput::Key::Enter)
                    || settingsKeyPressed(win, PlatformInput::Key::KpEnter)) {
                    std::string status;
                    if (DawIOSystemLogic::SaveThemeFromDraft(baseSystem,
                                                             daw.themeDraftName,
                                                             daw.themeDraftBackgroundHex,
                                                             daw.themeDraftPanelHex,
                                                             daw.themeDraftButtonHex,
                                                             daw.themeDraftPianoRollHex,
                                                             daw.themeDraftPianoRollAccentHex,
                                                             daw.themeDraftLaneHex,
                                                             status)) {
                        DawIOSystemLogic::ApplyThemeByIndex(baseSystem, daw.settingsSelectedTheme, true);
                        daw.themeCreateMode = false;
                        daw.themeEditField = -1;
                    }
                    daw.themeStatusMessage = status;
                } else {
                    updateThemeDraftTyping(win, daw, shiftDown);
                }
            } else {
                daw.themeEditField = -1;
                g_settingsKeyDown.clear();
            }

            g_cmdEShortcutWasDown = false;
            g_cmdShiftMShortcutWasDown = false;
            g_cmdLShortcutWasDown = false;
            g_spaceShortcutWasDown = false;
            g_deleteShortcutWasDown = false;
            g_cmdPrevTakeShortcutWasDown = false;
            g_cmdNextTakeShortcutWasDown = false;
            g_rightMouseWasDown = (PlatformInput::IsMouseButtonDown(win, PlatformInput::MouseButton::Right));
            ApplyLaneResizeCursor(win, false);
            return;
        }

        auto closeAutomationParamMenu = [&]() {
            daw.automationParamMenuOpen = false;
            daw.automationParamMenuTrack = -1;
            daw.automationParamMenuHoverIndex = -1;
            daw.automationParamMenuLabels.clear();
        };

        if (daw.automationParamMenuOpen) {
            const AutomationParamMenuLayout paramMenuLayout = computeAutomationParamMenuLayout(daw, layout);
            if (!paramMenuLayout.valid) {
                closeAutomationParamMenu();
            } else {
                const bool buttonHit = cursorInRect(ui, paramMenuLayout.buttonRect);
                const bool menuHit = cursorInRect(ui, paramMenuLayout.menuRect);
                daw.automationParamMenuHoverIndex = -1;
                if (menuHit) {
                    float localY = static_cast<float>(ui.cursorY) - paramMenuLayout.menuRect.top - paramMenuLayout.padding;
                    int hover = static_cast<int>(localY / paramMenuLayout.rowHeight);
                    if (hover >= 0 && hover < static_cast<int>(daw.automationParamMenuLabels.size())) {
                        daw.automationParamMenuHoverIndex = hover;
                    }
                }
                if (ui.uiLeftPressed) {
                    if (menuHit && daw.automationParamMenuHoverIndex >= 0) {
                        int trackIndex = daw.automationParamMenuTrack;
                        int slot = daw.automationParamMenuHoverIndex;
                        closeAutomationParamMenu();
                        if (ui.pendingActionType.empty()) {
                            ui.pendingActionType = "DawAutomationTrack";
                            ui.pendingActionKey = "target_param_pick";
                            ui.pendingActionValue = std::to_string(trackIndex) + ":" + std::to_string(slot);
                        }
                        ui.consumeClick = true;
                    } else if (buttonHit) {
                        ui.consumeClick = true;
                    } else {
                        closeAutomationParamMenu();
                        ui.consumeClick = true;
                    }
                } else if (menuHit || buttonHit) {
                    ui.consumeClick = true;
                }
            }
        }
    }
}
