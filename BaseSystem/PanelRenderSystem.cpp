#pragma once
#include "Host/PlatformInput.h"

namespace FontSystemLogic {
    void RenderFontsTimeline(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
    void RenderFontsSideButtons(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
    void RenderFontsTopButtons(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
    void CleanupFonts(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
}
namespace PanelSystemLogic {
    void RenderSidePanels(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
    void RenderTopBottomPanels(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
}
namespace ButtonSystemLogic {
    void RenderButtonsSide(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
    void RenderButtonsTopBottom(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle);
}
namespace DecibelMeterSystemLogic { void RenderDecibelMeters(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle); }
namespace DawFaderSystemLogic { void RenderDawFaders(BaseSystem&, std::vector<Entity>&, float, PlatformWindowHandle); }

namespace PanelRenderSystemLogic {
    void Step01_RenderFontsTimeline(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        FontSystemLogic::RenderFontsTimeline(baseSystem, prototypes, dt, win);
    }

    void Step02_RenderSidePanels(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        PanelSystemLogic::RenderSidePanels(baseSystem, prototypes, dt, win);
    }

    void Step03_RenderButtonsSide(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        ButtonSystemLogic::RenderButtonsSide(baseSystem, prototypes, dt, win);
    }

    void Step04_RenderFontsSideButtons(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        FontSystemLogic::RenderFontsSideButtons(baseSystem, prototypes, dt, win);
    }

    void Step05_RenderTopBottomPanels(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        PanelSystemLogic::RenderTopBottomPanels(baseSystem, prototypes, dt, win);
    }

    void Step05a_RenderDecibelMeters(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        DecibelMeterSystemLogic::RenderDecibelMeters(baseSystem, prototypes, dt, win);
    }

    void Step05b_RenderDawFaders(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        DawFaderSystemLogic::RenderDawFaders(baseSystem, prototypes, dt, win);
    }

    void Step06_RenderButtonsTopBottom(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        ButtonSystemLogic::RenderButtonsTopBottom(baseSystem, prototypes, dt, win);
    }

    void Step07_RenderFontsTopButtons(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        FontSystemLogic::RenderFontsTopButtons(baseSystem, prototypes, dt, win);
    }

    void CleanupFonts(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        FontSystemLogic::CleanupFonts(baseSystem, prototypes, dt, win);
    }
}
