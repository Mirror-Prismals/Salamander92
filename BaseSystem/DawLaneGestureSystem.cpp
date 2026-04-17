#pragma once

namespace DawLaneInputSystemLogic {
    namespace {
        bool cursorInLaneRect(const UIContext& ui, float laneLeft, float laneRight, float top, float bottom) {
            float x = static_cast<float>(ui.cursorX);
            float y = static_cast<float>(ui.cursorY);
            return x >= laneLeft && x <= laneRight && y >= top && y <= bottom;
        }

        bool cursorInRect(const UIContext& ui, float left, float right, float top, float bottom) {
            float x = static_cast<float>(ui.cursorX);
            float y = static_cast<float>(ui.cursorY);
            return x >= left && x <= right && y >= top && y <= bottom;
        }

        bool cursorInTrackHandleRect(const UIContext& ui,
                                     float laneLeft,
                                     float laneRight,
                                     float centerY,
                                     float laneHalfH) {
            float laneHeight = laneHalfH * 2.0f;
            float handleSize = std::min(kTrackHandleSize, std::max(14.0f, laneHeight));
            float handleHalf = handleSize * 0.5f;
            float centerX = laneRight + kTrackHandleInset + handleHalf;
            float minCenterX = laneLeft + 4.0f + handleHalf;
            if (centerX < minCenterX) centerX = minCenterX;
            return cursorInRect(ui,
                                centerX - handleHalf,
                                centerX + handleHalf,
                                centerY - handleHalf,
                                centerY + handleHalf);
        }

        void computeClipRect(float centerY,
                             float laneHalfH,
                             float& top,
                             float& bottom,
                             float& lipBottom) {
            top = centerY - laneHalfH + kClipVerticalInset;
            bottom = centerY + laneHalfH - kClipVerticalInset;
            if (bottom < top + kClipMinHeight) {
                float mid = (top + bottom) * 0.5f;
                top = mid - (kClipMinHeight * 0.5f);
                bottom = mid + (kClipMinHeight * 0.5f);
            }
            float lipHeight = std::clamp((bottom - top) * 0.18f, kClipLipMinHeight, kClipLipMaxHeight);
            lipBottom = std::min(bottom, top + lipHeight);
        }

        void expandAndClampClipX(float laneLeft, float laneRight, float& x0, float& x1) {
            x0 = std::max(laneLeft, x0 - kClipHorizontalPad);
            x1 = std::min(laneRight, x1 + kClipHorizontalPad);
        }

        bool isCursorOverOpenPanel(const BaseSystem& baseSystem, const UIContext& ui) {
            if (!baseSystem.panel) return false;
            const PanelContext& panel = *baseSystem.panel;
            const float x = static_cast<float>(ui.cursorX);
            const float y = static_cast<float>(ui.cursorY);
            auto pickRect = [](const PanelRect& renderRect, const PanelRect& fallbackRect) -> const PanelRect& {
                if (renderRect.w > 0.0f && renderRect.h > 0.0f) return renderRect;
                return fallbackRect;
            };
            auto overRect = [&](const PanelRect& rect) {
                if (rect.w <= 0.0f || rect.h <= 0.0f) return false;
                return x >= rect.x && x <= rect.x + rect.w
                    && y >= rect.y && y <= rect.y + rect.h;
            };
            if (panel.topState > 0.01f && overRect(pickRect(panel.topRenderRect, panel.topRect))) return true;
            if (panel.bottomState > 0.01f && overRect(pickRect(panel.bottomRenderRect, panel.bottomRect))) return true;
            if (panel.leftState > 0.01f && overRect(pickRect(panel.leftRenderRect, panel.leftRect))) return true;
            if (panel.rightState > 0.01f && overRect(pickRect(panel.rightRenderRect, panel.rightRect))) return true;
            return false;
        }

        int laneIndexFromCursorY(float y, float startY, float laneHalfH, float rowSpan, int trackCount) {
            for (int i = 0; i < trackCount; ++i) {
                float centerY = startY + static_cast<float>(i) * rowSpan;
                if (y >= centerY - laneHalfH && y <= centerY + laneHalfH) {
                    return i;
                }
            }
            return -1;
        }

