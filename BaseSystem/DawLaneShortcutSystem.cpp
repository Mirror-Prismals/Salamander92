#pragma once

namespace DawLaneInputSystemLogic {
    namespace {
        bool exportKeyPressed(PlatformWindowHandle win, PlatformInput::Key key) {
            return keyPressedEdge(win, key, g_exportKeyDown);
        }

        bool settingsKeyPressed(PlatformWindowHandle win, PlatformInput::Key key) {
            return keyPressedEdge(win, key, g_settingsKeyDown);
        }

        void appendStemChar(std::string& stem, char c) {
            constexpr size_t kMaxStemChars = 48;
            if (stem.size() >= kMaxStemChars) return;
            stem.push_back(c);
        }

        void appendThemeNameChar(std::string& value, char c) {
            constexpr size_t kMaxChars = 40;
            if (value.size() >= kMaxChars) return;
            value.push_back(c);
        }

        void appendThemeHexChar(std::string& value, char c) {
            constexpr size_t kMaxChars = 8;
            if (value.size() >= kMaxChars) return;
            value.push_back(c);
        }

        void updateStemNameTyping(PlatformWindowHandle win, DawContext& daw, bool shiftDown) {
            if (!win) return;
            if (daw.exportSelectedStem < 0 || daw.exportSelectedStem >= DawContext::kBusCount) return;
            std::string& stem = daw.exportStemNames[static_cast<size_t>(daw.exportSelectedStem)];
            static const std::array<PlatformInput::Key, 10> kDigitKeys = {
                PlatformInput::Key::Num0, PlatformInput::Key::Num1, PlatformInput::Key::Num2, PlatformInput::Key::Num3, PlatformInput::Key::Num4,
                PlatformInput::Key::Num5, PlatformInput::Key::Num6, PlatformInput::Key::Num7, PlatformInput::Key::Num8, PlatformInput::Key::Num9
            };
            static const std::array<PlatformInput::Key, 26> kAlphaKeys = {
                PlatformInput::Key::A, PlatformInput::Key::B, PlatformInput::Key::C, PlatformInput::Key::D, PlatformInput::Key::E,
                PlatformInput::Key::F, PlatformInput::Key::G, PlatformInput::Key::H, PlatformInput::Key::I, PlatformInput::Key::J,
                PlatformInput::Key::K, PlatformInput::Key::L, PlatformInput::Key::M, PlatformInput::Key::N, PlatformInput::Key::O,
                PlatformInput::Key::P, PlatformInput::Key::Q, PlatformInput::Key::R, PlatformInput::Key::S, PlatformInput::Key::T,
                PlatformInput::Key::U, PlatformInput::Key::V, PlatformInput::Key::W, PlatformInput::Key::X, PlatformInput::Key::Y,
                PlatformInput::Key::Z
            };

            if (exportKeyPressed(win, PlatformInput::Key::Backspace)) {
                if (!stem.empty()) stem.pop_back();
            }
            if (exportKeyPressed(win, PlatformInput::Key::Space)) {
                appendStemChar(stem, '_');
            }
            if (exportKeyPressed(win, PlatformInput::Key::Minus)) {
                appendStemChar(stem, shiftDown ? '_' : '-');
            }
            if (exportKeyPressed(win, PlatformInput::Key::Period)) {
                appendStemChar(stem, '.');
            }

            for (size_t i = 0; i < kDigitKeys.size(); ++i) {
                if (!exportKeyPressed(win, kDigitKeys[i])) continue;
                appendStemChar(stem, static_cast<char>('0' + static_cast<char>(i)));
            }
            for (size_t i = 0; i < kAlphaKeys.size(); ++i) {
                if (!exportKeyPressed(win, kAlphaKeys[i])) continue;
                char c = static_cast<char>('a' + static_cast<char>(i));
                if (shiftDown) c = static_cast<char>(c - ('a' - 'A'));
                appendStemChar(stem, c);
            }
        }

