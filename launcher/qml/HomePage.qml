import QtQuick
import QtQuick.Controls

Item {
    id: home
    signal primaryRequested()
    signal openSettings(string category, string section)

    property bool gameBlocked: false
    property bool savesBlocked: false
    property bool onlineBlocked: false
    property bool blocked: gameBlocked || savesBlocked || onlineBlocked || launcher.setupRunning
    property bool controllerDetected: false
    property string statusTitle: launcher.running ? "The hunt is underway" :
                                 launcher.setupRunning ? "Preparing game files" :
                                 gameBlocked ? "Game files need attention" :
                                 savesBlocked ? "Saves need attention" :
                                 onlineBlocked ? "Online account needs attention" : "Ready to play"
    property color cream: "#f3eee6"
    property color muted: "#bbc5cf"

    Component.onCompleted: controllerDetected = launcher.controllerConnected()
    Timer {
        interval: 3000; running: home.visible; repeat: true
        onTriggered: home.controllerDetected = launcher.controllerConnected()
    }

    Rectangle { anchors.fill: parent; color: "#090d12" }
    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("../assets/ui/launcher-bridge.png")
        fillMode: Image.PreserveAspectCrop
        smooth: true
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "#ae080c11" }
            GradientStop { position: 0.43; color: "#99080c11" }
            GradientStop { position: 1; color: "#f4080c11" }
        }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient { orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#b8080c11" }
            GradientStop { position: 0.50; color: "#72080c11" }
            GradientStop { position: 1; color: "#66080c11" }
        }
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: pageColumn.implicitHeight + 42
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: pageColumn
            x: 25; y: 20; width: scroller.width - 50
            spacing: 13

            Item {
                width: parent.width; height: 273
                Text {
                    x: 5; y: 0; text: "THE HUNT BEGINS"; color: "#c8d0d8"
                    font.pixelSize: 10; font.bold: true; font.letterSpacing: 2.6
                }
                Text {
                    anchors.right: parent.right; y: 0
                    text: "CUSA03173  ·  LOCAL PC BUILD"; color: "#aab7c2"
                    font.pixelSize: 10; font.letterSpacing: 1.6
                }
                Text {
                    x: 4; y: 40; text: "Bloodborne"; color: "#f2e8d5"
                    font.family: "Georgia"; font.pixelSize: 67
                }
                Text {
                    x: 8; y: 133; width: Math.min(650, parent.width * 0.68)
                    text: "A journey to Yharnam. Uncover its secrets, face its nightmares, and survive the night."
                    color: "#dce2e7"; font.pixelSize: 15; wrapMode: Text.WordWrap
                }
                Row {
                    x: 7; y: 204; spacing: 12
                    GlassButton {
                        width: 210; height: 51; highlighted: true
                        text: launcher.running ? "GAME RUNNING" : launcher.setupRunning ? "INSTALLING…" :
                              gameBlocked ? "FIX GAME FILES" : savesBlocked ? "FIX SAVES" :
                              onlineBlocked ? "FIX ONLINE ACCOUNT" : "▶   PLAY"
                        enabled: !launcher.running && !launcher.setupRunning
                        onClicked: home.primaryRequested()
                    }
                    GlassButton {
                        width: 190; height: 51; text: "GAME OPTIONS"
                        onClicked: home.openSettings("", "")
                    }
                    GlassButton {
                        visible: launcher.running; width: 112; height: 51; text: "STOP"
                        onClicked: launcher.stop()
                    }
                }
            }

            Rectangle {
                width: parent.width; height: 100; radius: 9
                color: "#dc0d141b"; border.color: "#65778691"
                Text {
                    x: 16; y: 12; text: "LAUNCH OPTIONS"; color: "#9bd3b5"
                    font.pixelSize: 10; font.bold: true; font.letterSpacing: 1.8
                }
                Row {
                    x: 16; y: 35; spacing: 13
                    GlassSwitch {
                        width: 270; height: 28; text: "Launch directly on startup"
                        checked: launcher.launchDirectly
                        onToggled: launcher.setValue("launchDirectly", checked)
                    }
                    GlassSwitch {
                        width: 250; height: 28; text: "Close launcher after Play"
                        checked: launcher.closeOnPlay
                        onToggled: launcher.setValue("close_on_play", checked)
                    }
                    GlassButton {
                        width: 142; height: 30; text: "Desktop shortcut"
                        onClicked: launcher.createShortcut()
                    }
                    GlassButton {
                        width: 117; height: 30; text: "Add to Steam"
                        onClicked: launcher.addToSteam()
                    }
                }
                Text {
                    x: 16; y: 77; text: "Hold Shift or Ctrl while starting to open this launcher."
                    color: muted; font.pixelSize: 11
                }
            }

            Row {
                width: parent.width; spacing: 12
                property real cardWidth: (width - 24) / 3

                Rectangle {
                    width: parent.cardWidth; height: 206; radius: 10
                    color: "#db111a23"; border.color: "#6374828e"
                    Text { x: 17; y: 17; text: "GAME STATUS"; color: cream; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7 }
                    Rectangle { x: 17; y: 44; width: parent.width - 34; height: 1; color: "#6877818a" }
                    Text { x: 17; y: 59; width: parent.width - 34; text: home.statusTitle; color: home.blocked ? "#eab893" : "#b8dfc7"; font.pixelSize: 15; font.bold: true; elide: Text.ElideRight }
                    Text { x: 17; y: 87; width: parent.width - 34; text: launcher.gameStatus; color: cream; font.pixelSize: 12; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                    Text { x: 17; y: 132; width: parent.width - 34; text: home.savesBlocked ? "Saves need attention" : launcher.saveStatus; color: home.savesBlocked ? "#eab893" : muted; font.pixelSize: 12; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                    Text { x: 17; y: 181; text: "Game files & saves  ›"; color: "#d9b6a6"; font.pixelSize: 11 }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: home.openSettings("Game & Saves", home.savesBlocked && !home.gameBlocked ? "Your data" : "Game files") }
                }
                Rectangle {
                    width: parent.cardWidth; height: 206; radius: 10
                    color: "#db111a23"; border.color: "#6374828e"
                    Text { x: 17; y: 17; text: "CURRENT SETUP"; color: cream; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7 }
                    Rectangle { x: 17; y: 44; width: parent.width - 34; height: 1; color: "#6877818a" }
                    Text { x: 17; y: 59; text: "Output"; color: muted; font.pixelSize: 12 }
                    Text { x: 110; y: 59; text: launcher.setupFacts.resolution; color: cream; font.pixelSize: 12 }
                    Text { x: 17; y: 85; text: "Frame rate"; color: muted; font.pixelSize: 12 }
                    Text { x: 110; y: 85; text: launcher.setupFacts.frameRate; color: cream; font.pixelSize: 12 }
                    Text { x: 17; y: 111; text: "Upscaling"; color: muted; font.pixelSize: 12 }
                    Text { x: 110; y: 111; text: launcher.setupFacts.upscaler; color: cream; font.pixelSize: 12 }
                    Text { x: 17; y: 137; text: "Display"; color: muted; font.pixelSize: 12 }
                    Text { x: 110; y: 137; text: launcher.setupFacts.display; color: cream; font.pixelSize: 12 }
                    Text { x: 17; y: 181; text: "Visual settings  ›"; color: "#d9b6a6"; font.pixelSize: 11 }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: home.openSettings("Visuals", "Display & frame rate") }
                }
                Rectangle {
                    width: parent.cardWidth; height: 206; radius: 10
                    color: "#db111a23"; border.color: "#6374828e"
                    Text { x: 17; y: 17; text: "CONTROLLER & CONTENT"; color: cream; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7 }
                    Rectangle { x: 17; y: 44; width: parent.width - 34; height: 1; color: "#6877818a" }
                    Text { x: 17; y: 59; width: parent.width - 34; text: home.controllerDetected ? "Xbox controller connected" : "No Xbox controller detected"; color: home.controllerDetected ? "#b8dfc7" : muted; font.pixelSize: 12; elide: Text.ElideRight }
                    Text { x: 17; y: 86; width: parent.width - 34; text: launcher.promptChoice + " button prompts"; color: cream; font.pixelSize: 12 }
                    Text { x: 17; y: 113; width: parent.width - 34; text: "Insert or L3+R3 opens the in-game guide."; color: muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                    Text { x: 17; y: 161; width: parent.width - 34; text: "Mods & patches  ›"; color: "#d9b6a6"; font.pixelSize: 11 }
                    MouseArea { x: 0; y: 0; width: parent.width; height: 141; cursorShape: Qt.PointingHandCursor; onClicked: home.openSettings("Controls", "Button prompts") }
                    MouseArea { x: 0; y: 145; width: parent.width; height: 61; cursorShape: Qt.PointingHandCursor; onClicked: home.openSettings("Mods & Patches", "Installed mods") }
                }
            }

            Rectangle {
                width: parent.width; height: 45; radius: 7
                color: "#b90c131a"; border.color: "#53677682"
                Text { x: 18; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 36
                    text: launcher.notice; color: muted; font.pixelSize: 12; elide: Text.ElideRight }
            }
        }
    }
}