        int dropSlotFromCursorY(float y, float startY, float rowSpan, int trackCount) {
            float rel = (y - startY) / rowSpan;
            int slot = static_cast<int>(std::floor(rel + 0.5f));
            if (slot < 0) slot = 0;
            if (slot > trackCount) slot = trackCount;
            return slot;
        }

        ClipTrimHit findAudioTrimHit(const DawContext& daw,
                                     const UIContext& ui,
                                     const std::vector<int>& audioLaneIndex,
                                     int audioTrackCount,
                                     float laneLeft,
                                     float laneRight,
                                     float laneHalfH,
                                     float rowSpan,
                                     float startY,
                                     double secondsPerScreen) {
            ClipTrimHit hit;
            if (audioTrackCount <= 0 || laneRight <= laneLeft) return hit;

            double offsetSamples = static_cast<double>(daw.timelineOffsetSamples);
            double windowSamples = secondsPerScreen * static_cast<double>(daw.sampleRate);
            if (windowSamples <= 0.0) windowSamples = 1.0;

            for (int t = 0; t < audioTrackCount; ++t) {
                const auto& clips = daw.tracks[static_cast<size_t>(t)].clips;
                if (clips.empty()) continue;
                int laneIndex = (t >= 0 && t < static_cast<int>(audioLaneIndex.size())) ? audioLaneIndex[static_cast<size_t>(t)] : -1;
                if (laneIndex < 0) continue;
                float centerY = startY + static_cast<float>(laneIndex) * rowSpan;
                float top = 0.0f;
                float bottom = 0.0f;
                float lipBottom = 0.0f;
                computeClipRect(centerY, laneHalfH, top, bottom, lipBottom);
                if (ui.cursorY < top || ui.cursorY > lipBottom) continue;

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
                    if (ui.cursorX < x0 - kTrimEdgeHitWidth || ui.cursorX > x1 + kTrimEdgeHitWidth) continue;

                    float distLeft = std::fabs(static_cast<float>(ui.cursorX) - x0);
                    float distRight = std::fabs(static_cast<float>(ui.cursorX) - x1);
                    bool nearLeft = distLeft <= kTrimEdgeHitWidth;
                    bool nearRight = distRight <= kTrimEdgeHitWidth;
                    if (!nearLeft && !nearRight) continue;

                    bool pickLeft = false;
                    float edgeDist = 0.0f;
                    if (nearLeft && nearRight) {
                        pickLeft = distLeft <= distRight;
                        edgeDist = pickLeft ? distLeft : distRight;
                    } else if (nearLeft) {
                        pickLeft = true;
                        edgeDist = distLeft;
                    } else {
                        pickLeft = false;
                        edgeDist = distRight;
                    }

                    if (!hit.valid || edgeDist < hit.edgeDistance) {
                        hit.valid = true;
                        hit.leftEdge = pickLeft;
                        hit.track = t;
                        hit.clipIndex = static_cast<int>(ci);
                        hit.edgeDistance = edgeDist;
                        hit.clipStart = clip.startSample;
                        hit.clipLength = clip.length;
                        hit.clipSourceOffset = clip.sourceOffset;
                    }
                }
            }
            return hit;
        }
    }

    void UpdateDawLaneGestures(BaseSystem& baseSystem, std::vector<Entity>&, float dt, PlatformWindowHandle win) {
        if (!baseSystem.ui || !baseSystem.daw || !baseSystem.level || !win) return;
        UIContext& ui = *baseSystem.ui;
        if (!ui.active || ui.loadingActive) return;
        if (baseSystem.midi && baseSystem.midi->pianoRollActive) return;
        if (!DawLaneTimelineSystemLogic::hasDawUiWorld(*baseSystem.level)) return;

        DawContext& daw = *baseSystem.daw;
        const bool exportInProgress = daw.exportInProgress.load(std::memory_order_relaxed);
        if (daw.exportMenuOpen || exportInProgress || daw.settingsMenuOpen) return;

        bool allowLaneInput = !isCursorOverOpenPanel(baseSystem, ui);
        const auto layout = DawLaneTimelineSystemLogic::ComputeLaneLayout(baseSystem, daw, win);
        const int laneCount = layout.laneCount;
        const float laneLeft = layout.laneLeft;
        const float laneRight = layout.laneRight;
        const float laneHalfH = layout.laneHalfH;
        const float rowSpan = layout.rowSpan;
        const float topBound = layout.topBound;
        const float laneBottomBound = layout.laneBottomBound;
        const float handleY = layout.handleY;
        const float handleHalf = layout.handleHalf;
        const float rulerTopY = layout.rulerTopY;
        const float rulerBottomY = layout.rulerBottomY;
        const float rulerLeft = layout.rulerLeft;
        const float rulerRight = layout.rulerRight;
        const float upperRulerTop = layout.upperRulerTop;
        const float upperRulerBottom = layout.upperRulerBottom;
        const float verticalRulerLeft = layout.verticalRulerLeft;
        const float verticalRulerRight = layout.verticalRulerRight;
        const float verticalRulerTop = layout.verticalRulerTop;
        const float verticalRulerBottom = layout.verticalRulerBottom;
        const double secondsPerScreen = layout.secondsPerScreen;

        bool bpmDragPressed = false;
        bool hasBpmDragControl = false;
        float bpmLeft = 0.0f;
        float bpmRight = 0.0f;
        float bpmTop = 0.0f;
        float bpmBottom = 0.0f;
        for (auto* instPtr : daw.tempoInstances) {
            if (!instPtr) continue;
            EntityInstance& inst = *instPtr;
            if (inst.actionKey != "bpm_drag") continue;
            glm::vec2 halfSize(inst.size.x, inst.size.y);
            bpmLeft = inst.position.x - halfSize.x;
            bpmRight = inst.position.x + halfSize.x;
            bpmTop = inst.position.y - halfSize.y;
            bpmBottom = inst.position.y + halfSize.y;
            hasBpmDragControl = true;
            break;
        }

        if (ui.uiLeftPressed && hasBpmDragControl
            && cursorInRect(ui, bpmLeft, bpmRight, bpmTop, bpmBottom)) {
            daw.bpmDragActive = true;
            daw.bpmDragLastY = ui.cursorY;
            bpmDragPressed = true;
            ui.consumeClick = true;
        }

        if (daw.bpmDragActive) {
            if (!ui.uiLeftDown) {
                daw.bpmDragActive = false;
            } else {
                double dy = ui.cursorY - daw.bpmDragLastY;
                daw.bpmDragLastY = ui.cursorY;
                double bpm = daw.bpm.load(std::memory_order_relaxed);
                const double bpmPerPixel = 0.15;
                bpm -= dy * bpmPerPixel;
                bpm = std::clamp(bpm, 40.0, 240.0);
                daw.bpm.store(bpm, std::memory_order_relaxed);
                ui.consumeClick = true;
            }
        }

        bool playheadPressed = false;
        if (allowLaneInput && ui.uiLeftPressed && !daw.bpmDragActive && !bpmDragPressed
            && !daw.verticalRulerDragActive) {
            double playheadSec = static_cast<double>(daw.playheadSample.load(std::memory_order_relaxed))
                / static_cast<double>(daw.sampleRate);
            double offsetSec = static_cast<double>(daw.timelineOffsetSamples) / static_cast<double>(daw.sampleRate);
            double tNorm = secondsPerScreen > 0.0 ? (playheadSec - offsetSec) / secondsPerScreen : 0.0;
            tNorm = std::clamp(tNorm, 0.0, 1.0);
            float playheadX = static_cast<float>(laneLeft + (laneRight - laneLeft) * tNorm);
            float left = playheadX - handleHalf;
            float right = playheadX + handleHalf;
            float top = handleY - handleHalf;
            float bottom = handleY + handleHalf;
            if (cursorInRect(ui, left, right, top, bottom)) {
                daw.playheadDragActive = true;
                daw.playheadDragOffsetX = playheadX - static_cast<float>(ui.cursorX);
                playheadPressed = true;
                ui.consumeClick = true;
            }
        }

        bool rulerPressed = false;
        if (allowLaneInput && ui.uiLeftPressed && !playheadPressed && !daw.bpmDragActive && !bpmDragPressed
            && !daw.verticalRulerDragActive) {
            if (cursorInRect(ui, rulerLeft, rulerRight, rulerTopY, rulerBottomY)) {
                daw.rulerDragActive = true;
                daw.rulerDragStartY = ui.cursorY;
                daw.rulerDragLastX = ui.cursorX;
                daw.rulerDragLastY = ui.cursorY;
                daw.rulerDragStartSeconds = secondsPerScreen;
                daw.rulerDragAccumDY = 0.0;
                daw.rulerDragEdgeDirection = 0;
                double anchorT = (laneRight > laneLeft)
                    ? (static_cast<double>(ui.cursorX) - laneLeft) / (laneRight - laneLeft)
                    : 0.0;
                daw.rulerDragAnchorT = std::clamp(anchorT, 0.0, 1.0);
                double offsetSec = static_cast<double>(daw.timelineOffsetSamples) / static_cast<double>(daw.sampleRate);
                daw.rulerDragAnchorTimeSec = offsetSec + daw.rulerDragAnchorT * secondsPerScreen;
                rulerPressed = true;
                ui.consumeClick = true;
            }
        }
        bool verticalRulerPressed = false;
        if (allowLaneInput && ui.uiLeftPressed && !playheadPressed && !rulerPressed
            && !daw.bpmDragActive && !bpmDragPressed) {
            if (cursorInRect(ui, verticalRulerLeft, verticalRulerRight, verticalRulerTop, verticalRulerBottom)) {
                daw.verticalRulerDragActive = true;
                daw.verticalRulerDragLastX = ui.cursorX;
                daw.verticalRulerDragLastY = ui.cursorY;
                daw.verticalRulerDragStartLaneHeight = std::clamp(daw.timelineLaneHeight, 24.0f, 180.0f);
                daw.verticalRulerDragAccumDX = 0.0;
                daw.verticalRulerDragXEdgeDirection = 0;
                daw.verticalRulerDragEdgeDirection = 0;
                verticalRulerPressed = true;
                ui.consumeClick = true;
            }
        }

        if (allowLaneInput && ui.uiLeftPressed && !playheadPressed && !rulerPressed && !verticalRulerPressed
            && !daw.bpmDragActive && !bpmDragPressed) {
            if (cursorInRect(ui, rulerLeft, rulerRight, upperRulerTop, upperRulerBottom)) {
                double offsetSec = (daw.sampleRate > 0.0)
                    ? static_cast<double>(daw.timelineOffsetSamples) / static_cast<double>(daw.sampleRate)
                    : 0.0;
                double loopStartSec = (daw.sampleRate > 0.0)
                    ? static_cast<double>(daw.loopStartSamples) / static_cast<double>(daw.sampleRate)
                    : 0.0;
                double loopEndSec = (daw.sampleRate > 0.0)
                    ? static_cast<double>(daw.loopEndSamples) / static_cast<double>(daw.sampleRate)
                    : 0.0;
                float loopStartX = static_cast<float>(laneLeft + (laneRight - laneLeft) * ((loopStartSec - offsetSec) / secondsPerScreen));
                float loopEndX = static_cast<float>(laneLeft + (laneRight - laneLeft) * ((loopEndSec - offsetSec) / secondsPerScreen));
                if (loopEndX < loopStartX) std::swap(loopStartX, loopEndX);
                float loopLeft = std::clamp(loopStartX, rulerLeft, rulerRight);
                float loopRight = std::clamp(loopEndX, rulerLeft, rulerRight);
                float leftHandle = loopLeft - kLoopHandleWidth;
                float rightHandle = loopLeft + kLoopHandleWidth;
                float leftHandle2 = loopRight - kLoopHandleWidth;
                float rightHandle2 = loopRight + kLoopHandleWidth;
                if (cursorInRect(ui, leftHandle, rightHandle, upperRulerTop, upperRulerBottom)) {
                    daw.loopDragActive = true;
                    daw.loopDragMode = 1;
                    ui.consumeClick = true;
                } else if (cursorInRect(ui, leftHandle2, rightHandle2, upperRulerTop, upperRulerBottom)) {
                    daw.loopDragActive = true;
                    daw.loopDragMode = 2;
                    ui.consumeClick = true;
                } else if (cursorInRect(ui, loopLeft, loopRight, upperRulerTop, upperRulerBottom)) {
                    double sampleRate = (daw.sampleRate > 0.0) ? daw.sampleRate : 44100.0;
                    double cursorT = (laneRight > laneLeft)
                        ? (static_cast<double>(ui.cursorX) - laneLeft) / (laneRight - laneLeft)
                        : 0.0;
                    cursorT = std::clamp(cursorT, 0.0, 1.0);
                    double timeSec = offsetSec + cursorT * secondsPerScreen;
                    int64_t cursorSample = static_cast<int64_t>(std::llround(timeSec * sampleRate));
                    daw.loopDragActive = true;
                    daw.loopDragMode = 3;
                    daw.loopDragOffsetSamples = cursorSample - static_cast<int64_t>(daw.loopStartSamples);
                    daw.loopDragLengthSamples = (daw.loopEndSamples > daw.loopStartSamples)
                        ? (daw.loopEndSamples - daw.loopStartSamples)
                        : 0;
                    ui.consumeClick = true;
                }
            }
        }

        if (daw.loopDragActive) {
            if (!ui.uiLeftDown) {
                daw.loopDragActive = false;
                daw.loopDragMode = 0;
            } else {
                double bpm = daw.bpm.load(std::memory_order_relaxed);
                if (bpm <= 0.0) bpm = 120.0;
                double secondsPerBeat = (bpm > 0.0) ? (60.0 / bpm) : 0.5;
                double gridSeconds = DawLaneTimelineSystemLogic::GridSecondsForZoom(secondsPerScreen, secondsPerBeat);
                double cursorT = (laneRight > laneLeft)
                    ? (static_cast<double>(ui.cursorX) - laneLeft) / (laneRight - laneLeft)
                    : 0.0;
                cursorT = std::clamp(cursorT, 0.0, 1.0);
                double offsetSec = (daw.sampleRate > 0.0)
                    ? static_cast<double>(daw.timelineOffsetSamples) / static_cast<double>(daw.sampleRate)
                    : 0.0;
                double sampleRate = (daw.sampleRate > 0.0) ? daw.sampleRate : 44100.0;
                double timeSec = offsetSec + cursorT * secondsPerScreen;
                long long rawSample = std::llround(timeSec * sampleRate);
                if (rawSample < 0) {
                    uint64_t shiftSamples = computeRebaseShiftSamples(daw, static_cast<int64_t>(rawSample));
                    DawTimelineRebaseLogic::ShiftTimelineRight(baseSystem, shiftSamples);
                    rawSample += static_cast<long long>(shiftSamples);
                }
                uint64_t targetSample = static_cast<uint64_t>(rawSample);
                bool cmdDown = PlatformInput::IsKeyDown(win, PlatformInput::Key::LeftSuper)
                    || PlatformInput::IsKeyDown(win, PlatformInput::Key::RightSuper);
                uint64_t gridStepSamples = 1;
                if (!cmdDown && gridSeconds > 0.0) {
                    gridStepSamples = std::max<uint64_t>(1, static_cast<uint64_t>(std::llround(gridSeconds * sampleRate)));
                    if (gridStepSamples > 0) {
                        targetSample = static_cast<uint64_t>(std::llround(static_cast<double>(targetSample) / gridStepSamples)) * gridStepSamples;
                    }
                }
                uint64_t loopStart = daw.loopStartSamples;
                uint64_t loopEnd = daw.loopEndSamples;
                if (loopEnd <= loopStart) {
                    loopEnd = loopStart + gridStepSamples;
                }
                uint64_t minLen = std::max<uint64_t>(1, gridStepSamples);
                if (daw.loopDragMode == 1) {
                    if (targetSample + minLen > loopEnd) {
                        targetSample = loopEnd - minLen;
                    }
                    daw.loopStartSamples = targetSample;
                } else if (daw.loopDragMode == 2) {
                    if (targetSample < loopStart + minLen) {
                        targetSample = loopStart + minLen;
                    }
                    daw.loopEndSamples = targetSample;
                } else if (daw.loopDragMode == 3) {
                    int64_t proposedStart = static_cast<int64_t>(targetSample) - daw.loopDragOffsetSamples;
                    if (proposedStart < 0) {
                        uint64_t shiftSamples = computeRebaseShiftSamples(daw, proposedStart);
                        DawTimelineRebaseLogic::ShiftTimelineRight(baseSystem, shiftSamples);
                        proposedStart += static_cast<int64_t>(shiftSamples);
                    }
                    uint64_t length = daw.loopDragLengthSamples;
                    if (length < minLen) length = minLen;
                    uint64_t newStart = static_cast<uint64_t>(proposedStart);
                    if (!cmdDown && gridStepSamples > 0) {
                        newStart = (newStart / gridStepSamples) * gridStepSamples;
                    }
                    daw.loopStartSamples = newStart;
                    daw.loopEndSamples = newStart + length;
                }
                uint64_t maxSamples = DawLaneTimelineSystemLogic::MaxTimelineSamples(daw);
                uint64_t windowSamples = static_cast<uint64_t>(std::max(0.0, secondsPerScreen * sampleRate));
                uint64_t maxAllowed = maxSamples + windowSamples;
                if (daw.loopStartSamples > maxAllowed) daw.loopStartSamples = maxAllowed;
                if (daw.loopEndSamples > maxAllowed) daw.loopEndSamples = maxAllowed;
            }
        }

        if (daw.rulerDragActive) {
            if (!ui.uiLeftDown) {
                daw.rulerDragActive = false;
            } else {
                double dx = ui.cursorX - daw.rulerDragLastX;
                daw.rulerDragLastX = ui.cursorX;
                double dy = ui.cursorY - daw.rulerDragLastY;
                daw.rulerDragLastY = ui.cursorY;

                if (dy < -0.001) {
                    daw.rulerDragEdgeDirection = -1;
                } else if (dy > 0.001) {
                    daw.rulerDragEdgeDirection = 1;
                }

                const double edgeMarginPx = 1.0;
                const double edgePixelsPerSecond = 420.0;
                if (ui.cursorY <= edgeMarginPx && daw.rulerDragEdgeDirection < 0) {
                    dy -= edgePixelsPerSecond * static_cast<double>(dt);
                } else if (ui.cursorY >= layout.screenHeight - edgeMarginPx && daw.rulerDragEdgeDirection > 0) {
                    dy += edgePixelsPerSecond * static_cast<double>(dt);
                }

                daw.rulerDragAccumDY += dy;
                double scale = std::exp(-daw.rulerDragAccumDY * 0.01);
                double newSeconds = daw.rulerDragStartSeconds * scale;
                newSeconds = std::clamp(newSeconds, 2.0, 120.0);

                if (laneRight > laneLeft) {
                    double secondsPerPixel = newSeconds / static_cast<double>(laneRight - laneLeft);
                    daw.rulerDragAnchorTimeSec -= dx * secondsPerPixel;
                }
                double newOffsetSec = daw.rulerDragAnchorTimeSec - daw.rulerDragAnchorT * newSeconds;
                daw.timelineSecondsPerScreen = newSeconds;
                daw.timelineOffsetSamples = static_cast<int64_t>(std::llround(newOffsetSec * daw.sampleRate));
                DawLaneTimelineSystemLogic::ClampTimelineOffset(daw);
            }
        }

        if (daw.verticalRulerDragActive) {
            if (!ui.uiLeftDown) {
                daw.verticalRulerDragActive = false;
            } else {
                double dx = ui.cursorX - daw.verticalRulerDragLastX;
                daw.verticalRulerDragLastX = ui.cursorX;
                double dy = ui.cursorY - daw.verticalRulerDragLastY;
                daw.verticalRulerDragLastY = ui.cursorY;

                if (dx < -0.001) {
                    daw.verticalRulerDragXEdgeDirection = -1;
                } else if (dx > 0.001) {
                    daw.verticalRulerDragXEdgeDirection = 1;
                }
                if (dy < -0.001) {
                    daw.verticalRulerDragEdgeDirection = -1;
                } else if (dy > 0.001) {
                    daw.verticalRulerDragEdgeDirection = 1;
                }

                const double edgeMarginPx = 1.0;
                const double edgePixelsPerSecond = 420.0;
                if (ui.cursorX <= edgeMarginPx && daw.verticalRulerDragXEdgeDirection < 0) {
                    dx -= edgePixelsPerSecond * static_cast<double>(dt);
                } else if (ui.cursorX >= layout.screenWidth - edgeMarginPx
                           && daw.verticalRulerDragXEdgeDirection > 0) {
                    dx += edgePixelsPerSecond * static_cast<double>(dt);
                }
                if (ui.cursorY <= edgeMarginPx && daw.verticalRulerDragEdgeDirection < 0) {
                    dy -= edgePixelsPerSecond * static_cast<double>(dt);
                } else if (ui.cursorY >= layout.screenHeight - edgeMarginPx
                           && daw.verticalRulerDragEdgeDirection > 0) {
                    dy += edgePixelsPerSecond * static_cast<double>(dt);
                }

                daw.verticalRulerDragAccumDX += dx;
                float oldLaneHeight = std::clamp(daw.timelineLaneHeight, 24.0f, 180.0f);
                float oldRowSpan = oldLaneHeight + layout.laneGap;
                float newLaneHeight = std::clamp(
                    static_cast<float>(daw.verticalRulerDragStartLaneHeight
                        * std::exp(-daw.verticalRulerDragAccumDX * 0.01)),
                    24.0f,
                    180.0f);
                float newRowSpan = newLaneHeight + layout.laneGap;
                float currentStartY = 100.0f + layout.scrollY + daw.timelineLaneOffset;
                float cursorRel = static_cast<float>(ui.cursorY) - currentStartY;
                float anchorDelta = 0.0f;
                if (oldRowSpan > 0.001f) {
                    anchorDelta = cursorRel * (1.0f - (newRowSpan / oldRowSpan));
                }
                daw.timelineLaneHeight = newLaneHeight;
                daw.timelineLaneOffset += anchorDelta + static_cast<float>(dy);
            }
        }

        if (allowLaneInput && ui.mainScrollDelta != 0.0
            && cursorInRect(ui, rulerLeft, rulerRight, rulerTopY, rulerBottomY)) {
            double zoomFactor = (ui.mainScrollDelta > 0.0) ? 1.1 : (1.0 / 1.1);
            double newSeconds = secondsPerScreen * zoomFactor;
            newSeconds = std::clamp(newSeconds, 2.0, 120.0);
            double cursorT = (laneRight > laneLeft)
                ? (static_cast<double>(ui.cursorX) - laneLeft) / (laneRight - laneLeft)
                : 0.0;
            cursorT = std::clamp(cursorT, 0.0, 1.0);
            double anchorTime = (static_cast<double>(daw.timelineOffsetSamples) / daw.sampleRate)
                + cursorT * secondsPerScreen;
            double newOffsetSec = anchorTime - cursorT * newSeconds;
            daw.timelineSecondsPerScreen = newSeconds;
            daw.timelineOffsetSamples = static_cast<int64_t>(std::llround(newOffsetSec * daw.sampleRate));
            DawLaneTimelineSystemLogic::ClampTimelineOffset(daw);
            ui.mainScrollDelta = 0.0;
        }

        if (allowLaneInput && ui.mainScrollDelta != 0.0
            && cursorInRect(ui, verticalRulerLeft, verticalRulerRight, verticalRulerTop, verticalRulerBottom)) {
            float oldLaneHeight = std::clamp(daw.timelineLaneHeight, 24.0f, 180.0f);
            float oldRowSpan = oldLaneHeight + layout.laneGap;
            float zoomFactor = (ui.mainScrollDelta > 0.0) ? 1.08f : (1.0f / 1.08f);
            float newLaneHeight = std::clamp(oldLaneHeight * zoomFactor, 24.0f, 180.0f);
            float newRowSpan = newLaneHeight + layout.laneGap;
            float currentStartY = 100.0f + layout.scrollY + daw.timelineLaneOffset;
            float cursorRel = static_cast<float>(ui.cursorY) - currentStartY;
            float anchorDelta = 0.0f;
            if (oldRowSpan > 0.001f) {
                anchorDelta = cursorRel * (1.0f - (newRowSpan / oldRowSpan));
            }
            daw.timelineLaneHeight = newLaneHeight;
            daw.timelineLaneOffset += anchorDelta;
            ui.mainScrollDelta = 0.0;
        }

        if (daw.playheadDragActive) {
            if (!ui.uiLeftDown) {
                daw.playheadDragActive = false;
            } else {
                float targetX = static_cast<float>(ui.cursorX) + daw.playheadDragOffsetX;
                float t = (laneRight > laneLeft) ? (targetX - laneLeft) / (laneRight - laneLeft) : 0.0f;
                t = std::clamp(t, 0.0f, 1.0f);
                double windowSamples = secondsPerScreen * static_cast<double>(daw.sampleRate);
                if (windowSamples < 0.0) windowSamples = 0.0;
                double offsetSamples = static_cast<double>(daw.timelineOffsetSamples);
                int64_t newSampleSigned = static_cast<int64_t>(std::llround(offsetSamples + t * windowSamples));
                if (newSampleSigned < 0) {
                    uint64_t shiftSamples = computeRebaseShiftSamples(daw, newSampleSigned);
                    DawTimelineRebaseLogic::ShiftTimelineRight(baseSystem, shiftSamples);
                    newSampleSigned += static_cast<int64_t>(shiftSamples);
                }
                uint64_t newSample = static_cast<uint64_t>(newSampleSigned);
                daw.playheadSample.store(newSample, std::memory_order_relaxed);
                ui.consumeClick = true;
            }
        }

        if (laneCount > 0 && allowLaneInput && daw.dragPending && ui.uiLeftDown) {
            float dy = std::abs(static_cast<float>(ui.cursorY) - daw.dragStartY);
            if (!daw.dragActive && dy > 4.0f) {
                daw.dragActive = true;
            }
        }

        if (laneCount > 0 && allowLaneInput && daw.dragActive) {
            float laneHeight = laneHalfH * 2.0f;
            float handleSize = std::min(kTrackHandleSize, std::max(14.0f, laneHeight));
            float dragRight = laneRight + kTrackHandleInset + handleSize + 4.0f;
            if (cursorInLaneRect(ui, laneLeft, dragRight, topBound - layout.laneGap, laneBottomBound + layout.laneGap)) {
                daw.dragDropIndex = dropSlotFromCursorY(static_cast<float>(ui.cursorY), layout.startY, rowSpan, laneCount);
            } else {
                daw.dragDropIndex = -1;
            }
        }

        if (!ui.uiLeftDown && (daw.dragPending || daw.dragActive)) {
            if (daw.dragActive && daw.dragDropIndex >= 0 && daw.dragLaneIndex >= 0) {
                int fromIndex = daw.dragLaneIndex;
                int toIndex = std::clamp(daw.dragDropIndex, 0, std::max(0, laneCount - 1));
                if (toIndex != fromIndex) {
                    if (DawTrackSystemLogic::MoveTrack(baseSystem, fromIndex, toIndex)) {
                        daw.selectedLaneIndex = toIndex;
                    }
                }
            }
            daw.dragPending = false;
            daw.dragActive = false;
            daw.dragLaneIndex = -1;
            daw.dragLaneType = -1;
            daw.dragLaneTrack = -1;
            daw.dragDropIndex = -1;
        }
        if (!ui.uiLeftDown && !daw.dragPending && !daw.dragActive
            && daw.selectedLaneIndex < 0 && daw.selectedLaneType < 0 && daw.selectedLaneTrack < 0) {
            daw.dragDropIndex = -1;
        }
    }
}
