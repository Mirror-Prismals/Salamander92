#pragma once

namespace DawLaneInputSystemLogic {
    void UpdateDawLaneInput(BaseSystem& baseSystem, std::vector<Entity>&, float, PlatformWindowHandle win) {
        if (!baseSystem.ui || !baseSystem.daw || !baseSystem.level || !win) return;
        UIContext& ui = *baseSystem.ui;
        if (!ui.active || ui.loadingActive) return;
        if (baseSystem.midi && baseSystem.midi->pianoRollActive) return;
        if (!DawLaneTimelineSystemLogic::hasDawUiWorld(*baseSystem.level)) return;

        DawContext& daw = *baseSystem.daw;
        const bool exportInProgress = daw.exportInProgress.load(std::memory_order_relaxed);
        if (daw.exportMenuOpen || exportInProgress || daw.settingsMenuOpen) {
            g_rightMouseWasDown = PlatformInput::IsMouseButtonDown(win, PlatformInput::MouseButton::Right);
            ApplyLaneResizeCursor(win, false);
            return;
        }

        bool allowLaneInput = !isCursorOverOpenPanel(baseSystem, ui);
        const auto layout = DawLaneTimelineSystemLogic::ComputeLaneLayout(baseSystem, daw, win);
        const int audioTrackCount = layout.audioTrackCount;
        const int laneCount = layout.laneCount;
        const float laneLeft = layout.laneLeft;
        const float laneRight = layout.laneRight;
        const float laneHalfH = layout.laneHalfH;
        const float rowSpan = layout.rowSpan;
        const float startY = layout.startY;
        const float topBound = layout.topBound;
        const float laneBottomBound = layout.laneBottomBound;
        const double secondsPerScreen = layout.secondsPerScreen;
        const int midiTrackCount = baseSystem.midi
            ? static_cast<int>(baseSystem.midi->tracks.size())
            : 0;

        std::vector<int> midiLaneIndex(static_cast<size_t>(std::max(0, midiTrackCount)), -1);
        if (midiTrackCount > 0) {
            if (!daw.laneOrder.empty()) {
                for (size_t laneIdx = 0; laneIdx < daw.laneOrder.size(); ++laneIdx) {
                    const auto& entry = daw.laneOrder[laneIdx];
                    if (entry.type == 1
                        && entry.trackIndex >= 0
                        && entry.trackIndex < midiTrackCount) {
                        midiLaneIndex[static_cast<size_t>(entry.trackIndex)] = static_cast<int>(laneIdx);
                    }
                }
            } else {
                for (int t = 0; t < midiTrackCount; ++t) {
                    midiLaneIndex[static_cast<size_t>(t)] = audioTrackCount + t;
                }
            }
        }

        auto computeDisplayLaneIndex = [&](int laneIndex, int laneType) -> int {
            int displayIndex = laneIndex;
            int previewSlot = -1;
            bool previewingDrag = false;
            if (daw.dragActive && daw.dragLaneType == laneType && daw.dragLaneIndex >= 0) {
                previewSlot = daw.dragDropIndex;
                previewingDrag = true;
            } else if (daw.externalDropActive && daw.externalDropType == laneType) {
                previewSlot = daw.externalDropIndex;
            }
            if (previewSlot < 0) return displayIndex;
            if (previewingDrag) {
                if (laneIndex == daw.dragLaneIndex) return -1;
                if (laneIndex > daw.dragLaneIndex) {
                    displayIndex -= 1;
                }
            }
            if (displayIndex >= previewSlot) {
                displayIndex += 1;
            }
            return displayIndex;
        };

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

        const bool rightMouseDownNow = PlatformInput::IsMouseButtonDown(win, PlatformInput::MouseButton::Right);
        const bool rightMousePressedNow = rightMouseDownNow && !g_rightMouseWasDown;
        const auto audioLaneIndex = DawLaneTimelineSystemLogic::BuildAudioLaneIndex(daw, audioTrackCount);
        ClipTrimHit trimHover;
        bool trimCursorWanted = false;
        if (allowLaneInput && !daw.clipDragActive && !daw.dragActive && !daw.dragPending) {
            trimHover = findAudioTrimHit(daw,
                                         ui,
                                         audioLaneIndex,
                                         audioTrackCount,
                                         laneLeft,
                                         laneRight,
                                         laneHalfH,
                                         rowSpan,
                                         startY,
                                         secondsPerScreen);
            trimCursorWanted = trimHover.valid;
        }

        if (laneCount > 0 && allowLaneInput && rightMousePressedNow
            && !daw.clipDragActive && !daw.clipTrimActive && !daw.dragActive && !daw.dragPending
            && !daw.takeCompDragActive) {
            int handleLaneIndex = -1;
            for (int laneIdx = 0; laneIdx < laneCount; ++laneIdx) {
                float centerY = startY + static_cast<float>(laneIdx) * rowSpan;
                if (cursorInTrackHandleRect(ui, laneLeft, laneRight, centerY, laneHalfH)) {
                    handleLaneIndex = laneIdx;
                    break;
                }
            }
            if (handleLaneIndex >= 0) {
                int laneType = 0;
                int laneTrack = handleLaneIndex;
                if (!daw.laneOrder.empty() && handleLaneIndex < static_cast<int>(daw.laneOrder.size())) {
                    const auto& entry = daw.laneOrder[static_cast<size_t>(handleLaneIndex)];
                    laneType = entry.type;
                    laneTrack = entry.trackIndex;
                } else if (handleLaneIndex >= audioTrackCount) {
                    laneType = 1;
                    laneTrack = handleLaneIndex - audioTrackCount;
                }
                bool wasExpanded = false;
                if (laneType == 0
                    && laneTrack >= 0
                    && laneTrack < static_cast<int>(daw.tracks.size())) {
                    wasExpanded = daw.tracks[static_cast<size_t>(laneTrack)].takeStackExpanded;
                } else if (laneType == 1 && baseSystem.midi
                           && laneTrack >= 0
                           && laneTrack < static_cast<int>(baseSystem.midi->tracks.size())) {
                    wasExpanded = baseSystem.midi->tracks[static_cast<size_t>(laneTrack)].takeStackExpanded;
                }
                for (auto& track : daw.tracks) {
                    track.takeStackExpanded = false;
                }
                if (baseSystem.midi) {
                    for (auto& track : baseSystem.midi->tracks) {
                        track.takeStackExpanded = false;
                    }
                }
                if (!wasExpanded) {
                    if (laneType == 0
                        && laneTrack >= 0
                        && laneTrack < static_cast<int>(daw.tracks.size())) {
                        daw.tracks[static_cast<size_t>(laneTrack)].takeStackExpanded = true;
                    } else if (laneType == 1 && baseSystem.midi
                               && laneTrack >= 0
                               && laneTrack < static_cast<int>(baseSystem.midi->tracks.size())) {
                        baseSystem.midi->tracks[static_cast<size_t>(laneTrack)].takeStackExpanded = true;
                    }
                }
                daw.selectedLaneIndex = handleLaneIndex;
                daw.selectedLaneType = laneType;
                daw.selectedLaneTrack = laneTrack;
            }
        }

        int takeHitType = -1;
        int takeHitTrack = -1;
        int takeHitIndex = -1;
        if (allowLaneInput && !daw.takeCompDragActive
            && !daw.clipDragActive && !daw.clipTrimActive
            && !daw.dragActive && !daw.dragPending
            && laneCount > 0
            && ui.cursorX >= laneLeft && ui.cursorX <= laneRight) {
            float rowHeight = takeRowHeight(laneHalfH);
            for (int t = 0; t < audioTrackCount && takeHitType < 0; ++t) {
                if (t < 0 || t >= static_cast<int>(daw.tracks.size())) continue;
                const DawTrack& track = daw.tracks[static_cast<size_t>(t)];
                if (!track.takeStackExpanded || track.loopTakeClips.empty()) continue;
                int laneIndex = (t >= 0 && t < static_cast<int>(audioLaneIndex.size()))
                    ? audioLaneIndex[static_cast<size_t>(t)]
                    : -1;
                if (laneIndex < 0) continue;
                int displayIndex = computeDisplayLaneIndex(laneIndex, 0);
                if (displayIndex < 0) continue;
                float centerY = startY + static_cast<float>(displayIndex) * rowSpan;
                float clipTop = 0.0f;
                float clipBottom = 0.0f;
                float lipBottom = 0.0f;
                computeClipRect(centerY, laneHalfH, clipTop, clipBottom, lipBottom);
                (void)clipTop;
                (void)lipBottom;
                float rowStartY = clipBottom + kTakeRowGap;
                for (size_t i = 0; i < track.loopTakeClips.size(); ++i) {
                    float top = rowStartY + static_cast<float>(i) * (rowHeight + kTakeRowSpacing);
                    float bottom = top + rowHeight;
                    if (ui.cursorY >= top && ui.cursorY <= bottom) {
                        takeHitType = 0;
                        takeHitTrack = t;
                        takeHitIndex = static_cast<int>(i);
                        break;
                    }
                }
            }
            if (takeHitType < 0 && baseSystem.midi) {
                for (int t = 0; t < midiTrackCount && takeHitType < 0; ++t) {
                    if (t < 0 || t >= static_cast<int>(baseSystem.midi->tracks.size())) continue;
                    const MidiTrack& track = baseSystem.midi->tracks[static_cast<size_t>(t)];
                    if (!track.takeStackExpanded || track.loopTakeClips.empty()) continue;
                    int laneIndex = (t >= 0 && t < static_cast<int>(midiLaneIndex.size()))
                        ? midiLaneIndex[static_cast<size_t>(t)]
                        : -1;
                    if (laneIndex < 0) continue;
                    int displayIndex = computeDisplayLaneIndex(laneIndex, 1);
                    if (displayIndex < 0) continue;
                    float centerY = startY + static_cast<float>(displayIndex) * rowSpan;
                    float clipTop = 0.0f;
                    float clipBottom = 0.0f;
                    float lipBottom = 0.0f;
                    computeClipRect(centerY, laneHalfH, clipTop, clipBottom, lipBottom);
                    (void)clipTop;
                    (void)lipBottom;
                    float rowStartY = clipBottom + kTakeRowGap;
                    for (size_t i = 0; i < track.loopTakeClips.size(); ++i) {
                        float top = rowStartY + static_cast<float>(i) * (rowHeight + kTakeRowSpacing);
                        float bottom = top + rowHeight;
                        if (ui.cursorY >= top && ui.cursorY <= bottom) {
                            takeHitType = 1;
                            takeHitTrack = t;
                            takeHitIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }
            }
        }

        if (takeHitType >= 0
            && allowLaneInput
            && ui.uiLeftPressed
            && !ui.consumeClick
            && !daw.bpmDragActive
            && !daw.clipDragActive
            && !daw.clipTrimActive
            && !daw.dragActive
            && !daw.dragPending) {
            bool cmdDown = isCommandDown(win);
            uint64_t anchor = sampleFromCursorX(baseSystem,
                                                daw,
                                                laneLeft,
                                                laneRight,
                                                secondsPerScreen,
                                                ui.cursorX,
                                                !cmdDown);
            daw.takeCompDragActive = true;
            daw.takeCompLaneType = takeHitType;
            daw.takeCompTrack = takeHitTrack;
            daw.takeCompTakeIndex = takeHitIndex;
            daw.takeCompStartSample = anchor;
            daw.takeCompEndSample = anchor;
            ui.consumeClick = true;
        }

        if (daw.takeCompDragActive) {
            if (!ui.uiLeftDown) {
                if (daw.takeCompLaneType == 0) {
                    applyAudioCompFromTake(daw.takeCompTrack,
                                           daw.takeCompTakeIndex,
                                           daw.takeCompStartSample,
                                           daw.takeCompEndSample);
                } else if (daw.takeCompLaneType == 1) {
                    applyMidiCompFromTake(daw.takeCompTrack,
                                          daw.takeCompTakeIndex,
                                          daw.takeCompStartSample,
                                          daw.takeCompEndSample);
                }
                daw.takeCompDragActive = false;
                daw.takeCompLaneType = -1;
                daw.takeCompTrack = -1;
                daw.takeCompTakeIndex = -1;
                daw.takeCompStartSample = 0;
                daw.takeCompEndSample = 0;
                ui.consumeClick = true;
            } else {
                bool cmdDown = isCommandDown(win);
                daw.takeCompEndSample = sampleFromCursorX(baseSystem,
                                                          daw,
                                                          laneLeft,
                                                          laneRight,
                                                          secondsPerScreen,
                                                          ui.cursorX,
                                                          !cmdDown);
                ui.consumeClick = true;
            }
        }

        if (laneCount > 0 && allowLaneInput && ui.uiLeftPressed && !ui.consumeClick
            && !daw.bpmDragActive) {
            int handleLaneIndex = -1;
            for (int laneIdx = 0; laneIdx < laneCount; ++laneIdx) {
                float centerY = startY + static_cast<float>(laneIdx) * rowSpan;
                if (cursorInTrackHandleRect(ui, laneLeft, laneRight, centerY, laneHalfH)) {
                    handleLaneIndex = laneIdx;
                    break;
                }
            }
            if (handleLaneIndex >= 0) {
                daw.selectedLaneIndex = handleLaneIndex;
                if (!daw.laneOrder.empty() && handleLaneIndex < static_cast<int>(daw.laneOrder.size())) {
                    const auto& entry = daw.laneOrder[static_cast<size_t>(handleLaneIndex)];
                    daw.selectedLaneType = entry.type;
                    daw.selectedLaneTrack = entry.trackIndex;
                    daw.dragLaneType = entry.type;
                    daw.dragLaneTrack = entry.trackIndex;
                } else {
                    daw.selectedLaneType = 0;
                    daw.selectedLaneTrack = handleLaneIndex;
                    daw.dragLaneType = 0;
                    daw.dragLaneTrack = handleLaneIndex;
                }
                daw.dragLaneIndex = handleLaneIndex;
                daw.dragStartY = static_cast<float>(ui.cursorY);
                daw.dragPending = true;
                daw.dragActive = false;
                ui.consumeClick = true;
            }
        }

        if (!daw.clipDragActive && !daw.clipTrimActive
            && allowLaneInput && ui.uiLeftPressed && !ui.consumeClick
            && !daw.bpmDragActive
            && trimHover.valid) {
            daw.clipTrimActive = true;
            daw.clipTrimLeftEdge = trimHover.leftEdge;
            daw.clipTrimTrack = trimHover.track;
            daw.clipTrimIndex = trimHover.clipIndex;
            daw.clipTrimOriginalStart = trimHover.clipStart;
            daw.clipTrimOriginalLength = trimHover.clipLength;
            daw.clipTrimOriginalSourceOffset = trimHover.clipSourceOffset;
            daw.clipTrimTargetStart = trimHover.clipStart;
            daw.clipTrimTargetLength = trimHover.clipLength;
            daw.clipTrimTargetSourceOffset = trimHover.clipSourceOffset;
            daw.selectedClipTrack = trimHover.track;
            daw.selectedClipIndex = trimHover.clipIndex;
            if (baseSystem.midi) {
                baseSystem.midi->selectedClipTrack = -1;
                baseSystem.midi->selectedClipIndex = -1;
            }
            ui.consumeClick = true;
            trimCursorWanted = true;
        }

        if (!daw.clipDragActive && !daw.clipTrimActive
            && allowLaneInput && ui.uiLeftPressed && !ui.consumeClick
            && !daw.bpmDragActive) {
            double offsetSamples = static_cast<double>(daw.timelineOffsetSamples);
            double windowSamples = secondsPerScreen * static_cast<double>(daw.sampleRate);
            if (windowSamples <= 0.0) windowSamples = 1.0;
            int hitTrack = -1;
            int hitClipIndex = -1;
            for (int t = 0; t < audioTrackCount; ++t) {
                const auto& clips = daw.tracks[static_cast<size_t>(t)].clips;
                if (clips.empty()) continue;
                int laneIndex = audioLaneIndex[static_cast<size_t>(t)];
                if (laneIndex < 0) continue;
                int displayIndex = laneIndex;
                if (daw.dragActive && daw.dragLaneType == 0 && daw.dragLaneIndex >= 0) {
                    int previewSlot = daw.dragDropIndex;
                    if (previewSlot >= 0 && laneIndex >= previewSlot) {
                        displayIndex += 1;
                    }
                } else if (daw.externalDropActive && daw.externalDropType == 0) {
                    int previewSlot = daw.externalDropIndex;
                    if (previewSlot >= 0 && laneIndex >= previewSlot) {
                        displayIndex += 1;
                    }
                }
                float centerY = startY + static_cast<float>(displayIndex) * rowSpan;
                float top = 0.0f;
                float bottom = 0.0f;
                float lipBottom = 0.0f;
                computeClipRect(centerY, laneHalfH, top, bottom, lipBottom);
                if (ui.cursorY < top || ui.cursorY > bottom) continue;
                for (size_t ci = 0; ci < clips.size(); ++ci) {
                    const auto& clip = clips[ci];
                    if (clip.length == 0) continue;
                    double clipStart = static_cast<double>(clip.startSample);
                    double clipEnd = static_cast<double>(clip.startSample + clip.length);
                    if (clipEnd <= offsetSamples || clipStart >= offsetSamples + windowSamples) continue;
                    double visibleStart = std::max(clipStart, offsetSamples);
                    double visibleEnd = std::min(clipEnd, offsetSamples + windowSamples);
                    float t0 = static_cast<float>((visibleStart - offsetSamples) / windowSamples);
                    float t1 = static_cast<float>((visibleEnd - offsetSamples) / windowSamples);
                    float x0 = laneLeft + (laneRight - laneLeft) * t0;
                    float x1 = laneLeft + (laneRight - laneLeft) * t1;
                    expandAndClampClipX(laneLeft, laneRight, x0, x1);
                    if (x1 <= x0) continue;
                    if (ui.cursorX >= x0 && ui.cursorX <= x1
                        && ui.cursorY >= top && ui.cursorY <= lipBottom) {
                        hitTrack = t;
                        hitClipIndex = static_cast<int>(ci);
                        break;
                    }
                }
                if (hitTrack >= 0) break;
            }
            if (hitTrack >= 0 && hitClipIndex >= 0) {
                double cursorT = (laneRight > laneLeft)
                    ? (static_cast<double>(ui.cursorX) - laneLeft) / (laneRight - laneLeft)
                    : 0.0;
                cursorT = std::clamp(cursorT, 0.0, 1.0);
                double cursorSample = offsetSamples + cursorT * windowSamples;
                const auto& clip = daw.tracks[static_cast<size_t>(hitTrack)].clips[static_cast<size_t>(hitClipIndex)];
                daw.clipDragActive = true;
                daw.clipDragTrack = hitTrack;
                daw.clipDragIndex = hitClipIndex;
                daw.clipDragOffsetSamples = static_cast<int64_t>(std::llround(cursorSample)) - static_cast<int64_t>(clip.startSample);
                daw.clipDragTargetTrack = hitTrack;
                daw.clipDragTargetStart = clip.startSample;
                daw.selectedClipTrack = hitTrack;
                daw.selectedClipIndex = hitClipIndex;
                daw.selectedAutomationClipTrack = -1;
                daw.selectedAutomationClipIndex = -1;
                int hitLane = (hitTrack >= 0 && hitTrack < static_cast<int>(audioLaneIndex.size()))
                    ? audioLaneIndex[static_cast<size_t>(hitTrack)]
                    : hitTrack;
                if (hitLane < 0) hitLane = 0;
                daw.timelineSelectionStartSample = clip.startSample;
                daw.timelineSelectionEndSample = clip.startSample + clip.length;
                daw.timelineSelectionStartLane = hitLane;
                daw.timelineSelectionEndLane = hitLane;
                daw.timelineSelectionAnchorSample = clip.startSample;
                daw.timelineSelectionAnchorLane = hitLane;
                daw.timelineSelectionActive = (clip.length > 0);
                daw.timelineSelectionDragActive = false;
                daw.timelineSelectionFromPlayhead = false;
                if (baseSystem.midi) {
                    baseSystem.midi->selectedClipTrack = -1;
                    baseSystem.midi->selectedClipIndex = -1;
                }
                daw.selectedLaneIndex = -1;
                daw.selectedLaneType = -1;
                daw.selectedLaneTrack = -1;
                ui.consumeClick = true;
            }
        }

        if (daw.clipTrimActive) {
            trimCursorWanted = true;
            int trimTrack = daw.clipTrimTrack;
            int trimIndex = daw.clipTrimIndex;
            bool validTrimClip = trimTrack >= 0
                && trimTrack < audioTrackCount
                && trimIndex >= 0
                && trimIndex < static_cast<int>(daw.tracks[static_cast<size_t>(trimTrack)].clips.size());
            if (!validTrimClip) {
                daw.clipTrimActive = false;
                daw.clipTrimTrack = -1;
                daw.clipTrimIndex = -1;
            } else if (!ui.uiLeftDown) {
                DawTrack& track = daw.tracks[static_cast<size_t>(trimTrack)];
                DawClip& clip = track.clips[static_cast<size_t>(trimIndex)];
                clip.startSample = daw.clipTrimTargetStart;
                clip.length = daw.clipTrimTargetLength;
                clip.sourceOffset = daw.clipTrimTargetSourceOffset;
                DawClipSystemLogic::RebuildTrackCacheFromClips(daw, track);
                daw.clipTrimActive = false;
                daw.clipTrimTrack = -1;
                daw.clipTrimIndex = -1;
            } else {
                DawTrack& track = daw.tracks[static_cast<size_t>(trimTrack)];
                const DawClip& clip = track.clips[static_cast<size_t>(trimIndex)];
                uint64_t origStart = daw.clipTrimOriginalStart;
                uint64_t origLength = daw.clipTrimOriginalLength;
                uint64_t origEnd = origStart + origLength;
                uint64_t origSourceOffset = daw.clipTrimOriginalSourceOffset;
                uint64_t sourceSize = 0;
                if (clip.audioId >= 0 && clip.audioId < static_cast<int>(daw.clipAudio.size())) {
                    sourceSize = static_cast<uint64_t>(daw.clipAudio[clip.audioId].left.size());
                }

                uint64_t prevEnd = 0;
                uint64_t nextStart = UINT64_MAX;
                for (size_t i = 0; i < track.clips.size(); ++i) {
                    if (static_cast<int>(i) == trimIndex) continue;
                    const DawClip& other = track.clips[i];
                    if (other.length == 0) continue;
                    uint64_t otherStart = other.startSample;
                    uint64_t otherEnd = other.startSample + other.length;
                    if (otherEnd <= origStart) {
                        prevEnd = std::max(prevEnd, otherEnd);
                    } else if (otherStart >= origEnd) {
                        nextStart = std::min(nextStart, otherStart);
                    }
                }

                bool cmdDown = isCommandDown(win);
                uint64_t cursorSample = sampleFromCursorX(baseSystem,
                                                          daw,
                                                          laneLeft,
                                                          laneRight,
                                                          secondsPerScreen,
                                                          ui.cursorX,
                                                          !cmdDown);
                if (daw.clipTrimLeftEdge) {
                    uint64_t minStart = prevEnd;
                    uint64_t sourceMinStart = (origStart > origSourceOffset) ? (origStart - origSourceOffset) : 0;
                    minStart = std::max(minStart, sourceMinStart);
                    uint64_t maxStart = (origLength > kMinClipSamples)
                        ? (origStart + origLength - kMinClipSamples)
                        : origStart;
                    if (maxStart < minStart) maxStart = minStart;
                    uint64_t newStart = std::clamp(cursorSample, minStart, maxStart);
                    uint64_t newLength = origEnd - newStart;
                    uint64_t newSourceOffset = origSourceOffset + (newStart - origStart);
                    if (sourceSize > 0) {
                        if (newSourceOffset >= sourceSize) {
                            newSourceOffset = sourceSize - 1;
                            newStart = origStart + (newSourceOffset - origSourceOffset);
                            newLength = origEnd - newStart;
                        }
                        uint64_t maxBySource = sourceSize - newSourceOffset;
                        if (maxBySource < kMinClipSamples) maxBySource = kMinClipSamples;
                        if (newLength > maxBySource) {
                            newLength = maxBySource;
                            newStart = origEnd - newLength;
                            newSourceOffset = origSourceOffset + (newStart - origStart);
                        }
                    }
                    daw.clipTrimTargetStart = newStart;
                    daw.clipTrimTargetLength = std::max<uint64_t>(kMinClipSamples, newLength);
                    daw.clipTrimTargetSourceOffset = newSourceOffset;
                } else {
                    uint64_t minEnd = origStart + kMinClipSamples;
                    uint64_t maxEnd = (nextStart == UINT64_MAX) ? UINT64_MAX : nextStart;
                    if (sourceSize > 0 && origSourceOffset < sourceSize) {
                        uint64_t maxBySource = sourceSize - origSourceOffset;
                        maxEnd = std::min<uint64_t>(maxEnd, origStart + std::max<uint64_t>(kMinClipSamples, maxBySource));
                    }
                    if (maxEnd < minEnd) maxEnd = minEnd;
                    uint64_t newEnd = std::clamp(cursorSample, minEnd, maxEnd);
                    daw.clipTrimTargetStart = origStart;
                    daw.clipTrimTargetLength = std::max<uint64_t>(kMinClipSamples, newEnd - origStart);
                    daw.clipTrimTargetSourceOffset = origSourceOffset;
                }
                ui.consumeClick = true;
            }
        }

        if (laneCount > 0
            && allowLaneInput
            && ui.uiLeftPressed
            && !ui.consumeClick
            && !daw.bpmDragActive
            && !daw.clipDragActive
            && !daw.clipTrimActive
            && cursorInLaneRect(ui,
                                laneLeft,
                                laneRight,
                                topBound - layout.laneGap,
                                laneBottomBound + layout.laneGap)) {
            int laneIdx = laneIndexFromCursorY(static_cast<float>(ui.cursorY), startY, laneHalfH, rowSpan, laneCount);
            if (laneIdx < 0) {
                laneIdx = laneIndexFromCursorYClamped(static_cast<float>(ui.cursorY), startY, rowSpan, laneCount);
            }
            if (laneIdx >= 0) {
                float centerY = startY + static_cast<float>(laneIdx) * rowSpan;
                float clipTop = 0.0f;
                float clipBottom = 0.0f;
                float lipBottom = 0.0f;
                computeClipRect(centerY, laneHalfH, clipTop, clipBottom, lipBottom);
                bool inTimelineBody = (static_cast<float>(ui.cursorY) > (lipBottom + 0.5f)
                    && static_cast<float>(ui.cursorY) <= clipBottom);
                if (inTimelineBody) {
                    bool cmdDown = isCommandDown(win);
                    uint64_t snappedSample = sampleFromCursorX(baseSystem, daw, laneLeft, laneRight, secondsPerScreen, ui.cursorX, true);
                    uint64_t rawSample = sampleFromCursorX(baseSystem, daw, laneLeft, laneRight, secondsPerScreen, ui.cursorX, false);
                    if (!cmdDown) {
                        daw.playheadSample.store(snappedSample, std::memory_order_relaxed);
                    }
                    daw.timelineSelectionDragActive = true;
                    daw.timelineSelectionFromPlayhead = cmdDown;
                    daw.timelineSelectionAnchorSample = cmdDown
                        ? daw.playheadSample.load(std::memory_order_relaxed)
                        : snappedSample;
                    daw.timelineSelectionAnchorLane = laneIdx;
                    daw.timelineSelectionStartSample = daw.timelineSelectionAnchorSample;
                    daw.timelineSelectionEndSample = cmdDown ? rawSample : daw.timelineSelectionAnchorSample;
                    daw.timelineSelectionStartLane = laneIdx;
                    daw.timelineSelectionEndLane = laneIdx;
                    daw.timelineSelectionActive = cmdDown && (daw.timelineSelectionStartSample != daw.timelineSelectionEndSample);
                    daw.selectedLaneIndex = -1;
                    daw.selectedLaneType = -1;
                    daw.selectedLaneTrack = -1;
                    daw.selectedClipTrack = -1;
                    daw.selectedClipIndex = -1;
                    daw.selectedAutomationClipTrack = -1;
                    daw.selectedAutomationClipIndex = -1;
                    if (baseSystem.midi) {
                        baseSystem.midi->selectedClipTrack = -1;
                        baseSystem.midi->selectedClipIndex = -1;
                    }
                    ui.consumeClick = true;
                }
            }
        }

        if (daw.timelineSelectionDragActive) {
            if (!ui.uiLeftDown) {
                daw.timelineSelectionDragActive = false;
                daw.timelineSelectionFromPlayhead = false;
                bool hasRange = (daw.timelineSelectionStartSample != daw.timelineSelectionEndSample)
                    || (daw.timelineSelectionStartLane != daw.timelineSelectionEndLane);
                daw.timelineSelectionActive = hasRange;
            } else {
                int laneIdx = laneIndexFromCursorY(static_cast<float>(ui.cursorY), startY, laneHalfH, rowSpan, laneCount);
                if (laneIdx < 0) {
                    laneIdx = laneIndexFromCursorYClamped(static_cast<float>(ui.cursorY), startY, rowSpan, laneCount);
                }
                if (laneIdx >= 0) {
                    bool cmdDown = isCommandDown(win);
                    uint64_t cursorSample = sampleFromCursorX(baseSystem,
                                                              daw,
                                                              laneLeft,
                                                              laneRight,
                                                              secondsPerScreen,
                                                              ui.cursorX,
                                                              !cmdDown);
                    daw.timelineSelectionStartSample = daw.timelineSelectionAnchorSample;
                    daw.timelineSelectionEndSample = cursorSample;
                    daw.timelineSelectionStartLane = daw.timelineSelectionAnchorLane;
                    daw.timelineSelectionEndLane = laneIdx;
                    bool hasRange = (daw.timelineSelectionStartSample != daw.timelineSelectionEndSample)
                        || (daw.timelineSelectionStartLane != daw.timelineSelectionEndLane);
                    daw.timelineSelectionActive = hasRange;
                    ui.consumeClick = true;
                }
            }
        }

        if (daw.clipDragActive) {
            if (!ui.uiLeftDown) {
                int srcTrack = daw.clipDragTrack;
                int srcIndex = daw.clipDragIndex;
                int dstTrack = daw.clipDragTargetTrack;
                if (srcTrack >= 0 && srcTrack < audioTrackCount
                    && dstTrack >= 0 && dstTrack < audioTrackCount) {
                    DawTrack& fromTrack = daw.tracks[static_cast<size_t>(srcTrack)];
                    if (srcIndex >= 0 && srcIndex < static_cast<int>(fromTrack.clips.size())) {
                        DawClip clip = fromTrack.clips[static_cast<size_t>(srcIndex)];
                        fromTrack.clips.erase(fromTrack.clips.begin() + srcIndex);
                        clip.startSample = daw.clipDragTargetStart;
                        DawTrack& toTrack = daw.tracks[static_cast<size_t>(dstTrack)];
                        DawClipSystemLogic::TrimClipsForNewClip(toTrack, clip);
                        toTrack.clips.push_back(clip);
                        std::sort(toTrack.clips.begin(), toTrack.clips.end(), [](const DawClip& a, const DawClip& b) {
                            if (a.startSample == b.startSample) return a.sourceOffset < b.sourceOffset;
                            return a.startSample < b.startSample;
                        });
                        int selectedIndex = -1;
                        for (size_t i = 0; i < toTrack.clips.size(); ++i) {
                            const DawClip& candidate = toTrack.clips[i];
                            if (candidate.audioId == clip.audioId
                                && candidate.startSample == clip.startSample
                                && candidate.length == clip.length
                                && candidate.sourceOffset == clip.sourceOffset
                                && candidate.takeId == clip.takeId) {
                                selectedIndex = static_cast<int>(i);
                                break;
                            }
                        }
                        daw.selectedClipTrack = dstTrack;
                        daw.selectedClipIndex = selectedIndex;
                        daw.selectedAutomationClipTrack = -1;
                        daw.selectedAutomationClipIndex = -1;
                        const DawClip* selectedClip = nullptr;
                        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(toTrack.clips.size())) {
                            selectedClip = &toTrack.clips[static_cast<size_t>(selectedIndex)];
                        } else {
                            selectedClip = &clip;
                        }
                        int selectedLane = (dstTrack >= 0 && dstTrack < static_cast<int>(audioLaneIndex.size()))
                            ? audioLaneIndex[static_cast<size_t>(dstTrack)]
                            : dstTrack;
                        if (selectedLane < 0) selectedLane = 0;
                        daw.timelineSelectionStartSample = selectedClip->startSample;
                        daw.timelineSelectionEndSample = selectedClip->startSample + selectedClip->length;
                        daw.timelineSelectionStartLane = selectedLane;
                        daw.timelineSelectionEndLane = selectedLane;
                        daw.timelineSelectionAnchorSample = selectedClip->startSample;
                        daw.timelineSelectionAnchorLane = selectedLane;
                        daw.timelineSelectionActive = (selectedClip->length > 0);
                        daw.timelineSelectionDragActive = false;
                        daw.timelineSelectionFromPlayhead = false;
                        if (baseSystem.midi) {
                            baseSystem.midi->selectedClipTrack = -1;
                            baseSystem.midi->selectedClipIndex = -1;
                        }
                        DawClipSystemLogic::RebuildTrackCacheFromClips(daw, toTrack);
                        if (srcTrack != dstTrack) {
                            DawClipSystemLogic::RebuildTrackCacheFromClips(daw, fromTrack);
                        }
                    }
                }
                daw.clipDragActive = false;
                daw.clipDragTrack = -1;
                daw.clipDragIndex = -1;
                daw.clipDragTargetTrack = -1;
                daw.clipDragTargetStart = 0;
                daw.clipDragOffsetSamples = 0;
            } else {
                double offsetSamples = static_cast<double>(daw.timelineOffsetSamples);
                double windowSamples = secondsPerScreen * static_cast<double>(daw.sampleRate);
                if (windowSamples <= 0.0) windowSamples = 1.0;
                double cursorT = (laneRight > laneLeft)
                    ? (static_cast<double>(ui.cursorX) - laneLeft) / (laneRight - laneLeft)
                    : 0.0;
                cursorT = std::clamp(cursorT, 0.0, 1.0);
                int64_t targetSample = static_cast<int64_t>(std::llround(offsetSamples + cursorT * windowSamples))
                    - daw.clipDragOffsetSamples;
                if (targetSample < 0) {
                    uint64_t shiftSamples = computeRebaseShiftSamples(daw, targetSample);
                    DawTimelineRebaseLogic::ShiftTimelineRight(baseSystem, shiftSamples);
                    targetSample += static_cast<int64_t>(shiftSamples);
                }
                bool cmdDown = PlatformInput::IsKeyDown(win, PlatformInput::Key::LeftSuper)
                    || PlatformInput::IsKeyDown(win, PlatformInput::Key::RightSuper);
                if (!cmdDown) {
                    double bpm = daw.bpm.load(std::memory_order_relaxed);
                    if (bpm <= 0.0) bpm = 120.0;
                    double secondsPerBeat = 60.0 / bpm;
                    double gridSeconds = DawLaneTimelineSystemLogic::GridSecondsForZoom(secondsPerScreen, secondsPerBeat);
                    if (gridSeconds > 0.0) {
                        uint64_t gridStepSamples = std::max<uint64_t>(1,
                            static_cast<uint64_t>(std::llround(gridSeconds * daw.sampleRate)));
                        targetSample = static_cast<int64_t>((static_cast<uint64_t>(targetSample) / gridStepSamples) * gridStepSamples);
                    }
                }
                daw.clipDragTargetStart = static_cast<uint64_t>(targetSample);

                int dstTrack = daw.clipDragTrack;
                if (!daw.laneOrder.empty()) {
                    int laneIdx = laneIndexFromCursorY(static_cast<float>(ui.cursorY), startY, laneHalfH, rowSpan, laneCount);
                    if (laneIdx >= 0 && laneIdx < static_cast<int>(daw.laneOrder.size())) {
                        const auto& entry = daw.laneOrder[static_cast<size_t>(laneIdx)];
                        if (entry.type == 0) {
                            dstTrack = entry.trackIndex;
                        }
                    }
                } else {
                    int laneIdx = laneIndexFromCursorY(static_cast<float>(ui.cursorY), startY, laneHalfH, rowSpan, laneCount);
                    if (laneIdx >= 0) dstTrack = laneIdx;
                }
                daw.clipDragTargetTrack = dstTrack;
                ui.consumeClick = true;
            }
        }

        g_rightMouseWasDown = rightMouseDownNow;
        ApplyLaneResizeCursor(win, trimCursorWanted || daw.clipTrimActive);
    }
}
