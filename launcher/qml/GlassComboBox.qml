import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    font.pixelSize: 12
    background: Rectangle {
        radius: 5; color: control.pressed ? "#b62b343e" : control.hovered ? "#a32a3640" : "#95202b35"
        border.color: control.activeFocus ? "#cfbd737e" : "#6f9eacb4"
    }
    contentItem: Text {
        text: control.displayText
        color: "#eef1f2"
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        leftPadding: 12; rightPadding: 33
    }
    indicator: Text {
        text: "▾"; color: "#e4e9eb"; font.pixelSize: 17
        x: control.width - width - 13; y: (control.height - height) / 2
    }
    delegate: ItemDelegate {
        width: control.width
        height: 35
        text: modelData.text || ""
        highlighted: control.highlightedIndex === index
        contentItem: Text { text: parent.text; color: "#edf0f1"; leftPadding: 11; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12 }
        background: Rectangle { color: parent.highlighted ? "#804f2c38" : "#e81a232d" }
    }
    popup: Popup {
        y: control.height + 3
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 4, 270)
        padding: 2
        contentItem: ListView { clip: true; implicitHeight: contentHeight; model: control.popup.visible ? control.delegateModel : null; currentIndex: control.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {} }
        background: Rectangle { color: "#eb141c25"; radius: 6; border.color: "#a083919a" }
    }
}
