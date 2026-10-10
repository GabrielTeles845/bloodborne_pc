import QtQuick

Rectangle {
    id: card
    property string heading: ""
    property string icon: "home"
    property string content: ""
    radius: 12
    color: "#a90d141c"
    border.color: "#65e0e8ee"
    border.width: 1
    Rectangle { x: 1; y: 1; width: parent.width - 2; height: 42; radius: 11; color: "#1effffff" }
    Row {
        x: 20; y: 18; spacing: 12
        NavIcon { kind: card.icon; width: 21; height: 21; strokeColor: "#c6d0d6" }
        Text { text: card.heading; color: "#e7edf1"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.6; anchors.verticalCenter: parent.verticalCenter }
    }
    Rectangle { x: 20; y: 55; width: parent.width - 40; height: 1; color: "#50ffffff" }
    Text { x: 20; y: 72; width: parent.width - 40; height: Math.max(0, parent.height - 85); clip: true; wrapMode: Text.WordWrap; text: card.content; color: "#d4dde0"; font.pixelSize: 13; lineHeight: 1.25 }
}
