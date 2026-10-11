import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 1290
    height: 830
    minimumWidth: 1050
    minimumHeight: 650
    title: "Bloodborne — Windows"
    color: "#080d12"
    property string page: "Home"
    property color cream: "#f3f1ee"
    property color quiet: "#aeb5bd"
    property color crimson: "#bf475b"
    property color mint: "#95dca8"
    property var nav: [
        {name:"Home", icon:"home"}, {name:"Graphics", icon:"graphics"},
        {name:"Display", icon:"display"}, {name:"Game", icon:"home"},
        {name:"Controller", icon:"controller"}, {name:"Cheats", icon:"cheats"},
        {name:"Online", icon:"online"}, {name:"Mods", icon:"mods"},
        {name:"Advanced", icon:"advanced"}, {name:"Log", icon:"log"}
    ]

    // Step the wizard opens on: 0 for a fresh or re-run setup, the failing step for "Fix".
    property int wizardStep: 0

    function go(next) {
        page = next
        settingsView.contentY = 0
    }
    function openWizard(step) {
        wizardStep = step
        go("Setup")
    }
    Component.onCompleted: {
        wizardStep = launcher.wizardStartStep
        page = launcher.startPage
    }

    // Full-window artwork remains behind every surface, including the sidebar and cards.
    Image {
        id: wallpaper
        anchors.fill: parent
        source: Qt.resolvedUrl("../art/hero.png")
        fillMode: Image.PreserveAspectCrop
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        smooth: true
        cache: true
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "#40070b10" }
            GradientStop { position: 0.48; color: "#80080b11" }
            GradientStop { position: 1; color: "#c2080b10" }
        }
    }
    Rectangle {
        x: 0; y: 0; width: 16; height: parent.height
        gradient: Gradient { orientation: Gradient.Horizontal
            GradientStop {position:0; color:"#8e06080a"}
            GradientStop {position:1; color:"transparent"} }
    }

    Row {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: sidebar
            width: 248
            height: parent.height
            color: "#c20b1017"
            border.color: "#4bffffff"
            border.width: 1
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: "#33ffffff" }
            Column {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.topMargin: 27
                spacing: 5
                Row {
                    height: 67
                    width: parent.width
                    spacing: 13
                    Image {
                        width: 46; height: 46
                        source: Qt.resolvedUrl("../assets/hunter-icon.png")
                        fillMode: Image.PreserveAspectCrop
                        smooth: true; mipmap: true
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text { text: "BLOODBORNE"; color: cream; font.family: "Georgia"; font.pointSize: 14; font.bold: true }
                        Text { text: "WINDOWS  ·  " + launcher.version; color: quiet; font.pixelSize: 10; font.letterSpacing: 1.3 }
                    }
                }
                Rectangle { width: parent.width; height: 1; color: "#38ffffff" }
                Item { width: 1; height: 10 }
                // The wizard is a single path: no page navigation until it is finished or skipped.
                Text {
                    visible: page === "Setup"
                    width: parent.width; wrapMode: Text.WordWrap; topPadding: 6
                    text: "Finish or skip setup to open the launcher pages."
                    color: "#a9afb6"; font.pixelSize: 12
                }
                Repeater {
                    model: page === "Setup" ? [] : nav
                    delegate: Rectangle {
                        width: sidebar.width - 36
                        height: 47
                        radius: 8
                        property bool active: page === modelData.name
                        color: active ? "#91412a34" : mouse.containsMouse ? "#4a363b43" : "transparent"
                        border.color: active ? "#a4cf6677" : "transparent"
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: 130 } }
                        Rectangle { visible: parent.active; width: 3; height: 23; radius: 2; color: crimson; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter }
                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            spacing: 17
                            NavIcon { kind: modelData.icon; strokeColor: parent.parent.active ? "#ffe4e9" : "#bec5ca" }
                            Text { text: modelData.name; color: parent.parent.active ? cream : "#d1d5d8"; font.pixelSize: 15; anchors.verticalCenter: parent.verticalCenter }
                        }
                        MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: go(modelData.name) }
                    }
                }
                Item { width: 1; height: 14 }
                Rectangle { width: parent.width; height: 1; color: "#38ffffff" }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "In game: Insert or L3 + R3 opens the guide."; color: "#a9afb6"; font.pixelSize: 11; topPadding: 14 }
            }
        }

        Item {
            id: main
            width: parent.width - sidebar.width
            height: parent.height

            Item {
                id: topBar
                x: 31; y: 21; width: parent.width - 62; height: 28
                Text { text: page === "Home" ? "THE HUNT BEGINS" : page.toUpperCase(); color: "#c5cbd1"; font.pixelSize: 11; font.letterSpacing: 2.6; font.bold: true }
                Text { anchors.right: parent.right; text: "CUSA03173  ·  LOCAL PC BUILD"; color: "#b4bdc6"; font.pixelSize: 10; font.letterSpacing: 1.6 }
            }

            Item {
                id: homeView
                visible: page === "Home"
                x: 31; y: 58; width: parent.width - 62; height: parent.height - 131
                // Short windows: a tighter hero leaves room for the launch options and cards.
                property bool compact: height < 600
                Text {
                    x: 4; y: homeView.compact ? 4 : 22
                    text: "Bloodborne"
                    color: "#efe5cd"
                    font.family: "Georgia"
                    font.pixelSize: homeView.compact ? 64 : 77
                    font.letterSpacing: -2
                    style: Text.Raised
                    styleColor: "#a7000000"
                }
                Text {
                    x: 7; y: homeView.compact ? 88 : 122; width: Math.min(560, parent.width * 0.62)
                    text: "A journey to Yharnam. Uncover its secrets, face its nightmares, and survive the night."
                    color: "#dae0e4"; wrapMode: Text.WordWrap; font.pixelSize: 16; lineHeight: 1.4
                }
                // Shown only when an update exists; it never blocks Play.
                Rectangle {
                    visible: launcher.updateVersion !== ""
                    anchors.right: parent.right; anchors.rightMargin: 2; y: 30
                    width: updateRow.implicitWidth + 24; height: 44; radius: 10
                    color: "#cb0b1119"; border.color: "#8c9cdda9"
                    Row {
                        id: updateRow
                        anchors.centerIn: parent; spacing: 12
                        Text { text: "Update available: " + launcher.updateVersion; color: "#b9ebc5"; font.pixelSize: 13; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                        GlassButton { text: "View release"; width: 112; height: 30; onClicked: launcher.viewRelease() }
                    }
                }
                Row {
                    id: playRow
                    x: 7; y: homeView.compact ? 158 : 206; spacing: 12
                    GlassButton { id: playHero; text: launcher.running ? "GAME RUNNING" : "▶   PLAY"; enabled: !launcher.running; onClicked: launcher.play(); width: 207; height: 51; highlighted: true }
                    GlassButton { text: "⚙   GAME OPTIONS"; onClicked: go("Graphics"); width: 184; height: 51 }
                }

                Rectangle {
                    id: launchOptions
                    x: 2; y: playRow.y + playRow.height + 14
                    width: parent.width - 4; height: launchFlow.implicitHeight + 16
                    radius: 10; color: "#a90d141c"; border.color: "#50e0e8ee"
                    Flow {
                        id: launchFlow
                        x: 14; y: 8; width: parent.width - 28
                        spacing: 10
                        Text {
                            height: 34; verticalAlignment: Text.AlignVCenter; rightPadding: 4
                            text: "LAUNCH OPTIONS"; color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.6
                        }
                        GlassSwitch {
                            height: 34; text: "Launch game directly on startup"
                            checked: launcher.launchDirectly
                            onToggled: launcher.setValue("launchDirectly", checked)
                        }
                        GlassSwitch {
                            height: 34; text: "Close launcher after Play"
                            checked: launcher.closeOnPlay
                            onToggled: launcher.setValue("close_on_play", checked)
                        }
                        GlassButton { text: "Desktop shortcut"; width: 140; height: 34; onClicked: launcher.createShortcut() }
                        GlassButton { text: "Add to Steam"; width: 118; height: 34; onClicked: launcher.addToSteam() }
                        Text {
                            visible: launcher.launchDirectly
                            height: 34; verticalAlignment: Text.AlignVCenter
                            text: "Hold Shift while starting to open this launcher."
                            color: quiet; font.pixelSize: 12
                        }
                    }
                }

                Row {
                    id: cards
                    x: 2; y: launchOptions.y + launchOptions.height + 14; width: parent.width - 4
                    height: Math.max(110, Math.min(236, parent.height - y - 90))
                    spacing: 12
                    GlassCard {
                        width: (cards.width - 24) / 3; height: cards.height
                        heading: "GAME STATUS"; icon: "home"
                        content: launcher.gameStatus + "\n\n" + launcher.saveStatus + "\n\nUse your own game files."
                    }
                    GlassCard {
                        width: (cards.width - 24) / 3; height: cards.height
                        heading: "CURRENT SETUP"; icon: "graphics"
                        content: launcher.setupSummary + "\n\n" + launcher.gpuStatus + "\n" + launcher.fsrStatus
                    }
                    GlassCard {
                        width: (cards.width - 24) / 3; height: cards.height
                        heading: "CONTROLLER & CONTENT"; icon: "controller"
                        content: launcher.promptChoice + " button prompts\n\nMods and patches are optional.\n\nLocal achievements in the in-game guide."
                    }
                }
                Rectangle {
                    visible: height >= 60
                    x: 2; y: cards.y + cards.height + 15
                    width: parent.width - 4; height: parent.height - (cards.y + cards.height + 15)
                    radius: 12; color: "#9c0c1118"; border.color: "#47d8e2e8"
                    Image { anchors.fill: parent; anchors.margins: 1; source: Qt.resolvedUrl("../art/strip.png"); fillMode: Image.PreserveAspectCrop; opacity: 0.48; clip: true }
                    Rectangle { anchors.fill: parent; radius: 12; gradient: Gradient { orientation: Gradient.Horizontal
                        GradientStop {position:0; color:"#cf080b10"} GradientStop {position:0.7; color:"#44080b10"} GradientStop {position:1; color:"#77080b10"} } }
                    Column { x: 24; anchors.verticalCenter: parent.verticalCenter; spacing: 3
                        Text { text: "Return to Yharnam"; color: cream; font.family: "Georgia"; font.pixelSize: 22 }
                        Text { text: "Face your nightmares once more."; color: "#cdd4d6"; font.pixelSize: 12 }
                    }
                }
            }

            Rectangle {
                id: settingsPanel
                visible: page !== "Home" && page !== "Setup"
                x: 31; y: 58; width: parent.width - 62; height: parent.height - 132
                radius: 14; color: "#cb0b1119"; border.color: "#52dce2e8"
                Rectangle { x: 1; y: 1; width: parent.width - 2; height: 57; radius: 13; color: "#2bffffff" }
                Text { x: 27; y: 17; text: page === "Display" ? "Display & FPS" : page; color: cream; font.family: "Georgia"; font.pixelSize: 27 }
                Text { x: 28; y: 58; text: page === "Mods" ? "Choose content to apply when the game starts." : "Settings save automatically as you change them."; color: quiet; font.pixelSize: 12 }
                Rectangle { x: 27; y: 87; width: parent.width - 54; height: 1; color: "#43ffffff" }
                Flickable {
                    id: settingsView
                    x: 27; y: 102; width: parent.width - 54; height: parent.height - 123
                    clip: true
                    contentHeight: settingsColumn.implicitHeight + 12
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    Column {
                        id: settingsColumn
                        width: settingsView.width - 14
                        spacing: 9
                        // Start-up validation failed; hides itself once fixed or dismissed.
                        Rectangle {
                            visible: page === "Game" && launcher.startupProblem !== ""
                            width: parent.width; height: visible ? 52 : 0; radius: 8
                            color: "#8a3a1820"; border.color: "#b4df6672"
                            Rectangle { x: 14; anchors.verticalCenter: parent.verticalCenter; width: 10; height: 10; radius: 5; color: "#df6672" }
                            Text {
                                x: 34; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 220; elide: Text.ElideRight
                                text: launcher.startupProblem; color: cream; font.pixelSize: 14; font.bold: true
                            }
                            Row {
                                anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; spacing: 8
                                GlassButton { text: "Fix"; width: 92; height: 32; highlighted: true; onClicked: openWizard(launcher.fixStep) }
                                GlassButton { text: "✕"; width: 34; height: 32; onClicked: launcher.dismissProblem() }
                            }
                        }
                        Rectangle {
                            visible: page === "Game"
                            width: parent.width; height: visible ? 82 : 0; radius: 8
                            color: "#86151d27"; border.color: "#6a9dc6b2"
                            GlassButton {
                                x: 12; y: 12; width: 232; height: 36
                                text: launcher.setupRunning ? "Installing game files…" : "Install from ZIP / PKG"
                                enabled: !launcher.setupRunning && !launcher.running
                                onClicked: launcher.installFromArchive()
                            }
                            GlassButton {
                                x: 256; y: 12; width: 135; height: 36
                                text: "Setup wizard"
                                onClicked: openWizard(0)
                            }
                            Text {
                                x: 12; y: 54; width: parent.width - 24
                                text: "Choose your cusa-03173.zip or base and 1.09 PKGs. Your current game folder is never replaced."
                                color: quiet; font.pixelSize: 12; elide: Text.ElideRight
                            }
                        }
                        Rectangle {
                            visible: page === "Game"
                            width: parent.width; height: visible ? 202 : 0; radius: 10
                            color: "#94111922"; border.color: "#65c4d4d2"
                            Text {
                                x: 15; y: 12; text: "SETUP CHECKS"
                                color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.6
                            }
                            GlassButton {
                                anchors.right: parent.right; anchors.rightMargin: 12; y: 7
                                text: "Check again"; width: 108; height: 29
                                onClicked: launcher.refreshReadiness()
                            }
                            Column {
                                x: 14; y: 47; width: parent.width - 28; spacing: 6
                                Repeater {
                                    model: page === "Game" ? launcher.readiness : []
                                    delegate: Rectangle {
                                        width: parent.width; height: 43; radius: 6
                                        color: "#40ffffff"; border.color: "#2cffffff"
                                        Rectangle {
                                            x: 9; y: 16; width: 10; height: 10; radius: 5
                                            color: modelData.level === "ready" ? "#72d69a" :
                                                   modelData.level === "error" ? "#df6672" : "#e9ba69"
                                        }
                                        Text {
                                            x: 27; y: 5; text: modelData.label
                                            color: cream; font.pixelSize: 13; font.bold: true
                                        }
                                        Text {
                                            x: 27; y: 23; width: parent.width - 37
                                            text: modelData.detail; color: quiet; font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                        // Controller: Xbox textures, the game's PlayStation glyphs, or native key names.
                        Text { visible: page === "Controller"; text: "BUTTON PROMPTS"; color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7 }
                        Row { visible: page === "Controller"; spacing: 8
                            GlassButton { text: "Xbox"; width: 112; height: 35; highlighted: launcher.promptChoice === "Xbox"; onClicked: launcher.setPrompt("Xbox") }
                            GlassButton { text: "PlayStation"; width: 112; height: 35; highlighted: launcher.promptChoice === "PlayStation"; onClicked: launcher.setPrompt("PlayStation") }
                            GlassButton { text: "Keyboard"; width: 112; height: 35; highlighted: launcher.promptChoice === "Keyboard"; onClicked: launcher.setPrompt("Keyboard") }
                            Text { text: "Applies after restart"; color: quiet; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Text { visible: page === "Controller"; text: "DEBUG"; color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7; topPadding: 12 }
                        // Advanced: automatic update check with "Check now" beside it.
                        Rectangle {
                            visible: page === "Advanced"
                            width: parent.width; height: visible ? 46 : 0; radius: 7
                            color: "#5a202a34"; border.color: "#36ffffff"
                            Text { x: 15; anchors.verticalCenter: parent.verticalCenter; text: "Check for updates"; color: "#e7edf1"; font.pixelSize: 13 }
                            Row {
                                anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter; spacing: 12
                                GlassButton { text: "Install update"; visible: launcher.hasUpdate; width: 118; height: 30; onClicked: launcher.installUpdate() }
                                GlassButton { text: "Check now"; width: 100; height: 30; onClicked: launcher.checkForUpdates(true) }
                                GlassSwitch {
                                    anchors.verticalCenter: parent.verticalCenter
                                    checked: launcher.checked("check_updates")
                                    onToggled: launcher.setValue("check_updates", checked)
                                }
                            }
                        }
                        Repeater {
                            model: page === "Home" || page === "Log" ? [] : launcher.fields(page)
                            delegate: FieldRow { width: settingsColumn.width; field: modelData }
                        }
                        Text { visible: page === "Advanced"; text: "DIAGNOSTICS"; color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7; topPadding: 12 }
                        Repeater {
                            model: page === "Advanced" ? launcher.fields("Diagnostics") : []
                            delegate: FieldRow { width: settingsColumn.width; field: modelData }
                        }
                        Row {
                            visible: page === "Game"
                            spacing: 12

                            GlassButton {
                                text: "Import Trophy Keys"
                                width: 190
                                height: 36
                                onClicked: launcher.importTrophyKeys()
                            }

                            Text {
                                text: "Optional - select your own keys.json"
                                color: quiet
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Row { visible: page === "Graphics"; spacing: 9
                            Text { text: launcher.fsrStatus; color: "#bad5c4"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            GlassButton { text: "Download FSR 4 assets"; width: 182; height: 35; onClicked: launcher.downloadFsr4() }
                        }
                        Row { visible: page === "Online"; spacing: 9
                            GlassButton { text: "Create shadNet account"; width: 195; height: 36; onClicked: launcher.openShadNetRegistration() }
                            GlassButton { text: "Test connection"; width: 165; height: 36; onClicked: launcher.testConnection() }
                        }
                        Text { visible: page === "Online"; text: "No Discord needed. Register on shadPS4, then enter that account name and password above. The Hunter's Dream website account is separate and optional."; color: quiet; font.pixelSize: 12; wrapMode: Text.WordWrap; width: settingsColumn.width }
                        Text { visible: page === "Online"; text: "Test connection checks server reachability, not your account sign-in."; color: quiet; font.pixelSize: 12; wrapMode: Text.WordWrap; width: settingsColumn.width }
                        Text { visible: page === "Mods"; text: "PC GAMEPLAY ENHANCEMENTS"; color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7 }
                        Repeater {
                            model: page === "Mods" ? launcher.fields("Enhancements") : []
                            delegate: FieldRow { width: settingsColumn.width; field: modelData }
                        }
                        Text { visible: page === "Mods"; text: "Made from your own game files at launch; restart to apply. These do not require Load mods."; color: quiet; font.pixelSize: 12; wrapMode: Text.WordWrap; width: settingsColumn.width }
                        Text { visible: page === "Mods"; text: "INSTALLED MODS"; color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7 }
                        Text { visible: page === "Mods" && launcher.contentMods.length === 0; text: "No other mods installed. Xbox button prompts are on the Controller page."; color: quiet; font.pixelSize: 12 }
                        Repeater {
                            model: page === "Mods" ? launcher.contentMods : []
                            delegate: Rectangle {
                                width: settingsColumn.width; height: 42; radius: 6; color: "#3bffffff"; border.color: "#32ffffff"
                                GlassSwitch { anchors.left: parent.left; anchors.leftMargin: 9; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; checked: modelData.enabled; onToggled: launcher.setMod(modelData.name, checked) }
                                Row { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; spacing: 4
                                    GlassButton { text: "↑"; width: 35; height: 30; onClicked: launcher.moveMod(modelData.name, -1) }
                                    GlassButton { text: "↓"; width: 35; height: 30; onClicked: launcher.moveMod(modelData.name, 1) }
                                }
                            }
                        }
                        Text { visible: page === "Mods"; text: "EXTERNAL PATCHES"; color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7; topPadding: 15 }
                        Repeater {
                            model: page === "Mods" ? launcher.patches : []
                            delegate: GlassSwitch { text: modelData.name; checked: modelData.enabled; onToggled: launcher.setPatch(modelData.name, checked) }
                        }
                        Row { visible: page === "Mods"; spacing: 8
                            GlassButton { text: "Refresh mods"; onClicked: launcher.refresh_addons() }
                            GlassButton { text: "Open mods folder"; onClicked: launcher.action("openMods") }
                            GlassButton { text: "Optional: Bloodborne Enhanced ↗"; onClicked: launcher.action("mod19") }
                        }
                        Text { visible: page === "Mods"; text: "Bloodborne Enhanced is a separate optional download. Install it yourself and enable it above."; color: quiet; font.pixelSize: 12; wrapMode: Text.WordWrap; width: parent.width }
                        Item { visible: page === "Advanced"; width: 1; height: visible ? 6 : 0 }
                        Row { visible: page === "Advanced"; spacing: 8
                            GlassButton { text: "Open port folder"; onClicked: launcher.action("openPort") }
                            GlassButton { text: "Open bbport.ini"; onClicked: launcher.action("openConfig") }
                            GlassButton { text: "Clear shader cache"; onClicked: launcher.action("clearCache") }
                            GlassButton { text: "Releases ↗"; onClicked: launcher.action("releases") }
                        }
                        Row { visible: page === "Advanced"; spacing: 8
                            GlassButton { text: "Re-run setup"; onClicked: openWizard(0) }
                        }
                        Row { visible: page === "Log"; spacing: 8
                            GlassButton { text: "Copy log"; onClicked: launcher.action("copyLog") }
                            GlassButton { text: "Clear log"; onClicked: launcher.action("clearLog") }
                            GlassButton { text: "Open saves folder"; onClicked: launcher.action("openSaves") }
                        }
                        TextArea { visible: page === "Log"; width: parent.width; height: visible ? 440 : 0; readOnly: true; wrapMode: TextEdit.NoWrap; text: launcher.log; color: cream; selectByMouse: true; background: Rectangle { color: "#9910161c"; radius: 7; border.color: "#4affffff" } }
                    }
                }
            }

            SetupWizard {
                visible: page === "Setup"
                x: 31; y: 58; width: parent.width - 62; height: parent.height - 132
                startStep: wizardStep
                onExitRequested: (target) => go(target)
            }

            Rectangle {
                id: footer
                x: 0; y: parent.height - 69; width: parent.width; height: 69
                color: "#d20a0e14"; border.color: "#34ffffff"
                Text { x: 30; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 320; elide: Text.ElideRight; text: launcher.notice; color: "#cbd2d8"; font.pixelSize: 12 }
                Row { visible: page !== "Setup"; anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter; spacing: 10
                    GlassButton { text: "STOP"; enabled: launcher.running; onClicked: launcher.stop(); width: 106; height: 42 }
                    GlassButton { text: launcher.running ? "RUNNING" : "▶  PLAY"; enabled: !launcher.running && !launcher.setupRunning; highlighted: true; onClicked: launcher.play(); width: 196; height: 42 }
                }
            }
        }
    }

    palette.windowText: cream
    palette.buttonText: cream
    palette.text: cream
    palette.highlight: crimson
    palette.highlightedText: cream
}