        void updateThemeDraftTyping(PlatformWindowHandle win, DawContext& daw, bool shiftDown) {
            if (!win) return;
            if (daw.themeEditField < 0 || daw.themeEditField > 6) return;
            static const std::array<PlatformInput::Key, 10> kDigitKeys = {
                PlatformInput::Key::Num0, PlatformInput::Key::Num1, PlatformInput::Key::Num2, PlatformInput::Key::Num3, PlatformInput::Key::Num4,
                PlatformInput::Key::Num5, PlatformInput::Key::Num6, PlatformInput::Key::Num7, PlatformInput::Key::Num8, PlatformInput::Key::Num9
            };
            static const std::array<PlatformInput::Key, 26> kAlphaKeys = {
                PlatformInput::Key::A, PlatformInput::Key::B, PlatformInput::Key::C, PlatformInput::Key::D, PlatformInput::Key::E,
                PlatformInput::Key::F, PlatformInput::Key::G, PlatformInput::Key::H, PlatformInput::Key::I, PlatformInput::Key::J,
                PlatformInput::Key::K, PlatformInput::Key::L, PlatformInput::Key::M, PlatformInput::Key::N, PlatformInput::Key::O,
                PlatformInput::Key::P, PlatformInput::Key::Q, PlatformInput::Key::R, PlatformInput::Key::S, PlatformInput::Key::T,
                PlatformInput::Key::U, PlatformInput::Key::V, PlatformInput::Key::W, PlatformInput::Key::X, PlatformInput::Key::Y,
                PlatformInput::Key::Z
            };
            static const std::array<PlatformInput::Key, 6> kHexLetterKeys = {
                PlatformInput::Key::A, PlatformInput::Key::B, PlatformInput::Key::C,
                PlatformInput::Key::D, PlatformInput::Key::E, PlatformInput::Key::F
            };

            std::string* target = nullptr;
            bool hexField = false;
            if (daw.themeEditField == 0) {
                target = &daw.themeDraftName;
            } else if (daw.themeEditField == 1) {
                target = &daw.themeDraftBackgroundHex;
                hexField = true;
            } else if (daw.themeEditField == 2) {
                target = &daw.themeDraftPanelHex;
                hexField = true;
            } else if (daw.themeEditField == 3) {
                target = &daw.themeDraftButtonHex;
                hexField = true;
            } else if (daw.themeEditField == 4) {
                target = &daw.themeDraftPianoRollHex;
                hexField = true;
            } else if (daw.themeEditField == 5) {
                target = &daw.themeDraftPianoRollAccentHex;
                hexField = true;
            } else if (daw.themeEditField == 6) {
                target = &daw.themeDraftLaneHex;
                hexField = true;
            }
            if (!target) return;

            if (settingsKeyPressed(win, PlatformInput::Key::Backspace)) {
                if (!target->empty()) target->pop_back();
            }

            if (!hexField) {
                if (settingsKeyPressed(win, PlatformInput::Key::Space)) {
                    appendThemeNameChar(*target, ' ');
                }
                if (settingsKeyPressed(win, PlatformInput::Key::Minus)) {
                    appendThemeNameChar(*target, shiftDown ? '_' : '-');
                }
                if (settingsKeyPressed(win, PlatformInput::Key::Period)) {
                    appendThemeNameChar(*target, '.');
                }
                for (size_t i = 0; i < kDigitKeys.size(); ++i) {
                    if (!settingsKeyPressed(win, kDigitKeys[i])) continue;
                    appendThemeNameChar(*target, static_cast<char>('0' + static_cast<char>(i)));
                }
                for (size_t i = 0; i < kAlphaKeys.size(); ++i) {
                    if (!settingsKeyPressed(win, kAlphaKeys[i])) continue;
                    char c = static_cast<char>('a' + static_cast<char>(i));
                    if (shiftDown) c = static_cast<char>(c - ('a' - 'A'));
                    appendThemeNameChar(*target, c);
                }
                return;
            }

            for (size_t i = 0; i < kDigitKeys.size(); ++i) {
                if (!settingsKeyPressed(win, kDigitKeys[i])) continue;
                appendThemeHexChar(*target, static_cast<char>('0' + static_cast<char>(i)));
            }
            for (size_t i = 0; i < kHexLetterKeys.size(); ++i) {
                if (!settingsKeyPressed(win, kHexLetterKeys[i])) continue;
                char c = static_cast<char>('A' + static_cast<char>(i));
                appendThemeHexChar(*target, c);
            }
        }

        uint64_t gridStepSamples(const DawContext& daw, double secondsPerScreen) {
            double bpm = daw.bpm.load(std::memory_order_relaxed);
            if (bpm <= 0.0) bpm = 120.0;
            double secondsPerBeat = 60.0 / bpm;
            double gridSeconds = DawLaneTimelineSystemLogic::GridSecondsForZoom(secondsPerScreen, secondsPerBeat);
            if (gridSeconds <= 0.0) return 1;
            double sampleRate = (daw.sampleRate > 0.0) ? daw.sampleRate : 44100.0;
            return std::max<uint64_t>(1, static_cast<uint64_t>(std::llround(gridSeconds * sampleRate)));
        }

        uint64_t snapSampleToGrid(const DawContext& daw, double secondsPerScreen, int64_t sample) {
            if (sample < 0) sample = 0;
            uint64_t step = gridStepSamples(daw, secondsPerScreen);
            if (step == 0) return static_cast<uint64_t>(sample);
            return (static_cast<uint64_t>(sample) / step) * step;
        }

        uint64_t computeRebaseShiftSamples(const DawContext& daw, int64_t negativeSample) {
            if (negativeSample >= 0) return 0;
            double sampleRate = (daw.sampleRate > 0.0) ? daw.sampleRate : 44100.0;
            double bpm = daw.bpm.load(std::memory_order_relaxed);
            if (bpm <= 0.0) bpm = 120.0;
            uint64_t barSamples = std::max<uint64_t>(1,
                static_cast<uint64_t>(std::llround((60.0 / bpm) * 4.0 * sampleRate)));
            uint64_t need = static_cast<uint64_t>(-negativeSample) + barSamples * 2ull;
            uint64_t shift = ((need + barSamples - 1ull) / barSamples) * barSamples;
            if (shift == 0) shift = barSamples;
            return shift;
        }

        uint64_t sampleFromCursorX(BaseSystem& baseSystem,
                                   DawContext& daw,
                                   float laneLeft,
                                   float laneRight,
                                   double secondsPerScreen,
                                   double cursorX,
                                   bool snap) {
            double offsetSamples = static_cast<double>(daw.timelineOffsetSamples);
            double windowSamples = secondsPerScreen * static_cast<double>(daw.sampleRate);
            if (windowSamples <= 0.0) windowSamples = 1.0;
            double t = (laneRight > laneLeft)
                ? (cursorX - laneLeft) / static_cast<double>(laneRight - laneLeft)
                : 0.0;
            t = std::clamp(t, 0.0, 1.0);
            int64_t sample = static_cast<int64_t>(std::llround(offsetSamples + t * windowSamples));
            if (sample < 0) {
                uint64_t shiftSamples = computeRebaseShiftSamples(daw, sample);
                DawTimelineRebaseLogic::ShiftTimelineRight(baseSystem, shiftSamples);
                sample += static_cast<int64_t>(shiftSamples);
            }
            if (snap) return snapSampleToGrid(daw, secondsPerScreen, sample);
            return static_cast<uint64_t>(sample);
        }

        int laneIndexFromCursorYClamped(float y, float startY, float rowSpan, int laneCount) {
            if (laneCount <= 0) return -1;
            int idx = static_cast<int>(std::floor((y - startY) / rowSpan + 0.5f));
            return std::clamp(idx, 0, laneCount - 1);
        }

