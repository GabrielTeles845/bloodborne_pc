import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    font.family: Theme.sans
    font.pixelSize: 13
    hoverEnabled: true
    background: Rectangle {
        radius: Theme.radius
        color: control.pressed || control.popup.visible ? Theme.surfaceHover : control.hovered ? "#181c21" : Theme.surface
        border.color: control.activeFocus || control.popup.visible ? Theme.accent : control.hovered ? Theme.lineStrong : Theme.line
    }
    contentItem: Text {
        text: control.displayText
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        leftPadding: 14; rightPadding: 34
    }
    indicator: Canvas {
        width: 10; height: 6
        x: control.width - width - 15; y: (control.height - height) / 2
        onPaint: {
            const c = getContext("2d"); c.reset()
            c.strokeStyle = Theme.textDim; c.lineWidth = 1.5
            c.beginPath(); c.moveTo(0.5, 0.5); c.lineTo(5, 5); c.lineTo(9.5, 0.5); c.stroke()
        }
    }
    delegate: ItemDelegate {
        width: control.width
        height: 36
        text: modelData.text || ""
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: parent.text; leftPadding: 12; verticalAlignment: Text.AlignVCenter
            color: index === control.currentIndex ? Theme.text : "#cfcac3"
            font.family: Theme.sans; font.pixelSize: 13
        }
        background: Rectangle {
            color: parent.highlighted ? Theme.surfaceHover : "transparent"
            Rectangle { visible: index === control.currentIndex; width: 2; height: parent.height - 14; anchors.verticalCenter: parent.verticalCenter; color: Theme.accent }
        }
    }
    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 300)
        padding: 4
        contentItem: ListView { clip: true; implicitHeight: contentHeight; model: control.popup.visible ? control.delegateModel : null; currentIndex: control.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {} }
        background: Rectangle { color: "#15191e"; radius: Theme.radius; border.color: Theme.lineStrong }
    }
}
