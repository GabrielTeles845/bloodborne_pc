import QtQuick

// A row of the reference menu: icon, serif label, an optional current value, chevron. The
// selected row has the crimson outline; hover lifts it.
Rectangle {
    id: row
    property string icon: ""
    property string label: ""
    property string value: ""
    property color valueColor: Theme.textDim
    property bool selected: false
    property bool chevron: true
    signal clicked()
    height: 64; radius: Theme.radius
    color: hit.containsMouse || selected ? "#16191d" : "#111418"
    border.color: selected ? Theme.accent : hit.containsMouse ? Theme.lineStrong : Theme.line
    border.width: selected ? 1.5 : 1
    Behavior on color { ColorAnimation { duration: 100 } }

    NavIcon {
        id: glyph; visible: row.icon !== ""
        x: 22; anchors.verticalCenter: parent.verticalCenter; width: 24; height: 24
        kind: row.icon; strokeColor: "#d6d1c9"
    }
    Text {
        x: row.icon !== "" ? 64 : 22; anchors.verticalCenter: parent.verticalCenter
        width: parent.width - x - valueText.width - 60
        text: row.label; color: Theme.text; elide: Text.ElideRight
        font.family: Theme.serif; font.pixelSize: 19
    }
    Text {
        id: valueText
        anchors.right: parent.right; anchors.rightMargin: row.chevron ? 46 : 22; anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, row.width * 0.42)
        text: row.value; color: row.valueColor; elide: Text.ElideRight
        font.family: Theme.sans; font.pixelSize: 13
    }
    NavIcon {
        visible: row.chevron
        anchors.right: parent.right; anchors.rightMargin: 20; anchors.verticalCenter: parent.verticalCenter
        width: 16; height: 16; kind: "chevron"; strokeColor: "#cfcac2"; stroke: 1.8
    }
    MouseArea { id: hit; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: row.clicked() }
}
