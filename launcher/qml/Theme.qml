pragma Singleton
import QtQuick

// The launcher's design tokens, shared with the in-game guide's look (bbport_overlay.cpp):
// near-black panels over the game's art, warm off-white text, one crimson accent.
QtObject {
    readonly property color base: "#0b0d10"
    readonly property color panel: "#ee0e1115"
    readonly property color surface: "#14181d"
    readonly property color surfaceHover: "#1a1f25"
    readonly property color line: "#262b31"
    readonly property color lineStrong: "#363c43"
    readonly property color text: "#ece7df"
    readonly property color textDim: "#a39e97"
    readonly property color textFaint: "#6f6b66"
    readonly property color accent: "#c8323f"
    readonly property color accentHover: "#d9414e"
    readonly property color accentSoft: "#2a1216"
    readonly property color good: "#8fc59c"
    readonly property color warn: "#e3a878"

    readonly property string serif: "Georgia"
    readonly property string sans: "Segoe UI"

    readonly property int railWidth: 76
    readonly property int radius: 6
    readonly property int rowHeight: 60
}
