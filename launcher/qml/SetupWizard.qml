import QtQuick
import QtQuick.Controls

Rectangle {
    id: wizard
    objectName: "setupWizard"
    property int step: 0
    property var checks: launcher.readiness
    signal exitRequested()
    onVisibleChanged: if (visible) step = 0
    radius: 14
    color: "#d00b111a"
    border.color: "#73cad5d9"

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
                width: (wizard.width - 81) / 4; height: 34; radius: 6
                color: index === wizard.step ? "#98552b3c" : index < wizard.step ? "#65415349" : "#342b343b"
                border.color: index === wizard.step ? "#d4906571" : "#35ffffff"
                Text {
                    anchors.centerIn: parent; text: (index + 1) + "  " + modelData
                    color: index === wizard.step ? "#fff8f4" : "#bec5c9"
                    font.pixelSize: 12; font.bold: index === wizard.step
                }
            }
        }
    }
    Rectangle { x: 30; y: 158; width: parent.width - 60; height: 1; color: "#49ffffff" }

    Column {
        x: 30; y: 186; width: parent.width - 60; spacing: 18
        visible: wizard.step === 0
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: "This guide helps you choose your own game files, check the trophy pack, and keep your saves in the right place. It takes a few minutes."
            color: "#f0f0ef"; font.pixelSize: 18; lineHeight: 1.35
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: "You can skip setup at any time and use the normal launcher. The game files and trophy key are not included in this download."
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
                color: checks[0].level === "ready" ? "#72d69a" : checks[0].level === "error" ? "#df6672" : "#e9ba69"
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
            text: "Trophy names need your own keys.json. Importing it is optional. Your saves stay in this launcher's user folder unless you choose an existing save folder."
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
                    color: modelData.level === "ready" ? "#72d69a" : modelData.level === "error" ? "#df6672" : "#e9ba69"
                }
                Text { x: 35; y: 11; text: modelData.label; color: "#f5f1ef"; font.bold: true; font.pixelSize: 15 }
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
            text: checks[0].level === "ready" ? "Your game files are ready. You can start playing or explore the launcher settings." :
                   "The game folder still needs attention. Go back to Game files, or open the launcher and fix it on the Game page."
            color: "#eef0ed"; font.pixelSize: 17
        }
        Repeater {
            model: checks
            delegate: Row {
                spacing: 10
                Rectangle {
                    width: 10; height: 10; radius: 5; anchors.verticalCenter: parent.verticalCenter
                    color: modelData.level === "ready" ? "#72d69a" : modelData.level === "error" ? "#df6672" : "#e9ba69"
                }
                Text {
                    text: modelData.label + ":  " + modelData.detail
                    width: wizard.width - 105; wrapMode: Text.WordWrap
                    color: "#dce2e3"; font.pixelSize: 13
                }
            }
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: "In the game, press Insert or L3+R3 to open the guide. Setup can be opened again from the Game page."
            color: "#b7c1c5"; font.pixelSize: 13
        }
    }

    Rectangle { x: 30; y: parent.height - 93; width: parent.width - 60; height: 1; color: "#4affffff" }
    Row {
        x: 30; y: parent.height - 76; spacing: 10
        GlassButton {
            text: "Skip setup"; width: 122; height: 40
            onClicked: { launcher.finishSetupWizard(); wizard.exitRequested() }
        }
        GlassButton {
            text: "Back"; width: 94; height: 40; visible: wizard.step > 0
            onClicked: wizard.step--
        }
        GlassButton {
            text: wizard.step === 3 ? "Open launcher" : "Continue"
            width: 156; height: 40; highlighted: true
            onClicked: {
                if (wizard.step < 3) wizard.step++
                else { launcher.finishSetupWizard(); wizard.exitRequested() }
            }
        }
        GlassButton {
            visible: wizard.step === 3 && checks[0].level === "ready"
            text: "Play now"; width: 126; height: 40
            onClicked: { launcher.finishSetupWizard(); wizard.exitRequested(); launcher.play() }
        }
    }
}
