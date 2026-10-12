import QtQuick
import QtQuick.Controls

TextField {
    id: control
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectedTextColor: "#ffffff"
    selectionColor: "#8a2a33"
    font.family: Theme.sans
    font.pixelSize: 13
    leftPadding: 12
    hoverEnabled: true
    background: Rectangle {
        radius: Theme.radius
        color: Theme.surface
        border.color: control.activeFocus ? Theme.accent : control.hovered ? Theme.lineStrong : Theme.line
        border.width: 1
    }
}
