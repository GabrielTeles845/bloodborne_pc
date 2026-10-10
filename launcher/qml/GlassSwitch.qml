import QtQuick
import QtQuick.Controls

Switch {
    id: control
    spacing: 8
    contentItem: Text {
        text: control.text
        color: "#e9eef0"
        font.pixelSize: 13
        leftPadding: control.indicator.width + control.spacing
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Rectangle {
        implicitWidth: 43
        implicitHeight: 23
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 12
        color: control.checked ? "#a9427554" : "#70515a62"
        border.color: control.checked ? "#c79cdda9" : "#8c9ca7ae"
        Rectangle {
            width: 17; height: 17; radius: 9
            x: control.checked ? parent.width - width - 4 : 3
            y: 3
            color: control.checked ? "#daf8df" : "#d2d8dc"
            Behavior on x { NumberAnimation { duration: 130 } }
        }
    }
}
