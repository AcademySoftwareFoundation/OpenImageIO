// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <imgui.h>

namespace Imiv::UiMetrics {

// Shared spacing for regular app windows. Menus intentionally keep the Dear
// ImGui defaults so menu bars and popups don't inherit tighter content
// spacing.
inline constexpr ImVec2 kAppFramePadding = ImVec2(4.0f, 4.0f);
inline constexpr ImVec2 kAppItemSpacing  = ImVec2(4.0f, 4.0f);

// Shared padding used by the standard auxiliary windows.
inline constexpr ImVec2 kAuxWindowPadding = ImVec2(10.0f, 10.0f);

// Shared status-bar spacing used inside image windows.
inline constexpr ImVec2 kStatusBarPadding     = ImVec2(8.0f, 4.0f);
inline constexpr ImVec2 kStatusBarCellPadding = ImVec2(8.0f, 4.0f);

namespace AuxiliaryWindows {

    // Default first-use offsets and sizes for the dockable auxiliary windows.
    inline constexpr ImVec2 kInfoOffset        = ImVec2(72.0f, 72.0f);
    inline constexpr ImVec2 kInfoSize          = ImVec2(360.0f, 600.0f);
    inline constexpr ImVec2 kPreferencesOffset = ImVec2(740.0f, 72.0f);
    inline constexpr ImVec2 kPreferencesSize   = ImVec2(360.0f, 700.0f);
    inline constexpr ImVec2 kPreviewOffset     = ImVec2(1030.0f, 72.0f);
    inline constexpr ImVec2 kPreviewSize       = ImVec2(300.0f, 360.0f);

    // Shared body sizing and small layout gaps inside auxiliary windows.
    inline constexpr float kBodyBottomGap            = 4.0f;
    inline constexpr float kInfoBodyMinHeight        = 100.0f;
    inline constexpr float kPreferencesBodyMinHeight = 120.0f;
    inline constexpr float kPreviewBodyMinHeight     = 120.0f;
    inline constexpr float kInfoCloseGap             = 3.0f;
    inline constexpr float kPreferencesCloseGap      = 4.0f;

    // Shared auxiliary-window table widths and empty-state padding.
    inline constexpr float kInfoTableLabelWidth  = 120.0f;
    inline constexpr ImVec2 kEmptyMessagePadding = ImVec2(8.0f, 8.0f);

}  // namespace AuxiliaryWindows

namespace Preferences {

    // Preferences form layout and compact right-aligned stepper/button sizes.
    inline constexpr float kSectionSeparatorTextPaddingY = 1.0f;
    inline constexpr float kLabelColumnWidth             = 150.0f;
    inline constexpr float kStepperButtonWidth           = 22.0f;
    inline constexpr float kStepperValueWidth            = 38.0f;
    inline constexpr float kOcioBrowseButtonWidth        = 64.0f;
    inline constexpr float kCloseButtonWidth             = 72.0f;

}  // namespace Preferences

namespace Preview {

    // Compact form widths used by the preview controls tool window.
    inline constexpr float kLabelColumnWidth = 90.0f;

}  // namespace Preview

namespace ImageList {

    // Shared defaults for the docked image library panel.
    inline constexpr float kDockTargetWidth    = 200.0f;
    inline constexpr float kDockMinRatio       = 0.12f;
    inline constexpr float kDockMaxRatio       = 0.35f;
    inline constexpr ImVec2 kFloatingOffset    = ImVec2(24.0f, 72.0f);
    inline constexpr ImVec2 kDefaultWindowSize = ImVec2(200.0f, 420.0f);

}  // namespace ImageList

namespace PixelCloseup {

    // Fixed geometry for the closeup preview overlay.
    inline constexpr float kWindowSize        = 260.0f;
    inline constexpr float kFollowMouseOffset = 15.0f;
    inline constexpr float kCornerPadding     = 5.0f;
    inline constexpr float kTextPadX          = 10.0f;
    inline constexpr float kTextPadY          = 8.0f;
    inline constexpr float kTextLineGap       = 2.0f;
    inline constexpr float kTextToWindowGap   = 2.0f;
    inline constexpr float kFontSize          = 13.5f;

}  // namespace PixelCloseup

namespace AreaProbe {

    // Fixed text-panel spacing for the area-probe overlay.
    inline constexpr float kPadY         = 8.0f;
    inline constexpr float kLineGap      = 2.0f;
    inline constexpr float kBorderMargin = 9.0f;

}  // namespace AreaProbe

namespace OverlayPanel {

    // Shared styling for text-only overlay panels such as status popups.
    inline constexpr float kPadX             = 10.0f;
    inline constexpr float kPadY             = 8.0f;
    inline constexpr float kLineGap          = 2.0f;
    inline constexpr float kCornerRounding   = 4.0f;
    inline constexpr float kBorderThickness  = 1.0f;
    inline constexpr float kCornerMarkerSize = 4.0f;

}  // namespace OverlayPanel

namespace ImageView {

    // Shared image-window geometry limits.
    inline constexpr float kViewportMinHeight = 64.0f;

}  // namespace ImageView

namespace StatusBar {

    // Fixed status-bar sizing and column widths inside the image window.
    inline constexpr float kMinHeight        = 30.0f;
    inline constexpr float kExtraHeight      = 8.0f;
    inline constexpr float kLoadColumnWidth  = 140.0f;
    inline constexpr float kMouseColumnWidth = 150.0f;

}  // namespace StatusBar

}  // namespace Imiv::UiMetrics
