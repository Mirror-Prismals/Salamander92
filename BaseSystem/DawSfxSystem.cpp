#pragma once

#include <algorithm>
#include <fstream>
#include <vector>

namespace DawSfxSystemLogic {

    namespace {
        bool canTrigger(double now, double lastTime, float cooldownSeconds) {
            return cooldownSeconds <= 0.0f || (now - lastTime) >= static_cast<double>(cooldownSeconds);
        }

        bool triggerTransientScript(BaseSystem& baseSystem, const std::string& scriptPath) {
            if (!baseSystem.audio || !baseSystem.audio->chuck) return false;

            std::ifstream f(scriptPath);
            if (!f.is_open()) {
                std::cerr << "DawSfxSystem: missing script '" << scriptPath << "'" << std::endl;
                return false;
            }

            std::vector<t_CKUINT> ids;
            std::lock_guard<std::mutex> chuckLock(baseSystem.audio->chuck_vm_mutex);
            bool ok = baseSystem.audio->chuck->compileFile(scriptPath, "", 1, FALSE, &ids);
            if (!ok || ids.empty()) {
                std::cerr << "DawSfxSystem: failed to compile '" << scriptPath << "'" << std::endl;
                return false;
            }
            return true;
        }
    }

    void QueueButtonClick(BaseSystem& baseSystem) {
        if (!baseSystem.dawSfx) return;
        DawSfxContext& dawSfx = *baseSystem.dawSfx;
        dawSfx.pendingButtonClickCount = std::min(dawSfx.pendingButtonClickCount + 1, 8);
    }

    void QueueOpen(BaseSystem& baseSystem) {
        if (!baseSystem.dawSfx) return;
        DawSfxContext& dawSfx = *baseSystem.dawSfx;
        dawSfx.pendingOpenCount = std::min(dawSfx.pendingOpenCount + 1, 4);
    }

    void QueueClose(BaseSystem& baseSystem) {
        if (!baseSystem.dawSfx) return;
        DawSfxContext& dawSfx = *baseSystem.dawSfx;
        dawSfx.pendingCloseCount = std::min(dawSfx.pendingCloseCount + 1, 4);
    }

    void UpdateDawSfx(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        (void)prototypes;
        (void)dt;
        (void)win;

        if (!baseSystem.audio || !baseSystem.audio->chuck || !baseSystem.dawSfx) return;

        DawSfxContext& dawSfx = *baseSystem.dawSfx;
        const int pendingCloseCount = dawSfx.pendingCloseCount;
        const int pendingOpenCount = dawSfx.pendingOpenCount;
        const int pendingButtonClickCount = dawSfx.pendingButtonClickCount;
        dawSfx.pendingCloseCount = 0;
        dawSfx.pendingOpenCount = 0;
        dawSfx.pendingButtonClickCount = 0;

        if (pendingCloseCount <= 0 && pendingOpenCount <= 0 && pendingButtonClickCount <= 0) return;

        const double now = PlatformInput::GetTimeSeconds();
        constexpr float kButtonCooldownSeconds = 0.025f;
        constexpr float kOpenCloseCooldownSeconds = 0.060f;

        if (pendingCloseCount > 0
            && canTrigger(now, dawSfx.lastCloseTime, kOpenCloseCooldownSeconds)
            && triggerTransientScript(baseSystem, dawSfx.closeScriptPath)) {
            dawSfx.lastCloseTime = now;
        }

        if (pendingOpenCount > 0
            && canTrigger(now, dawSfx.lastOpenTime, kOpenCloseCooldownSeconds)
            && triggerTransientScript(baseSystem, dawSfx.openScriptPath)) {
            dawSfx.lastOpenTime = now;
        }

        if (pendingButtonClickCount > 0
            && canTrigger(now, dawSfx.lastButtonClickTime, kButtonCooldownSeconds)
            && triggerTransientScript(baseSystem, dawSfx.buttonClickScriptPath)) {
            dawSfx.lastButtonClickTime = now;
        }
    }
}
