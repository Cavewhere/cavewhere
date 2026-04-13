import QtQuick
import QtQuick.Controls as QC
import cavewherelib

pragma Singleton

QtObject {
    id: theme

    // Track the OS/application color scheme
    readonly property bool dark: Qt.application.styleHints.colorScheme === Qt.Dark

    // Base palette hook using the active system palette
    readonly property SystemPalette palette: SystemPalette { colorGroup: SystemPalette.Active }

    // Core surfaces/text
    readonly property color background: palette.window
    readonly property color surface: dark ? "#1f232a" : "#ffffff"
    readonly property color surfaceMuted: dark ? "#292d35" : "#f6f6f6"
    readonly property color surfaceRaised: dark ? "#242933" : "#f0f0f0"
    // readonly property color sidebar: dark ? "#141414" : "#f4f4f4"
    readonly property color text: palette.text
    readonly property color textSecondary: dark ? "#cdd2db" : "#33363a"
    readonly property color textSubtle: dark ? "#9fa6b1" : "#616469"
    readonly property color textInverse: dark ? "#111318" : "#f5f5f5"
    readonly property color textLink: dark ? "#85c1f4" : "#1d4d77"

    // Accents & states
    readonly property color accent: palette.accent
    readonly property color accentMuted: "#8AC6FF"
    readonly property color success: dark ? "#76e596" : "#4caf50"
    readonly property color warning: dark ? "#6b643e" : "#FF9C14"
    readonly property color danger: dark ? "#6f312e" : "#FF6736"
    readonly property color info: dark ? "#1f3f61" : "#85c1f4"
    readonly property color highlight: dark ? "#314f78" : "#a5cdff"
    readonly property color icon: palette.buttonText
    readonly property color tag: dark ? "#656565" : border

    // Lines and outlines
    readonly property color border: dark ? "#4a4f58" : "#d3d3d3"
    readonly property color borderSubtle: dark ? "#353a42" : "#e4e4e4"
    readonly property color divider: "#141414" // dark ? #2c3138" : "#d8d8d8"

    // Diff colors
    readonly property color diffAddedBackground: dark ? "#1a3626" : "#dafbe1"
    readonly property color diffDeletedBackground: dark ? "#3d1f1f" : "#ffebe9"
    readonly property color diffHunkBackground: dark ? "#1c2d4d" : "#ddf4ff"
    readonly property color diffAddedText: dark ? "#76e596" : "#1a7f37"
    readonly property color diffDeletedText: dark ? "#f47067" : "#cf222e"
    readonly property color diffHunkText: dark ? "#85c1f4" : "#0969da"
    readonly property color diffContextBackground: "transparent"

    // Git graph lane colors (8-entry cycling palette)
    readonly property list<color> laneColors: [
        "#4dc9f6", "#f67019", "#f53794", "#537bc4",
        "#acc236", "#166a8f", "#00a950", "#58595b"
    ]

    // Typography — driven by cwFontSettings; scale all sizes proportionally
    readonly property real fontScale: RootData.settings.fontSettings.fontBaseSize / 16.0
    readonly property string fontFamily: RootData.settings.fontSettings.fontFamily !== ""
        ? RootData.settings.fontSettings.fontFamily
        : Qt.application.font.family
    readonly property string fontFamilyBody: Qt.application.font.family
    readonly property string fontFamilyMono: "Courier Prime"

    readonly property int fontSizeCaption: Math.round(11 * fontScale)
    readonly property int fontSizeSmall:   Math.round(12 * fontScale)
    readonly property int fontSizeBody:    Math.round(14 * fontScale)
    readonly property int fontSizeUI:      RootData.settings.fontSettings.fontBaseSize
    readonly property int fontSizeMedium:  Math.round(18 * fontScale)
    readonly property int fontSizeTitle:   Math.round(20 * fontScale)
    readonly property int fontSizeLarge:   Math.round(24 * fontScale)
    readonly property int fontSizeXLarge:  Math.round(30 * fontScale)

    // Responsive layout tiers
    enum LayoutSize { Narrow, Medium, Wide }

    // Responsive breakpoints (window width in pixels)
    readonly property int breakpointWide: 800
    readonly property int breakpointMedium: 500

    // Sidebar dimensions per tier
    readonly property int sidebarWidthFull: 80
    readonly property int sidebarWidthCompact: 50

    // Icon sizes
    readonly property int iconSizeSmall: 24
    readonly property int iconSizeMedium: 32

    // Spacing
    readonly property int pageMargin: 8

    // Utility
    readonly property color transparent: "#00000000"
    readonly property color shadow: dark ? "#33000000" : "#22000000"
    readonly property color focusRing: dark ? "#b0d3ff" : "#5a9bff"

    // Legacy values mapped from the previous Theme.js
    readonly property color floatingWidgetColor: dark ? "#2b3038" : "#DDDDDD"
    readonly property real floatingWidgetRadius: 3
    readonly property color errorBackground: danger

    // Sidebar-specific palette (original colors retained)
    readonly property QtObject sidebar: QtObject {
        readonly property color background: palette.window
        readonly property color gradientTop: "#1b2331"
        readonly property color gradientBottom: dark ? theme.surface : "#616469"
        readonly property color panel: palette.window
        readonly property color divider: theme.divider //"#141414"
        readonly property color hoverStart: "#00d1d1d1"
        readonly property color hoverMid: "#96b5b5b5"
        readonly property color hoverMidHover: "#32b5b5b5"
        readonly property color toggledStart: "#ffffff"
        readonly property color toggledMid: "#000000"
        readonly property color toggledEnd: "#c8c0c0c0"
        readonly property color text: "#ffffff"
        readonly property color textActive: "#000000"
        readonly property color textStroke: "#aaaaaa"
        readonly property color borderActive: "#313131"
        readonly property color borderHover: "#ffffff"
    }
}
