import QtQuick
import QtQuick.Controls

Button {
    id: button
    property bool primary: highlighted
    font.pixelSize: 12
    font.bold: primary
    font.letterSpacing: primary ? 1.2 : 0.3
    contentItem: Text {
        text: button.text
        font: button.font
        color: button.enabled ? "#f5f1ef" : "#84919a"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 7
        color: button.primary ? (button.down ? "#cf893e50" : button.hovered ? "#c4773041" : "#ad612432")
                              : button.down ? "#9a38454d" : button.hovered ? "#78414b53" : "#85202832"
        border.color: button.primary ? "#d89a5966" : "#6da9b5bc"
        border.width: 1
        Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: "#43ffffff" }
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
