import QtQuick
import QtQuick.Controls

Item {
    id: hub
    signal backToLauncher()
    signal destinationSelected(string category, string section, string key)
    property var categories: []
    property bool compact: height < 720
    property real sideMargin: Math.max(32, width * 0.043)
    property real heroHeight: Math.min(326, height * 0.39)
    property real cardsTop: Math.max(232, height * 0.36)
    property real cardGap: compact ? 12 : 16
    property real cardHeight: (height - cardsTop - 58 - 2 * cardGap) / 3
    function clearSearch() { search.text = "" }

    Rectangle { anchors.fill: parent; color: "#0b1015" }
    Image {
        x: 0; y: 0; width: parent.width; height: heroHeight
        source: Qt.resolvedUrl("../assets/ui/launcher-bridge.png")
        fillMode: Image.PreserveAspectCrop
        verticalAlignment: Image.AlignTop
        horizontalAlignment: Image.AlignHCenter
        smooth: true
    }
    Rectangle {
        x: 0; y: 0; width: parent.width; height: heroHeight + 40
        gradient: Gradient {
            GradientStop { position: 0; color: "#23091015" }
            GradientStop { position: 0.43; color: "#68091015" }
            GradientStop { position: 0.78; color: "#d8091015" }
            GradientStop { position: 1; color: "#0b1015" }
        }
    }
    Rectangle {
        x: 0; y: 0; width: parent.width; height: heroHeight
        gradient: Gradient { orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#6d091015" }
            GradientStop { position: 0.42; color: "#1b091015" }
            GradientStop { position: 1; color: "#4b091015" }
        }
    }

    Button {
        x: sideMargin - 5; y: heroHeight * 0.55
        width: 160; height: 34
        text: "‹   Launcher"
        onClicked: hub.backToLauncher()
        background: Item {}
        contentItem: Text {
            text: parent.text; color: "#e8e9eb"
            font.family: "Georgia"; font.pixelSize: hub.compact ? 17 : 19
            verticalAlignment: Text.AlignVCenter
        }
    }
    Text {
        x: sideMargin; y: heroHeight * 0.66
        text: "Settings"; color: "#f1ece8"
        font.family: "Georgia"; font.pixelSize: compact ? 47 : 57
    }

    Rectangle {
        id: searchBox
        x: parent.width - sideMargin - width
        y: heroHeight * 0.68
        width: Math.min(385, parent.width * 0.35); height: compact ? 43 : 47
        radius: 5; color: "#b70a1017"
        border.color: search.activeFocus ? "#d3a69a" : "#64717c86"
        border.width: 1
        Canvas {
            x: 14; anchors.verticalCenter: parent.verticalCenter
            width: 24; height: 24
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "#dce1e5"
                ctx.lineWidth = 1.7
                ctx.beginPath(); ctx.arc(10, 10, 6, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(14.5, 14.5); ctx.lineTo(21, 21); ctx.stroke()
            }
        }
        TextField {
            id: search
            anchors.fill: parent
            leftPadding: 48; rightPadding: 12
            placeholderText: "Find a setting"
            color: "#f1ece8"; placeholderTextColor: "#9aa5af"
            font.family: "Georgia"; font.pixelSize: compact ? 16 : 18
            font.italic: text === ""
            background: Item {}
        }
    }

    Flickable {
        x: sideMargin; y: cardsTop
        width: parent.width - 2 * sideMargin
        height: parent.height - cardsTop - 58
        clip: true; boundsBehavior: Flickable.StopAtBounds
        contentHeight: search.text.trim() === "" ? categoryGrid.height : resultsColumn.height
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Grid {
            id: categoryGrid
            visible: search.text.trim() === ""
            width: parent.width; columns: 2; spacing: cardGap
            Repeater {
                model: hub.categories
                delegate: Rectangle {
                    id: card
                    width: (categoryGrid.width - cardGap) / 2; height: cardHeight
                    radius: 6; clip: true
                    color: cardMouse.containsMouse ? "#19232c" : "#11191f"
                    border.color: cardMouse.containsMouse ? "#b28b8a" : "#49545d"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 130 } }
                    Image {
                        anchors.fill: parent
                        source: Qt.resolvedUrl("../art/hero.png")
                        fillMode: Image.PreserveAspectCrop; opacity: 0.075
                    }
                    Rectangle {
                        id: iconDisc
                        width: Math.min(82, card.height * 0.58)
                        height: width; radius: width / 2
                        x: hub.compact ? 22 : 29
                        anchors.verticalCenter: parent.verticalCenter
                        color: modelData.warm ? "#3c171d" : "#202a35"
                        border.color: modelData.warm ? "#a8525d" : "#6d8095"
                        border.width: 1
                        Image {
                            anchors.centerIn: parent
                            width: parent.width * 0.57; height: width
                            source: Qt.resolvedUrl("../assets/ui/" + modelData.icon + ".png")
                            fillMode: Image.PreserveAspectFit; smooth: true
                        }
                    }
                    Column {
                        x: iconDisc.x + iconDisc.width + (hub.compact ? 22 : 32)
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - x - 52
                        spacing: hub.compact ? 3 : 6
                        Text {
                            width: parent.width; text: modelData.name; color: "#f3f0ec"
                            font.family: "Georgia"; font.pixelSize: hub.compact ? 23 : 30
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width; text: modelData.detail; color: "#aeb9c5"
                            font.family: "Georgia"; font.pixelSize: hub.compact ? 13 : 16
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        anchors.right: parent.right; anchors.rightMargin: 22
                        anchors.verticalCenter: parent.verticalCenter
                        text: "›"; color: "#dce1e4"; font.family: "Georgia"
                        font.pixelSize: hub.compact ? 28 : 34
                    }
                    MouseArea {
                        id: cardMouse; anchors.fill: parent
                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: hub.destinationSelected(modelData.name, "", "")
                    }
                }
            }
        }
        Column {
            id: resultsColumn
            visible: search.text.trim() !== ""
            width: parent.width; spacing: 8
            property var matches: launcher.searchSettings(search.text)
            Text {
                visible: resultsColumn.matches.length === 0
                text: "No settings found. Try a different term."
                color: "#b4bec7"; font.pixelSize: 14
            }
            Repeater {
                model: resultsColumn.matches
                delegate: Rectangle {
                    width: resultsColumn.width; height: 54; radius: 5
                    color: resultMouse.containsMouse ? "#202b35" : "#141d25"
                    border.color: "#52616d"
                    Text { x: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: "#f1ece8"; font.pixelSize: 15 }
                    Text { anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.category + "  /  " + modelData.section + "  ›"; color: "#aeb9c5"; font.pixelSize: 12 }
                    MouseArea { id: resultMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: hub.destinationSelected(modelData.category, modelData.section, modelData.key) }
                }
            }
        }
    }

    Row {
        x: sideMargin; y: parent.height - 38
        width: parent.width - 2 * sideMargin; height: 23; spacing: 14
        Rectangle { width: Math.max(35, (parent.width - note.implicitWidth - 65) / 2); height: 1; color: "#39434b"; anchors.verticalCenter: parent.verticalCenter }
        Rectangle {
            width: 19; height: 19; radius: 10; color: "transparent"; border.color: "#87929c"
            Text { anchors.centerIn: parent; text: "i"; color: "#b5c0c8"; font.family: "Georgia"; font.pixelSize: 13; font.italic: true }
        }
        Text {
            id: note
            text: "Changes save automatically; some take effect after restart."
            color: "#aeb9c5"; font.family: "Georgia"; font.pixelSize: compact ? 11 : 13
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle { width: Math.max(35, (parent.width - note.implicitWidth - 65) / 2); height: 1; color: "#39434b"; anchors.verticalCenter: parent.verticalCenter }
    }
}
