import QtQuick
import QtQuick.Window

// Shown instead of the launcher during a direct launch. It never takes focus from the game.
Window {
    id: splash
    objectName: "launchSplash"
    signal openLauncher()
    visible: true
    width: 372; height: 92
    x: Screen.virtualX + (Screen.desktopAvailableWidth - width) / 2
    y: Screen.virtualY + Screen.desktopAvailableHeight - height - 72
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowDoesNotAcceptFocus
    color: "transparent"
    title: "Bloodborne"

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: "#e60b1017"
        border.color: "#65e0e8ee"
        Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: "#43ffffff" }
        Rectangle { x: 14; anchors.verticalCenter: parent.verticalCenter; width: 3; height: 42; radius: 2; color: "#bf475b" }
        Image {
            x: 30; anchors.verticalCenter: parent.verticalCenter
            width: 44; height: 44
            source: Qt.resolvedUrl("../assets/hunter-icon.png")
            fillMode: Image.PreserveAspectCrop
            smooth: true; mipmap: true
        }
        Column {
            x: 88; anchors.verticalCenter: parent.verticalCenter; spacing: 5
            Text { text: "Launching Bloodborne…"; color: "#f3f1ee"; font.family: "Georgia"; font.pixelSize: 19 }
            Text { text: "Hold Shift for launcher"; color: "#aeb5bd"; font.pixelSize: 12; font.letterSpacing: 0.4 }
        }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: splash.openLauncher() }
    }
}
