import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window
    visible: true
    width: 1290; height: 830
    minimumWidth: 1050; minimumHeight: 650
    title: "Bloodborne — Windows"
    color: Theme.base
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
        else if (launcher.startPage === "Log") openSettings("System", "Logs")
        else if (launcher.startPage !== "Home" && launcher.startPage !== "Game") openSettings()
    }

    HomePage {
        anchors.fill: parent
        visible: page === "Home"
        gameBlocked: window.gameBlocked
        savesBlocked: window.savesBlocked
        onlineBlocked: window.onlineBlocked
        onPrimaryRequested: window.primaryAction()
        onOpenSettings: (category, section) => window.openSettings(category, section)
    }

    // First-time setup: the wizard on a panel, the workshop art beside it.
    Item {
        anchors.fill: parent; visible: page === "Setup"; clip: true
        Rectangle { anchors.fill: parent; color: Theme.base }
        Image {
            x: Theme.railWidth + parent.width * 0.42; width: parent.width - x; height: parent.height
            source: Qt.resolvedUrl("../assets/ui/setup-workshop.png")
            fillMode: Image.PreserveAspectCrop; horizontalAlignment: Image.AlignRight; smooth: true; asynchronous: true
        }
        Rectangle {
            x: Theme.railWidth; width: parent.width * 0.62; height: parent.height
            gradient: Gradient { orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#f20b0d10" }
                GradientStop { position: 0.78; color: "#f00b0d10" }
                GradientStop { position: 1; color: "#000b0d10" }
            }
        }
        SetupWizard {
            x: Theme.railWidth + 44; y: 34
            width: Math.min(640, parent.width * 0.55 - Theme.railWidth); height: parent.height - 68
            startStep: wizardStep
            onExitRequested: (target) => { page = "Home" }
        }
    }

    SettingsWorkspace {
        id: settings; anchors.fill: parent; visible: page === "Settings"
        onBackToLauncher: page = "Home"
        onOpenSetup: (step) => openWizard(step)
    }

    Rail {
        height: parent.height
        current: page
        locked: page === "Setup"
        onNavigate: (target) => { if (target === "Settings") openSettings("", ""); else page = target }
    }
    font.family: Theme.sans
    palette.windowText: cream
    palette.buttonText: cream
    palette.text: cream
    palette.highlightedText: cream
}
