import QtQuick

// A launcher toggle laid out like a setting row: label (and hint) left, switch right.
Item {
    id: row
    property string label: ""
    property string hint: ""
    property bool checked: false
    signal toggled(bool checked)
    width: parent ? parent.width : 400
    height: Math.max(58, texts.implicitHeight + 26)
    Column {
        id: texts; x: 2; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 80; spacing: 3
        Text { width: parent.width; text: row.label; color: Theme.text; font.family: Theme.sans; font.pixelSize: 14; elide: Text.ElideRight }
        Text { visible: row.hint !== ""; width: parent.width; text: row.hint; color: Theme.textDim; font.family: Theme.sans; font.pixelSize: 12; wrapMode: Text.WordWrap }
    }
    GlassSwitch {
        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
        checked: row.checked
        onToggled: row.toggled(checked)
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
}
