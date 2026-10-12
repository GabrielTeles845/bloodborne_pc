import QtQuick
import QtQuick.Controls

Switch {
    id: control
    spacing: 12
    hoverEnabled: true
    contentItem: Text {
        text: control.text
        color: Theme.text
        font.family: Theme.sans
        font.pixelSize: 14
        leftPadding: control.indicator.width + control.spacing
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Rectangle {
        implicitWidth: 40
        implicitHeight: 22
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 11
        color: control.checked ? Theme.accent : (control.hovered ? "#2a3037" : "#22272d")
        border.color: control.checked ? Theme.accent : Theme.lineStrong
        Rectangle {
            width: 16; height: 16; radius: 8
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            color: control.checked ? "#fff6f2" : Theme.textDim
            Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }
        Behavior on color { ColorAnimation { duration: 110 } }
    }
}
