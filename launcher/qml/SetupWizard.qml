import QtQuick
import QtQuick.Controls

Rectangle {
    id: wizard
    objectName: "setupWizard"
    property int step: 0
    property int startStep: 0
    property var checks: launcher.readiness
    // A game folder with an older version is usable (amber); only a failed check blocks.
    property bool gameValid: checks[0].level !== "error"
    signal exitRequested(string page)
    onVisibleChanged: if (visible) step = startStep
    radius: 14
    color: "#d00b111a"
    border.color: "#73cad5d9"

    function levelColor(level) {
        return level === "ready" ? "#72d69a" : level === "error" ? "#df6672" : "#e9ba69"
    }
    // Your data: red if a check failed, green only when every item is verified, amber otherwise.
    function dataLevel() {
        var levels = checks.slice(1).map(function(c) { return c.level })
        return levels.indexOf("error") >= 0 ? "error" : levels.every(function(l) { return l === "ready" }) ? "ready" : "warning"
    }
    function stepValid(index) { return index !== 1 || gameValid }
    function canReach(index) {
        for (var i = 0; i < index; i++)
            if (!stepValid(i)) return false
        return true
    }
    function skip() {
        if (!gameValid && !launcher.confirm("Skip setup?",
                "The game won't launch without game files. You can add them later on the Game page.\n\nSkip setup anyway?"))
            return
        launcher.skipSetup()
        wizard.exitRequested("Game")
    }
    function finish(play) {
        launcher.completeSetup()
        wizard.exitRequested("Home")
        if (play) launcher.play()
    }

    Text {
        x: 30; y: 25; text: "FIRST-TIME SETUP"
        color: "#91d8a7"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 2
    }
    Text {
        x: 30; y: 54; width: parent.width - 60
        text: ["Welcome to Yharnam", "Add your game files", "Trophies and saves", "Ready to play"][wizard.step]
        color: "#f3f1ee"; font.family: "Georgia"; font.pixelSize: 30
    }
    Row {
        x: 30; y: 109; width: parent.width - 60; spacing: 7
        Repeater {
            model: ["Welcome", "Game", "Your data", "Finish"]
            delegate: Rectangle {
                id: tab
                property bool current: index === wizard.step
                property bool reachable: index < wizard.step || wizard.canReach(index)
                // Only steps with real checks get a status dot.
                property string level: index === 1 ? checks[0].level : index === 2 ? wizard.dataLevel() : ""
                width: (wizard.width - 81) / 4; height: 34; radius: 6
                color: current ? "#98552b3c" : tabMouse.containsMouse && reachable ? "#4a363b43" : "#342b343b"
                border.color: current ? "#d4906571" : "#35ffffff"
                opacity: reachable || current ? 1 : 0.5
                Row {
                    anchors.centerIn: parent; spacing: 8
                    Rectangle {
                        visible: tab.level !== ""
                        width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter
                        color: wizard.levelColor(tab.level)
                    }
                    Text {
                        text: (index + 1) + "  " + modelData
                        color: tab.current ? "#fff8f4" : "#bec5c9"
                        font.pixelSize: 12; font.bold: tab.current
                    }
                }
                MouseArea {
                    id: tabMouse
                    anchors.fill: parent; hoverEnabled: true
                    enabled: tab.reachable && !tab.current
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: wizard.step = index
                }
            }
        }
    }
    Rectangle { x: 30; y: 158; width: parent.width - 60; height: 1; color: "#49ffffff" }

    Column {
        x: 30; y: 180; width: parent.width - 60; spacing: 16
        visible: wizard.step === 0
        Rectangle {
            width: recommended.implicitWidth + 22; height: 26; radius: 13
            color: "#3a427554"; border.color: "#8c9cdda9"
            Text {
                id: recommended
                anchors.centerIn: parent
                text: "Recommended, takes about 2 minutes"
                color: "#b9ebc5"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 0.4
            }
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: "This guide helps you choose your own game files, check the trophy pack, and keep your saves in the right place."
            color: "#f0f0ef"; font.pixelSize: 18; lineHeight: 1.35
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: "You can skip setup and use the normal launcher instead. The game files and trophy key are not included in this download."
            color: "#b8c2c8"; font.pixelSize: 14; lineHeight: 1.4
        }
        Rectangle {
            width: parent.width; height: 91; radius: 8
            color: "#5628323d"; border.color: "#5a9daab1"
            Text {
                anchors.fill: parent; anchors.margins: 17; wrapMode: Text.WordWrap
                text: "Have cusa-03173.zip? The next step can unpack it and apply update 1.09. Already have a CUSA03173 folder? Point the launcher to the folder containing eboot.bin."
                color: "#d9e2e4"; font.pixelSize: 14; verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Column {
        x: 30; y: 185; width: parent.width - 60; spacing: 14
        visible: wizard.step === 1
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: "Choose how to add Bloodborne 1.09. The installer creates a new game folder and never replaces an existing one."
            color: "#e5e8e8"; font.pixelSize: 16
        }
        Row {
            spacing: 12
            GlassButton {
                text: launcher.setupRunning ? "Installing…" : "Install from ZIP / PKG"
                width: 230; height: 43; highlighted: true
                enabled: !launcher.setupRunning && !launcher.running
                onClicked: launcher.installFromArchive()
            }
            GlassButton {
                text: "Choose existing game folder"
                width: 230; height: 43
                enabled: !launcher.setupRunning
                onClicked: launcher.browse("game_dir")
            }
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: "For cusa-03173.zip, choose the ZIP and then a destination such as C:\\Games\\Bloodborne. For an extracted game, select the folder with eboot.bin directly inside it."
            color: "#aebac1"; font.pixelSize: 13
        }
        Rectangle {
            width: parent.width; height: 87; radius: 8
            color: "#60323c45"; border.color: "#58b8c6c8"
            Rectangle {
                x: 14; y: 19; width: 11; height: 11; radius: 6
                color: wizard.levelColor(checks[0].level)
            }
            Text { x: 35; y: 13; text: "Game files"; color: "#f5f1ef"; font.bold: true; font.pixelSize: 15 }
            Text {
                x: 35; y: 38; width: parent.width - 48; wrapMode: Text.WordWrap
                text: checks[0].detail; color: "#ccd4d6"; font.pixelSize: 13
            }
        }
        GlassButton { text: "Check again"; width: 128; height: 35; onClicked: launcher.refreshReadiness() }
    }

    Column {
        x: 30; y: 185; width: parent.width - 60; spacing: 12
        visible: wizard.step === 2
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: "Both items are optional. Trophy names need your own keys.json. Your saves stay in this launcher's user folder unless you choose an existing save folder."
            color: "#e5e8e8"; font.pixelSize: 15
        }
        Row {
            spacing: 12
            GlassButton { text: "Import trophy keys"; width: 198; height: 39; onClicked: launcher.importTrophyKeys() }
            GlassButton { text: "Choose existing saves folder"; width: 225; height: 39; onClicked: launcher.browse("user_dir") }
        }
        Repeater {
            model: checks.slice(1)
            delegate: Rectangle {
                width: wizard.width - 60; height: 75; radius: 8
                color: "#60323c45"; border.color: "#58b8c6c8"
                Rectangle {
                    x: 14; y: 17; width: 11; height: 11; radius: 6
                    color: wizard.levelColor(modelData.level)
                }
                Text { id: itemLabel; x: 35; y: 11; text: modelData.label; color: "#f5f1ef"; font.bold: true; font.pixelSize: 15 }
                Rectangle {
                    x: itemLabel.x + itemLabel.implicitWidth + 10; y: 11
                    width: optionalText.implicitWidth + 14; height: 19; radius: 9
                    color: "#3328323d"; border.color: "#5a9daab1"
                    Text {
                        id: optionalText
                        anchors.centerIn: parent; text: "Optional"
                        color: "#c3ccd1"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.6
                    }
                }
                Text {
                    x: 35; y: 36; width: parent.width - 48; text: modelData.detail
                    color: "#ccd4d6"; font.pixelSize: 13; elide: Text.ElideRight
                }
            }
        }
        GlassButton { text: "Check again"; width: 128; height: 35; onClicked: launcher.refreshReadiness() }
    }

    Column {
        x: 30; y: 185; width: parent.width - 60; spacing: 15
        visible: wizard.step === 3
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: gameValid ? "Your game files are ready. You can start playing or explore the launcher settings." :
                   "The game folder still needs attention. Go back to Game files, or open the launcher and fix it on the Game page."
            color: "#eef0ed"; font.pixelSize: 17
        }
        Repeater {
            model: checks
            delegate: Row {
                spacing: 10
                Rectangle {
                    width: 10; height: 10; radius: 5; anchors.verticalCenter: parent.verticalCenter
                    color: wizard.levelColor(modelData.level)
                }
                Text {
                    text: modelData.label + ":  " + modelData.detail
                    width: wizard.width - 105; wrapMode: Text.WordWrap
                    color: "#dce2e3"; font.pixelSize: 13
                }
            }
        }
        Rectangle {
            width: parent.width; height: 58; radius: 8
            color: "#5628323d"; border.color: "#5a9daab1"
            GlassSwitch {
                x: 12; anchors.verticalCenter: parent.verticalCenter
                text: "Launch the game directly next time"
                checked: launcher.launchDirectly
                onToggled: launcher.setValue("launchDirectly", checked)
            }
            Text {
                anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter
                text: "Hold Shift while starting to open the launcher."
                color: "#aebac1"; font.pixelSize: 12
            }
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: "In the game, press Insert or L3+R3 to open the guide. Setup can be run again from the Game or Advanced page."
            color: "#b7c1c5"; font.pixelSize: 13
        }
    }

    Rectangle { x: 30; y: parent.height - 93; width: parent.width - 60; height: 1; color: "#4affffff" }
    Row {
        x: 30; y: parent.height - 76; spacing: 10
        // Welcome
        GlassButton {
            visible: wizard.step === 0
            text: "Start setup"; width: 156; height: 40; highlighted: true
            onClicked: wizard.step = 1
        }
        GlassButton {
            visible: wizard.step === 0
            text: "Skip for now"; width: 132; height: 40
            onClicked: { launcher.skipSetup(); wizard.exitRequested("Game") }
        }
        // Game and Your data
        GlassButton {
            visible: wizard.step === 1 || wizard.step === 2
            text: "Skip setup"; width: 122; height: 40
            onClicked: wizard.skip()
        }
        GlassButton {
            text: "Back"; width: 94; height: 40; visible: wizard.step > 0
            onClicked: wizard.step--
        }
        GlassButton {
            visible: wizard.step === 1 || wizard.step === 2
            text: "Continue"; width: 156; height: 40; highlighted: true
            enabled: wizard.stepValid(wizard.step)
            onClicked: wizard.step++
        }
        // Finish
        GlassButton {
            visible: wizard.step === 3
            text: "▶  Play now"; width: 156; height: 40; highlighted: true
            enabled: gameValid && !launcher.running && !launcher.setupRunning
            onClicked: wizard.finish(true)
        }
        GlassButton {
            visible: wizard.step === 3
            text: "Open launcher"; width: 140; height: 40
            onClicked: wizard.finish(false)
        }
    }
    Text {
        visible: wizard.step === 1 && !gameValid
        anchors.right: parent.right; anchors.rightMargin: 30; y: parent.height - 64
        text: "Add valid game files to continue."
        color: "#aebac1"; font.pixelSize: 12
    }
}
