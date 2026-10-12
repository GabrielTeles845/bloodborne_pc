import QtQuick
import QtQuick.Controls

// Buttons: `highlighted` is the one primary action on a surface (crimson), others are quiet
// outlines; `serif` sets the label in the display face (Play).
Button {
    id: button
    property bool primary: highlighted
    property bool serif: false
    font.family: serif ? Theme.serif : Theme.sans
    font.pixelSize: serif ? 20 : 13
    font.letterSpacing: serif ? 0.5 : 0.2
    hoverEnabled: true
    contentItem: Text {
        text: button.text
        font: button.font
        color: !button.enabled ? Theme.textFaint : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.radius
        color: button.primary ? (!button.enabled ? Theme.accentSoft : button.down ? "#a92a35" : button.hovered ? Theme.accentHover : Theme.accent)
                              : button.down ? Theme.surfaceHover : button.hovered ? Theme.surfaceHover : "transparent"
        border.color: button.primary ? "transparent" : button.hovered ? Theme.lineStrong : Theme.line
        border.width: 1
        Behavior on color { ColorAnimation { duration: 110 } }
    }
}
