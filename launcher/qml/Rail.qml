import QtQuick

// The left icon rail (as the in-game guide's): the hunter mark opens Play, the gear Settings.
Rectangle {
    id: rail
    property string current: "Home"
    property bool locked: false   // during first-time setup the rail only shows the mark
    signal navigate(string page)
    width: Theme.railWidth
    color: "#e6090b0e"
    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.line }

    Column {
        y: 26; width: parent.width; spacing: 0
        Repeater {
            model: rail.locked ? [{kind: "hunter", page: "Setup", tip: "Setup"}]
                               : [{kind: "hunter", page: "Home", tip: "Play"},
                                  {kind: "settings", page: "Settings", tip: "Settings"}]
            delegate: Item {
                width: rail.width; height: 76
                property bool active: rail.current === modelData.page
                Rectangle { visible: active; x: 0; y: 8; width: 3; height: parent.height - 16; color: Theme.accent }
                Rectangle {
                    anchors.centerIn: parent; width: 52; height: 52; radius: 4
                    color: active ? "#1d1316" : hit.containsMouse ? "#15181c" : "transparent"
                }
                NavIcon {
                    anchors.centerIn: parent; width: 30; height: 30; kind: modelData.kind
                    strokeColor: active ? Theme.text : hit.containsMouse ? "#d9d4cc" : "#8f8a84"
                    stroke: modelData.kind === "hunter" ? 1.3 : 1.6
                }
                Rectangle { anchors.bottom: parent.bottom; x: 18; width: parent.width - 36; height: 1; color: Theme.line }
                MouseArea {
                    id: hit; anchors.fill: parent; hoverEnabled: true
                    cursorShape: rail.locked ? Qt.ArrowCursor : Qt.PointingHandCursor
                    onClicked: if (!rail.locked) rail.navigate(modelData.page)
                }
                ToolTipText { visible: hit.containsMouse && !rail.locked; text: modelData.tip }
            }
        }
    }
    Text {
        anchors.bottom: parent.bottom; anchors.bottomMargin: 18; anchors.horizontalCenter: parent.horizontalCenter
        text: launcher.version; color: Theme.textFaint; font.family: Theme.sans; font.pixelSize: 10
    }
}
