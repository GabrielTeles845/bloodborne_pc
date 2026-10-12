import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window
    visible: true
    width: 1290; height: 830
    minimumWidth: 1050; minimumHeight: 650
    title: "Bloodborne — Windows"
    color: "#090d12"
    property string page: "Home"
    property int wizardStep: 0
    property color cream: "#f1ece3"
    property color quiet: "#a8b2bd"
    property var checks: launcher.readiness
    property bool gameBlocked: checks.length > 0 && checks[0].level === "error"
    property bool savesBlocked: (checks.length > 2 && checks[2].level === "error") || !launcher.savesWritable
    property bool onlineBlocked: launcher.onlineIdInvalid
    property bool blocked: gameBlocked || savesBlocked || onlineBlocked || launcher.setupRunning
    property string statusTitle: launcher.running ? "The hunt is underway" : launcher.setupRunning ? "Preparing game files" : gameBlocked ? "Game files need attention" : savesBlocked ? "Saves need attention" : onlineBlocked ? "Online account needs attention" : "Ready to play"
    property string statusDetail: launcher.running ? "Bloodborne is running." : launcher.setupRunning ? "Installation is in progress." : gameBlocked ? checks[0].detail : savesBlocked ? "Choose a writable saves folder." : onlineBlocked ? "Enter an account ID rather than an email address." : "Everything required is ready."

    function openWizard(step) { wizardStep = step; page = "Setup" }
    function openSettings(category, section) { page = "Settings"; settings.openAt(category || "", section || "") }
    function primaryAction() {
        if (gameBlocked) openWizard(1)
        else if (savesBlocked) openWizard(2)
        else if (onlineBlocked) openSettings("Online", "Play online")
        else if (!launcher.running && !launcher.setupRunning) launcher.play()
    }
    Component.onCompleted: {
        wizardStep = launcher.wizardStartStep
        if (launcher.startPage === "Setup") page = "Setup"
        else if (launcher.startPage === "Log") openSettings("System", "Activity & logs")
        else if (launcher.startPage !== "Home" && launcher.startPage !== "Game") openSettings()
    }

    Item {
        anchors.fill: parent; visible: page === "Home"; clip: true
        Image {
            x: 0; y: 0; width: parent.width * 0.51; height: parent.height
            source: Qt.resolvedUrl("../assets/ui/launcher-bridge.png")
            fillMode: Image.PreserveAspectCrop; horizontalAlignment: Image.AlignLeft; smooth: true
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient { orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#12090d12" }
                GradientStop { position: 0.42; color: "#97090d12" }
                GradientStop { position: 0.55; color: "#ec090d12" }
                GradientStop { position: 1; color: "#fa090d12" }
            }
        }
        Column {
            x: 34; y: 30; spacing: 5
            Text { text: "BLOODBORNE"; color: cream; font.family: "Georgia"; font.pixelSize: 23; font.bold: true; font.letterSpacing: 1 }
            Text { text: "WINDOWS  /  LAUNCHER " + launcher.version; color: "#d4d8dc"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2 }
        }
        Column {
            x: 36; y: parent.height - 116; spacing: 5
            Text { text: "ENTER YHARNAM"; color: "#d8a993"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 2.8 }
            Text { text: "The night awaits."; color: cream; font.family: "Georgia"; font.pixelSize: 29 }
        }
        Item {
            id: homeContent
            x: parent.width * 0.51 + 28; y: 28
            width: parent.width - x - 32; height: parent.height - 56
            Row {
                height: 32; spacing: 9
                Rectangle { width: 8; height: 8; radius: 4; color: blocked ? "#e4a270" : "#8fcaab"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: launcher.running ? "RUNNING" : blocked ? "ACTION NEEDED" : "READY"; color: blocked ? "#e9b28e" : "#abd9bc"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 2; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "  /  QUICK LAUNCHER"; color: quiet; font.pixelSize: 10; font.letterSpacing: 1.6; anchors.verticalCenter: parent.verticalCenter }
            }
            Text { y: 53; width: parent.width; text: statusTitle; color: cream; font.family: "Georgia"; font.pixelSize: 37; elide: Text.ElideRight }
            Text { y: 104; width: parent.width; height: 46; text: statusDetail; color: "#c1cad1"; font.pixelSize: 14; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
            GlassButton {
                y: 162; width: parent.width; height: 62; highlighted: true
                text: launcher.running ? "GAME RUNNING" : launcher.setupRunning ? "INSTALLING GAME FILES…" : gameBlocked ? "FIX GAME FILES" : savesBlocked ? "FIX SAVES" : onlineBlocked ? "FIX ONLINE ACCOUNT" : "▶   PLAY BLOODBORNE"
                enabled: !launcher.running && !launcher.setupRunning
                onClicked: primaryAction()
            }
            Row {
                y: 237; spacing: 10
                GlassButton { text: "Settings"; width: (homeContent.width - 10) / 2; height: 42; onClicked: openSettings() }
                GlassButton { text: "Activity"; width: (homeContent.width - 10) / 2; height: 42; onClicked: openSettings("System", "Activity & logs") }
            }
            GlassButton { visible: launcher.running; y: 286; width: parent.width; height: 35; text: "Stop game"; onClicked: launcher.stop() }
            Rectangle { y: 333; width: parent.width; height: 1; color: "#4b71808a" }
            Text { y: 351; text: "YOUR SETUP"; color: "#d8a993"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2 }
            Text { y: 376; width: parent.width; text: launcher.setupSummary + "  ·  " + launcher.promptChoice + " prompts"; color: cream; font.pixelSize: 13; elide: Text.ElideRight }
            MouseArea { y: 369; width: parent.width; height: 31; cursorShape: Qt.PointingHandCursor; onClicked: openSettings("Visuals", "Display & frame rate") }
            Text { y: 409; text: "GAME FILES  /  SAVES"; color: "#d8a993"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2 }
            Text { y: 434; width: parent.width; text: (gameBlocked ? "Game files missing" : launcher.gameStatus) + "  ·  " + (savesBlocked ? "Saves need attention" : "Saves writable"); color: blocked ? "#edb090" : "#bbd9c3"; font.pixelSize: 13; elide: Text.ElideRight }
            Text {
                visible: launcher.updateVersion !== ""; y: Math.min(parent.height - 83, 490); width: parent.width
                text: "Launcher update " + launcher.updateVersion + " available  ↗"; color: "#bfe0ca"; font.pixelSize: 13
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: launcher.viewRelease() }
            }
            Text { y: parent.height - 35; width: parent.width; height: 30; text: launcher.notice; color: quiet; font.pixelSize: 11; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        }
    }

    Item {
        anchors.fill: parent; visible: page === "Setup"; clip: true
        Rectangle { anchors.fill: parent; color: "#0a0f15" }
        Image {
            x: parent.width * 0.59; width: parent.width * 0.41; height: parent.height
            source: Qt.resolvedUrl("../assets/ui/setup-workshop.png")
            fillMode: Image.PreserveAspectCrop; horizontalAlignment: Image.AlignRight; smooth: true
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient { orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#080d13" }
                GradientStop { position: 0.57; color: "#100d13" }
                GradientStop { position: 0.73; color: "#610b1016" }
                GradientStop { position: 1; color: "#200b1016" }
            }
        }
        SetupWizard {
            x: 28; y: 27; width: parent.width * 0.61 - 40; height: parent.height - 54
            startStep: wizardStep
            onExitRequested: (target) => { page = "Home" }
        }
    }

    SettingsWorkspace {
        id: settings; anchors.fill: parent; visible: page === "Settings"
        onBackToLauncher: page = "Home"
        onOpenSetup: (step) => openWizard(step)
    }
    palette.windowText: cream
    palette.buttonText: cream
    palette.text: cream
    palette.highlightedText: cream
}