        PlatformInput::CursorHandle loadTrimCursorImage(const char* path, int hotX, int hotY) {
            int width = 0;
            int height = 0;
            int channels = 0;
            unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
            if (!data) return nullptr;
            int outW = width / 2;
            int outH = height / 2;
            std::vector<unsigned char> scaled;
            if (outW > 0 && outH > 0) {
                scaled.resize(static_cast<size_t>(outW * outH * 4));
                for (int y = 0; y < outH; ++y) {
                    for (int x = 0; x < outW; ++x) {
                        int srcX = x * 2;
                        int srcY = y * 2;
                        int srcIdx = (srcY * width + srcX) * 4;
                        int dstIdx = (y * outW + x) * 4;
                        scaled[static_cast<size_t>(dstIdx + 0)] = data[srcIdx + 0];
                        scaled[static_cast<size_t>(dstIdx + 1)] = data[srcIdx + 1];
                        scaled[static_cast<size_t>(dstIdx + 2)] = data[srcIdx + 2];
                        scaled[static_cast<size_t>(dstIdx + 3)] = data[srcIdx + 3];
                    }
                }
            }

            PlatformInput::CursorImage image;
            image.width = (outW > 0) ? outW : width;
            image.height = (outH > 0) ? outH : height;
            image.pixels = (outW > 0) ? scaled.data() : data;
            PlatformInput::CursorHandle cursor = PlatformInput::CreateCursor(image, hotX / 2, hotY / 2);
            stbi_image_free(data);
            return cursor;
        }

        PlatformInput::CursorHandle ensureLaneTrimCursor() {
            if (!g_laneTrimCursorLoaded) {
                g_laneTrimCursor = loadTrimCursorImage("Procedures/assets/resize_a_horizontal.png", 16, 16);
                if (!g_laneTrimCursor) {
                    g_laneTrimCursor = PlatformInput::CreateStandardCursor(PlatformInput::StandardCursor::HorizontalResize);
                }
                g_laneTrimCursorLoaded = true;
            }
            return g_laneTrimCursor;
        }

        void applySplitToMidiClip(MidiClip& clip, uint64_t newStart, uint64_t newLength) {
            uint64_t oldStart = clip.startSample;
            uint64_t newEnd = newStart + newLength;
            std::vector<MidiNote> trimmed;
            trimmed.reserve(clip.notes.size());
            for (const auto& note : clip.notes) {
                if (note.length == 0) continue;
                uint64_t noteStart = oldStart + note.startSample;
                uint64_t noteEnd = noteStart + note.length;
                if (noteEnd <= newStart || noteStart >= newEnd) continue;
                uint64_t clippedStart = std::max(noteStart, newStart);
                uint64_t clippedEnd = std::min(noteEnd, newEnd);
                if (clippedEnd <= clippedStart) continue;
                MidiNote out = note;
                out.startSample = clippedStart - newStart;
                out.length = clippedEnd - clippedStart;
                trimmed.push_back(out);
            }
            clip.startSample = newStart;
            clip.length = newLength;
            clip.notes = std::move(trimmed);
        }

        void trimMidiClipsForNewClip(MidiTrack& track, const MidiClip& clip) {
            if (clip.length == 0) return;
            uint64_t newStart = clip.startSample;
            uint64_t newEnd = clip.startSample + clip.length;
            std::vector<MidiClip> updated;
            updated.reserve(track.clips.size() + 1);
            for (const auto& existing : track.clips) {
                if (existing.length == 0) continue;
                uint64_t exStart = existing.startSample;
                uint64_t exEnd = existing.startSample + existing.length;
                if (exEnd <= newStart || exStart >= newEnd) {
                    updated.push_back(existing);
                    continue;
                }
                if (newStart <= exStart && newEnd >= exEnd) {
                    continue;
                }
                if (newStart > exStart && newEnd < exEnd) {
                    MidiClip left = existing;
                    MidiClip right = existing;
                    applySplitToMidiClip(left, exStart, newStart - exStart);
                    applySplitToMidiClip(right, newEnd, exEnd - newEnd);
                    if (left.length > 0) updated.push_back(std::move(left));
                    if (right.length > 0) updated.push_back(std::move(right));
                } else if (newStart <= exStart) {
                    MidiClip right = existing;
                    applySplitToMidiClip(right, newEnd, exEnd - newEnd);
                    if (right.length > 0) updated.push_back(std::move(right));
                } else {
                    MidiClip left = existing;
                    applySplitToMidiClip(left, exStart, newStart - exStart);
                    if (left.length > 0) updated.push_back(std::move(left));
                }
            }
            track.clips = std::move(updated);
        }

        void sortMidiClipsByStart(std::vector<MidiClip>& clips) {
            std::sort(clips.begin(), clips.end(), [](const MidiClip& a, const MidiClip& b) {
                if (a.startSample == b.startSample) return a.length < b.length;
                return a.startSample < b.startSample;
            });
        }

        float takeRowHeight(float laneHalfH) {
            float laneHeight = laneHalfH * 2.0f;
            return std::clamp(laneHeight * 0.26f, kTakeRowMinHeight, kTakeRowMaxHeight);
        }

        bool splitSelectedAudioClipAtPlayhead(BaseSystem& baseSystem, DawContext& daw, uint64_t splitSample) {
            int trackIndex = daw.selectedClipTrack;
            int clipIndex = daw.selectedClipIndex;
            if (trackIndex < 0 || clipIndex < 0 || trackIndex >= static_cast<int>(daw.tracks.size())) return false;
            DawTrack& track = daw.tracks[static_cast<size_t>(trackIndex)];
            if (clipIndex >= static_cast<int>(track.clips.size())) return false;

            const DawClip& src = track.clips[static_cast<size_t>(clipIndex)];
            uint64_t clipStart = src.startSample;
            uint64_t clipEnd = src.startSample + src.length;
            if (src.length == 0 || splitSample <= clipStart || splitSample >= clipEnd) return false;

            DawClip left = src;
            DawClip right = src;
            left.length = splitSample - clipStart;
            right.startSample = splitSample;
            right.length = clipEnd - splitSample;
            right.sourceOffset = src.sourceOffset + (splitSample - clipStart);

            track.clips[static_cast<size_t>(clipIndex)] = left;
            track.clips.insert(track.clips.begin() + clipIndex + 1, right);
            DawClipSystemLogic::RebuildTrackCacheFromClips(daw, track);
            daw.selectedClipTrack = trackIndex;
            daw.selectedClipIndex = clipIndex + 1;
            daw.selectedAutomationClipTrack = -1;
            daw.selectedAutomationClipIndex = -1;
            if (baseSystem.midi) {
                baseSystem.midi->selectedClipTrack = -1;
                baseSystem.midi->selectedClipIndex = -1;
            }
            return true;
        }

