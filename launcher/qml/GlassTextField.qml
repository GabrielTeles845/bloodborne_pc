import QtQuick
import QtQuick.Controls

TextField {
    id: control
    color: "#f0f2f3"
    selectedTextColor: "#fff"
    selectionColor: "#9b4d6575"
    font.pixelSize: 12
    leftPadding: 10
    background: Rectangle {
        radius: 5
        color: "#9e18222b"
        border.color: control.activeFocus ? "#c8b66a77" : "#6199a8b0"
        border.width: 1
    }
}
