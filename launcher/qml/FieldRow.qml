import QtQuick
import QtQuick.Controls

Rectangle {
    id: fieldRow
    property var field: ({})
    property string settingKey: field.key || ""
    property bool hasHint: (field.hint || "") !== ""
    height: (field.kind === "toggle" ? 46 : field.kind === "folder" ? 73 : 64) + (hasHint && field.kind === "toggle" ? 14 : 0)
    radius: 7
    color: "#5a202a34"
    border.color: "#36ffffff"
    Text {
        id: label
        x: 15; y: field.kind === "toggle" ? (hasHint ? 10 : 15) : 9
        width: parent.width * 0.41
        text: field.label || ""
        color: "#e7edf1"
        font.pixelSize: 13
        elide: Text.ElideRight
    }
    // What the setting does (graphics rows): under the label, beside the control.
    Text {
        visible: hasHint
        x: 15; y: label.y + label.height + 3
        width: field.kind === "toggle" ? parent.width - 110 : parent.width * 0.40
        text: field.hint || ""
        color: "#9eabb3"; font.pixelSize: 11
        elide: Text.ElideRight
        maximumLineCount: field.kind === "toggle" ? 1 : 2
        wrapMode: Text.WordWrap
    }
    GlassSwitch {
        visible: field.kind === "toggle"
        anchors.right: parent.right; anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        checked: launcher.checked(settingKey)
        onToggled: launcher.setValue(settingKey, checked)
    }
    GlassComboBox {
        id: selection
        visible: field.kind === "choice"
        x: parent.width * 0.42; y: 12; width: parent.width * 0.54; height: 41
        model: field.options || []
        textRole: "text"
        valueRole: "value"
        Component.onCompleted: currentIndex = indexOfValue(String(launcher.value(settingKey)))
        onActivated: launcher.setValue(settingKey, currentValue)
    }
    GlassTextField {
        id: entry
        visible: field.kind !== "toggle" && field.kind !== "choice"
        x: field.kind === "folder" ? 15 : parent.width * 0.42
        y: field.kind === "folder" ? 32 : 12
        width: field.kind === "folder" ? parent.width - 126 : parent.width * 0.54
        height: 35
        text: String(launcher.value(settingKey))
        echoMode: field.kind === "password" ? TextInput.Password : TextInput.Normal
        onEditingFinished: launcher.setValue(settingKey, text)
    }
    GlassButton {
        visible: field.kind === "folder"
        text: "Browse"
        anchors.right: parent.right; anchors.rightMargin: 15
        y: 31; height: 36; width: 88
        onClicked: launcher.browse(settingKey)
    }
}