        bool splitSelectedMidiClipAtPlayhead(BaseSystem& baseSystem, DawContext& daw, uint64_t splitSample) {
            if (!baseSystem.midi) return false;
            MidiContext& midi = *baseSystem.midi;
            int trackIndex = midi.selectedClipTrack;
            int clipIndex = midi.selectedClipIndex;
            if (trackIndex < 0 || clipIndex < 0 || trackIndex >= static_cast<int>(midi.tracks.size())) return false;
            MidiTrack& track = midi.tracks[static_cast<size_t>(trackIndex)];
            if (clipIndex >= static_cast<int>(track.clips.size())) return false;

            const MidiClip& src = track.clips[static_cast<size_t>(clipIndex)];
            uint64_t clipStart = src.startSample;
            uint64_t clipEnd = src.startSample + src.length;
            if (src.length == 0 || splitSample <= clipStart || splitSample >= clipEnd) return false;

            MidiClip left = src;
            MidiClip right = src;
            applySplitToMidiClip(left, clipStart, splitSample - clipStart);
            applySplitToMidiClip(right, splitSample, clipEnd - splitSample);
            track.clips[static_cast<size_t>(clipIndex)] = left;
            track.clips.insert(track.clips.begin() + clipIndex + 1, right);

            midi.selectedTrackIndex = trackIndex;
            midi.selectedClipTrack = trackIndex;
            midi.selectedClipIndex = clipIndex + 1;
            daw.selectedClipTrack = -1;
            daw.selectedClipIndex = -1;
            daw.selectedAutomationClipTrack = -1;
            daw.selectedAutomationClipIndex = -1;
            return true;
        }

        bool createMidiClipsFromTimelineSelection(BaseSystem& baseSystem, DawContext& daw) {
            if (!baseSystem.midi) return false;
            MidiContext& midi = *baseSystem.midi;
            if (!daw.timelineSelectionActive) return false;
            if (midi.tracks.empty()) return false;

            uint64_t selStart = std::min(daw.timelineSelectionStartSample, daw.timelineSelectionEndSample);
            uint64_t selEnd = std::max(daw.timelineSelectionStartSample, daw.timelineSelectionEndSample);
            if (selEnd <= selStart) return false;

            int laneMin = std::min(daw.timelineSelectionStartLane, daw.timelineSelectionEndLane);
            int laneMax = std::max(daw.timelineSelectionStartLane, daw.timelineSelectionEndLane);
            if (laneMax < 0) return false;

            std::vector<int> targetTracks;
            if (!daw.laneOrder.empty()) {
                laneMin = std::max(0, laneMin);
                laneMax = std::min(laneMax, static_cast<int>(daw.laneOrder.size()) - 1);
                for (int lane = laneMin; lane <= laneMax; ++lane) {
                    const auto& entry = daw.laneOrder[static_cast<size_t>(lane)];
                    if (entry.type != 1) continue;
                    if (entry.trackIndex < 0 || entry.trackIndex >= static_cast<int>(midi.tracks.size())) continue;
                    if (std::find(targetTracks.begin(), targetTracks.end(), entry.trackIndex) == targetTracks.end()) {
                        targetTracks.push_back(entry.trackIndex);
                    }
                }
            } else {
                int audioTrackCount = static_cast<int>(daw.tracks.size());
                int startTrack = std::max(0, laneMin - audioTrackCount);
                int endTrack = std::min(static_cast<int>(midi.tracks.size()) - 1, laneMax - audioTrackCount);
                for (int t = startTrack; t <= endTrack; ++t) {
                    targetTracks.push_back(t);
                }
            }

            if (targetTracks.empty()) return false;

            for (int trackIdx : targetTracks) {
                const MidiTrack& track = midi.tracks[static_cast<size_t>(trackIdx)];
                for (const auto& clip : track.clips) {
                    if (clip.length == 0) continue;
                    uint64_t clipStart = clip.startSample;
                    uint64_t clipEnd = clip.startSample + clip.length;
                    bool overlap = !(clipEnd <= selStart || clipStart >= selEnd);
                    if (overlap) return false;
                }
            }

            int firstTrack = -1;
            int firstClipIndex = -1;
            for (int trackIdx : targetTracks) {
                MidiTrack& track = midi.tracks[static_cast<size_t>(trackIdx)];
                MidiClip newClip{};
                newClip.startSample = selStart;
                newClip.length = selEnd - selStart;
                track.clips.push_back(newClip);
                std::sort(track.clips.begin(), track.clips.end(), [](const MidiClip& a, const MidiClip& b) {
                    if (a.startSample == b.startSample) return a.length < b.length;
                    return a.startSample < b.startSample;
                });
                if (firstTrack < 0) {
                    firstTrack = trackIdx;
                    for (size_t i = 0; i < track.clips.size(); ++i) {
                        if (track.clips[i].startSample == newClip.startSample
                            && track.clips[i].length == newClip.length
                            && track.clips[i].notes.empty()) {
                            firstClipIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }
            }

            if (firstTrack >= 0) {
                midi.selectedTrackIndex = firstTrack;
                midi.selectedClipTrack = firstTrack;
                midi.selectedClipIndex = firstClipIndex;
                daw.selectedClipTrack = -1;
                daw.selectedClipIndex = -1;
                daw.selectedAutomationClipTrack = -1;
                daw.selectedAutomationClipIndex = -1;
            }
            return true;
        }

        bool createAutomationClipsFromTimelineSelection(BaseSystem& baseSystem, DawContext& daw) {
            if (!daw.timelineSelectionActive) return false;
            if (daw.automationTracks.empty()) return false;

            uint64_t selStart = std::min(daw.timelineSelectionStartSample, daw.timelineSelectionEndSample);
            uint64_t selEnd = std::max(daw.timelineSelectionStartSample, daw.timelineSelectionEndSample);
            if (selEnd <= selStart) return false;

            int laneMin = std::min(daw.timelineSelectionStartLane, daw.timelineSelectionEndLane);
            int laneMax = std::max(daw.timelineSelectionStartLane, daw.timelineSelectionEndLane);
            if (laneMax < 0) return false;

            std::vector<int> targetTracks;
            if (!daw.laneOrder.empty()) {
                laneMin = std::max(0, laneMin);
                laneMax = std::min(laneMax, static_cast<int>(daw.laneOrder.size()) - 1);
                for (int lane = laneMin; lane <= laneMax; ++lane) {
                    const auto& entry = daw.laneOrder[static_cast<size_t>(lane)];
                    if (entry.type != 2) continue;
                    if (entry.trackIndex < 0
                        || entry.trackIndex >= static_cast<int>(daw.automationTracks.size())) continue;
                    if (std::find(targetTracks.begin(), targetTracks.end(), entry.trackIndex) == targetTracks.end()) {
                        targetTracks.push_back(entry.trackIndex);
                    }
                }
            } else {
                int audioTrackCount = static_cast<int>(daw.tracks.size());
                int midiTrackCount = baseSystem.midi ? static_cast<int>(baseSystem.midi->tracks.size()) : 0;
                int base = audioTrackCount + midiTrackCount;
                int startTrack = std::max(0, laneMin - base);
                int endTrack = std::min(static_cast<int>(daw.automationTracks.size()) - 1, laneMax - base);
                for (int t = startTrack; t <= endTrack; ++t) {
                    targetTracks.push_back(t);
                }
            }

            if (targetTracks.empty()) return false;

            for (int trackIdx : targetTracks) {
                const AutomationTrack& track = daw.automationTracks[static_cast<size_t>(trackIdx)];
                for (const auto& clip : track.clips) {
                    if (clip.length == 0) continue;
                    uint64_t clipStart = clip.startSample;
                    uint64_t clipEnd = clip.startSample + clip.length;
                    bool overlap = !(clipEnd <= selStart || clipStart >= selEnd);
                    if (overlap) return false;
                }
            }

            int firstTrack = -1;
            int firstClipIndex = -1;
            for (int trackIdx : targetTracks) {
                AutomationTrack& track = daw.automationTracks[static_cast<size_t>(trackIdx)];
                AutomationClip newClip{};
                newClip.startSample = selStart;
                newClip.length = selEnd - selStart;
                track.clips.push_back(newClip);
                std::sort(track.clips.begin(), track.clips.end(), [](const AutomationClip& a, const AutomationClip& b) {
                    if (a.startSample == b.startSample) return a.length < b.length;
                    return a.startSample < b.startSample;
                });
                if (firstTrack < 0) {
                    firstTrack = trackIdx;
                    for (size_t i = 0; i < track.clips.size(); ++i) {
                        if (track.clips[i].startSample == newClip.startSample
                            && track.clips[i].length == newClip.length
                            && track.clips[i].points.empty()) {
                            firstClipIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }
            }

            if (firstTrack >= 0) {
                daw.selectedAutomationClipTrack = firstTrack;
                daw.selectedAutomationClipIndex = firstClipIndex;
                daw.selectedClipTrack = -1;
                daw.selectedClipIndex = -1;
                if (baseSystem.midi) {
                    baseSystem.midi->selectedClipTrack = -1;
                    baseSystem.midi->selectedClipIndex = -1;
                }
            }
            return true;
        }

        bool deleteSelectedAudioClip(BaseSystem& baseSystem, DawContext& daw) {
            int trackIndex = daw.selectedClipTrack;
            int clipIndex = daw.selectedClipIndex;
            if (trackIndex < 0 || clipIndex < 0 || trackIndex >= static_cast<int>(daw.tracks.size())) return false;
            DawTrack& track = daw.tracks[static_cast<size_t>(trackIndex)];
            if (clipIndex >= static_cast<int>(track.clips.size())) return false;
            track.clips.erase(track.clips.begin() + clipIndex);
            DawClipSystemLogic::RebuildTrackCacheFromClips(daw, track);
            daw.selectedClipTrack = -1;
            daw.selectedClipIndex = -1;
            daw.selectedAutomationClipTrack = -1;
            daw.selectedAutomationClipIndex = -1;
            daw.timelineSelectionActive = false;
            daw.timelineSelectionDragActive = false;
            return true;
        }

        bool deleteSelectedMidiClip(BaseSystem& baseSystem, DawContext& daw) {
            if (!baseSystem.midi) return false;
            MidiContext& midi = *baseSystem.midi;
            int trackIndex = midi.selectedClipTrack;
            int clipIndex = midi.selectedClipIndex;
            if (trackIndex < 0 || clipIndex < 0 || trackIndex >= static_cast<int>(midi.tracks.size())) return false;
            MidiTrack& track = midi.tracks[static_cast<size_t>(trackIndex)];
            if (clipIndex >= static_cast<int>(track.clips.size())) return false;
            track.clips.erase(track.clips.begin() + clipIndex);
            midi.selectedClipTrack = -1;
            midi.selectedClipIndex = -1;
            daw.selectedClipTrack = -1;
            daw.selectedClipIndex = -1;
            daw.selectedAutomationClipTrack = -1;
            daw.selectedAutomationClipIndex = -1;
            daw.timelineSelectionActive = false;
            daw.timelineSelectionDragActive = false;
            return true;
        }

        bool deleteSelectedAutomationClip(DawContext& daw) {
            int trackIndex = daw.selectedAutomationClipTrack;
            int clipIndex = daw.selectedAutomationClipIndex;
            if (trackIndex < 0 || clipIndex < 0 || trackIndex >= static_cast<int>(daw.automationTracks.size())) {
                return false;
            }
            AutomationTrack& track = daw.automationTracks[static_cast<size_t>(trackIndex)];
            if (clipIndex >= static_cast<int>(track.clips.size())) return false;
            track.clips.erase(track.clips.begin() + clipIndex);
            daw.selectedAutomationClipTrack = -1;
            daw.selectedAutomationClipIndex = -1;
            daw.timelineSelectionActive = false;
            daw.timelineSelectionDragActive = false;
            return true;
        }

    }

    void UpdateDawLaneShortcuts(BaseSystem& baseSystem, std::vector<Entity>&, float, PlatformWindowHandle win) {
        if (!baseSystem.ui || !baseSystem.daw || !baseSystem.level || !win) return;
        UIContext& ui = *baseSystem.ui;
        if (!ui.active || ui.loadingActive) return;
        if (baseSystem.midi && baseSystem.midi->pianoRollActive) return;
        if (!DawLaneTimelineSystemLogic::hasDawUiWorld(*baseSystem.level)) return;

        DawContext& daw = *baseSystem.daw;
        const bool exportInProgress = daw.exportInProgress.load(std::memory_order_relaxed);
        if (daw.exportMenuOpen || exportInProgress || daw.settingsMenuOpen) return;

        bool cmdDownNow = isCommandDown(win);
        bool shiftDownNow = isShiftDown(win);
        bool cmdEShortcutNow = cmdDownNow
            && !shiftDownNow
            && PlatformInput::IsKeyDown(win, PlatformInput::Key::E);
        if (cmdEShortcutNow && !g_cmdEShortcutWasDown) {
            uint64_t splitSample = daw.playheadSample.load(std::memory_order_relaxed);
            if (!splitSelectedAudioClipAtPlayhead(baseSystem, daw, splitSample)) {
                splitSelectedMidiClipAtPlayhead(baseSystem, daw, splitSample);
            }
        }
        g_cmdEShortcutWasDown = cmdEShortcutNow;

        bool cmdShiftMShortcutNow = cmdDownNow
            && shiftDownNow
            && PlatformInput::IsKeyDown(win, PlatformInput::Key::M);
        if (cmdShiftMShortcutNow && !g_cmdShiftMShortcutWasDown) {
            bool createdMidi = createMidiClipsFromTimelineSelection(baseSystem, daw);
            bool createdAutomation = createAutomationClipsFromTimelineSelection(baseSystem, daw);
            (void)createdMidi;
            (void)createdAutomation;
        }
        g_cmdShiftMShortcutWasDown = cmdShiftMShortcutNow;

        bool cmdLShortcutNow = cmdDownNow
            && !shiftDownNow
            && PlatformInput::IsKeyDown(win, PlatformInput::Key::L);
        if (cmdLShortcutNow && !g_cmdLShortcutWasDown) {
            if (daw.timelineSelectionActive) {
                uint64_t selStart = std::min(daw.timelineSelectionStartSample, daw.timelineSelectionEndSample);
                uint64_t selEnd = std::max(daw.timelineSelectionStartSample, daw.timelineSelectionEndSample);
                if (selEnd > selStart) {
                    daw.loopStartSamples = selStart;
                    daw.loopEndSamples = selEnd;
                    daw.loopEnabled.store(true, std::memory_order_relaxed);
                }
            }
        }
        g_cmdLShortcutWasDown = cmdLShortcutNow;

        bool cmdPrevTakeShortcutNow = cmdDownNow
            && !shiftDownNow
            && PlatformInput::IsKeyDown(win, PlatformInput::Key::LeftBracket);
        bool cmdNextTakeShortcutNow = cmdDownNow
            && !shiftDownNow
            && PlatformInput::IsKeyDown(win, PlatformInput::Key::RightBracket);
        auto updateTimelineSelectionForClip = [&](uint64_t start, uint64_t length, int laneType, int laneTrack) {
            int laneIndex = -1;
            if (!daw.laneOrder.empty()) {
                for (size_t i = 0; i < daw.laneOrder.size(); ++i) {
                    const auto& entry = daw.laneOrder[i];
                    if (entry.type == laneType && entry.trackIndex == laneTrack) {
                        laneIndex = static_cast<int>(i);
                        break;
                    }
                }
            } else {
                laneIndex = (laneType == 0)
                    ? laneTrack
                    : static_cast<int>(daw.tracks.size()) + laneTrack;
            }
            if (laneIndex < 0) laneIndex = 0;
            daw.timelineSelectionStartSample = start;
            daw.timelineSelectionEndSample = start + length;
            daw.timelineSelectionStartLane = laneIndex;
            daw.timelineSelectionEndLane = laneIndex;
            daw.timelineSelectionAnchorSample = start;
            daw.timelineSelectionAnchorLane = laneIndex;
            daw.timelineSelectionActive = (length > 0);
            daw.timelineSelectionDragActive = false;
            daw.timelineSelectionFromPlayhead = false;
        };
        auto applyAudioCompFromTake = [&](int trackIndex,
                                          int takeIndex,
                                          uint64_t startSample,
                                          uint64_t endSample) -> bool {
            if (trackIndex < 0 || trackIndex >= static_cast<int>(daw.tracks.size())) return false;
            DawTrack& track = daw.tracks[static_cast<size_t>(trackIndex)];
            if (takeIndex < 0 || takeIndex >= static_cast<int>(track.loopTakeClips.size())) return false;
            const DawClip& take = track.loopTakeClips[static_cast<size_t>(takeIndex)];
            if (take.length == 0) return false;
            uint64_t selStart = std::min(startSample, endSample);
            uint64_t selEnd = std::max(startSample, endSample);
            uint64_t takeStart = take.startSample;
            uint64_t takeEnd = take.startSample + take.length;
            uint64_t clipStart = std::max(selStart, takeStart);
            uint64_t clipEnd = std::min(selEnd, takeEnd);
            if (clipEnd <= clipStart) return false;
            DawClip compClip = take;
            compClip.startSample = clipStart;
            compClip.length = clipEnd - clipStart;
            compClip.sourceOffset = take.sourceOffset + (clipStart - takeStart);
            DawClipSystemLogic::TrimClipsForNewClip(track, compClip);
            track.clips.push_back(compClip);
            std::sort(track.clips.begin(), track.clips.end(), [](const DawClip& a, const DawClip& b) {
                if (a.startSample == b.startSample) return a.sourceOffset < b.sourceOffset;
                return a.startSample < b.startSample;
            });
            int selectedIndex = -1;
            for (int i = static_cast<int>(track.clips.size()) - 1; i >= 0; --i) {
                const DawClip& candidate = track.clips[static_cast<size_t>(i)];
                if (candidate.audioId == compClip.audioId
                    && candidate.startSample == compClip.startSample
                    && candidate.length == compClip.length
                    && candidate.sourceOffset == compClip.sourceOffset
                    && candidate.takeId == compClip.takeId) {
                    selectedIndex = i;
                    break;
                }
            }
            DawClipSystemLogic::RebuildTrackCacheFromClips(daw, track);
            daw.selectedClipTrack = trackIndex;
            daw.selectedClipIndex = selectedIndex;
            daw.selectedAutomationClipTrack = -1;
            daw.selectedAutomationClipIndex = -1;
            daw.selectedLaneType = 0;
            daw.selectedLaneTrack = trackIndex;
            if (baseSystem.midi) {
                baseSystem.midi->selectedClipTrack = -1;
                baseSystem.midi->selectedClipIndex = -1;
            }
            updateTimelineSelectionForClip(compClip.startSample, compClip.length, 0, trackIndex);
            return true;
        };
        auto applyMidiCompFromTake = [&](int trackIndex,
                                         int takeIndex,
                                         uint64_t startSample,
                                         uint64_t endSample) -> bool {
            if (!baseSystem.midi) return false;
            MidiContext& midi = *baseSystem.midi;
            if (trackIndex < 0 || trackIndex >= static_cast<int>(midi.tracks.size())) return false;
            MidiTrack& track = midi.tracks[static_cast<size_t>(trackIndex)];
            if (takeIndex < 0 || takeIndex >= static_cast<int>(track.loopTakeClips.size())) return false;
            const MidiClip& take = track.loopTakeClips[static_cast<size_t>(takeIndex)];
            if (take.length == 0) return false;
            uint64_t selStart = std::min(startSample, endSample);
            uint64_t selEnd = std::max(startSample, endSample);
            uint64_t takeStart = take.startSample;
            uint64_t takeEnd = take.startSample + take.length;
            uint64_t clipStart = std::max(selStart, takeStart);
            uint64_t clipEnd = std::min(selEnd, takeEnd);
            if (clipEnd <= clipStart) return false;
            MidiClip compClip = take;
            compClip.startSample = clipStart;
            compClip.length = clipEnd - clipStart;
            compClip.notes.clear();
            compClip.notes.reserve(take.notes.size());
            for (const auto& note : take.notes) {
                if (note.length == 0) continue;
                uint64_t noteAbsStart = takeStart + note.startSample;
                uint64_t noteAbsEnd = noteAbsStart + note.length;
                if (noteAbsEnd <= clipStart || noteAbsStart >= clipEnd) continue;
                uint64_t clippedStart = std::max(noteAbsStart, clipStart);
                uint64_t clippedEnd = std::min(noteAbsEnd, clipEnd);
                if (clippedEnd <= clippedStart) continue;
                MidiNote out = note;
                out.startSample = clippedStart - clipStart;
                out.length = clippedEnd - clippedStart;
                compClip.notes.push_back(out);
            }
            trimMidiClipsForNewClip(track, compClip);
            track.clips.push_back(compClip);
            sortMidiClipsByStart(track.clips);
            int selectedIndex = -1;
            for (int i = static_cast<int>(track.clips.size()) - 1; i >= 0; --i) {
                const MidiClip& candidate = track.clips[static_cast<size_t>(i)];
                if (candidate.startSample == compClip.startSample
                    && candidate.length == compClip.length
                    && candidate.takeId == compClip.takeId
                    && candidate.notes.size() == compClip.notes.size()) {
                    selectedIndex = i;
                    break;
                }
            }
            midi.selectedTrackIndex = trackIndex;
            midi.selectedClipTrack = trackIndex;
            midi.selectedClipIndex = selectedIndex;
            daw.selectedClipTrack = -1;
            daw.selectedClipIndex = -1;
            daw.selectedAutomationClipTrack = -1;
            daw.selectedAutomationClipIndex = -1;
            daw.selectedLaneType = 1;
            daw.selectedLaneTrack = trackIndex;
            updateTimelineSelectionForClip(compClip.startSample, compClip.length, 1, trackIndex);
            return true;
        };
        auto resolveTakeTarget = [&](int& laneType, int& laneTrack) {
            laneType = -1;
            laneTrack = -1;
            if (daw.selectedLaneType == 0 && daw.selectedLaneTrack >= 0
                && daw.selectedLaneTrack < static_cast<int>(daw.tracks.size())) {
                laneType = 0;
                laneTrack = daw.selectedLaneTrack;
                return;
            }
            if (baseSystem.midi && daw.selectedLaneType == 1 && daw.selectedLaneTrack >= 0
                && daw.selectedLaneTrack < static_cast<int>(baseSystem.midi->tracks.size())) {
                laneType = 1;
                laneTrack = daw.selectedLaneTrack;
                return;
            }
            if (daw.selectedClipTrack >= 0 && daw.selectedClipTrack < static_cast<int>(daw.tracks.size())) {
                laneType = 0;
                laneTrack = daw.selectedClipTrack;
                return;
            }
            if (baseSystem.midi && baseSystem.midi->selectedClipTrack >= 0
                && baseSystem.midi->selectedClipTrack < static_cast<int>(baseSystem.midi->tracks.size())) {
                laneType = 1;
                laneTrack = baseSystem.midi->selectedClipTrack;
                return;
            }
        };
        auto applyTakeCycle = [&](int direction) {
            int targetType = -1;
            int targetTrack = -1;
            resolveTakeTarget(targetType, targetTrack);
            if (targetType == 0 && targetTrack >= 0) {
                if (!DawClipSystemLogic::CycleTrackLoopTake(daw, targetTrack, direction)) return;
                DawTrack& track = daw.tracks[static_cast<size_t>(targetTrack)];
                int activeTake = track.activeLoopTakeIndex;
                if (activeTake < 0 || activeTake >= static_cast<int>(track.loopTakeClips.size())) return;
                const DawClip& activeClip = track.loopTakeClips[static_cast<size_t>(activeTake)];
                int selectedIndex = -1;
                for (size_t i = 0; i < track.clips.size(); ++i) {
                    const DawClip& candidate = track.clips[i];
                    if (candidate.audioId == activeClip.audioId
                        && candidate.startSample == activeClip.startSample
                        && candidate.length == activeClip.length
                        && candidate.sourceOffset == activeClip.sourceOffset
                        && candidate.takeId == activeClip.takeId) {
                        selectedIndex = static_cast<int>(i);
                        break;
                    }
                }
                if (selectedIndex < 0) return;
                daw.selectedClipTrack = targetTrack;
                daw.selectedClipIndex = selectedIndex;
                daw.selectedAutomationClipTrack = -1;
                daw.selectedAutomationClipIndex = -1;
                daw.selectedLaneType = 0;
                daw.selectedLaneTrack = targetTrack;
                if (baseSystem.midi) {
                    baseSystem.midi->selectedClipTrack = -1;
                    baseSystem.midi->selectedClipIndex = -1;
                }
                updateTimelineSelectionForClip(activeClip.startSample, activeClip.length, 0, targetTrack);
                return;
            }
            if (targetType == 1 && targetTrack >= 0 && baseSystem.midi) {
                MidiContext& midi = *baseSystem.midi;
                if (!MidiTransportSystemLogic::CycleTrackLoopTake(midi, targetTrack, direction)) return;
                MidiTrack& track = midi.tracks[static_cast<size_t>(targetTrack)];
                int activeTake = track.activeLoopTakeIndex;
                if (activeTake < 0 || activeTake >= static_cast<int>(track.loopTakeClips.size())) return;
                const MidiClip& activeClip = track.loopTakeClips[static_cast<size_t>(activeTake)];
                int selectedIndex = -1;
                for (size_t i = 0; i < track.clips.size(); ++i) {
                    const MidiClip& candidate = track.clips[i];
                    if (candidate.startSample != activeClip.startSample) continue;
                    if (candidate.length != activeClip.length) continue;
                    if (candidate.notes.size() != activeClip.notes.size()) continue;
                    if (candidate.takeId != activeClip.takeId) continue;
                    selectedIndex = static_cast<int>(i);
                    break;
                }
                if (selectedIndex < 0) return;
                midi.selectedTrackIndex = targetTrack;
                midi.selectedClipTrack = targetTrack;
                midi.selectedClipIndex = selectedIndex;
                daw.selectedClipTrack = -1;
                daw.selectedClipIndex = -1;
                daw.selectedAutomationClipTrack = -1;
                daw.selectedAutomationClipIndex = -1;
                daw.selectedLaneType = 1;
                daw.selectedLaneTrack = targetTrack;
                updateTimelineSelectionForClip(activeClip.startSample, activeClip.length, 1, targetTrack);
            }
        };
        if (cmdPrevTakeShortcutNow && !g_cmdPrevTakeShortcutWasDown) {
            applyTakeCycle(-1);
        }
        if (cmdNextTakeShortcutNow && !g_cmdNextTakeShortcutWasDown) {
            applyTakeCycle(+1);
        }
        g_cmdPrevTakeShortcutWasDown = cmdPrevTakeShortcutNow;
        g_cmdNextTakeShortcutWasDown = cmdNextTakeShortcutNow;

        bool deleteShortcutNow = PlatformInput::IsKeyDown(win, PlatformInput::Key::DeleteKey)
            || PlatformInput::IsKeyDown(win, PlatformInput::Key::Backspace);
        if (deleteShortcutNow && !g_deleteShortcutWasDown) {
            if (!deleteSelectedAudioClip(baseSystem, daw)) {
                if (!deleteSelectedMidiClip(baseSystem, daw)) {
                    deleteSelectedAutomationClip(daw);
                }
            }
        }
        g_deleteShortcutWasDown = deleteShortcutNow;

        bool spaceShortcutNow = (!cmdDownNow)
            && PlatformInput::IsKeyDown(win, PlatformInput::Key::Space);
        if (spaceShortcutNow && !g_spaceShortcutWasDown) {
            if (ui.pendingActionType.empty()) {
                if (daw.transportPlaying.load(std::memory_order_relaxed)
                    || daw.transportRecording.load(std::memory_order_relaxed)) {
                    ui.pendingActionType = "DawTransport";
                    ui.pendingActionKey = "stop";
                    ui.pendingActionValue.clear();
                } else {
                    ui.pendingActionType = "DawTransport";
                    ui.pendingActionKey = "play";
                    ui.pendingActionValue.clear();
                }
            }
        }
        g_spaceShortcutWasDown = spaceShortcutNow;
    }
}
