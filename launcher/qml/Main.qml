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

    function go(next) { page = next; settingsView.contentY = 0 }

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
                    Rectangle {
                        width: 46; height: 46; radius: 11; color: "#59323e"
                        border.color: "#77e7a5af"
                        NavIcon { anchors.centerIn: parent; width: 26; height: 26; kind: "achievements"; strokeColor: "#f8e6e8" }
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text { text: "BLOODBORNE"; color: cream; font.family: "Georgia"; font.pointSize: 14; font.bold: true }
                        Text { text: "WINDOWS  ·  " + "1.6.5"; color: quiet; font.pixelSize: 10; font.letterSpacing: 1.3 }
                    }
                }
                Rectangle { width: parent.width; height: 1; color: "#38ffffff" }
                Item { width: 1; height: 10 }
                Repeater {
                    model: nav
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
                Text {
                    x: 4; y: 22
                    text: "Bloodborne"
                    color: "#efe5cd"
                    font.family: "Georgia"
                    font.pixelSize: 77
                    font.letterSpacing: -2
                    style: Text.Raised
                    styleColor: "#a7000000"
                }
                Text {
                    x: 7; y: 122; width: Math.min(560, parent.width * 0.62)
                    text: "A journey to Yharnam. Uncover its secrets, face its nightmares, and survive the night."
                    color: "#dae0e4"; wrapMode: Text.WordWrap; font.pixelSize: 16; lineHeight: 1.4
                }
                Row {
                    x: 7; y: 206; spacing: 12
                    GlassButton { id: playHero; text: launcher.running ? "GAME RUNNING" : "▶   PLAY"; enabled: !launcher.running; onClicked: launcher.play(); width: 207; height: 51; highlighted: true }
                    GlassButton { text: "⚙   GAME OPTIONS"; onClicked: go("Graphics"); width: 184; height: 51 }
                }

                Row {
                    id: cards
                    x: 2; y: 291; width: parent.width - 4; height: Math.min(236, parent.height * 0.37)
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
                    x: 2; y: cards.y + cards.height + 15
                    width: parent.width - 4; height: Math.max(75, parent.height - (cards.y + cards.height + 15))
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
                visible: page !== "Home"
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
                        Repeater {
                            model: page === "Home" || page === "Log" ? [] : launcher.fields(page)
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
                            GlassButton { text: "Test connection"; width: 165; height: 36; onClicked: launcher.testConnection() }
                            Text { text: "Your account ID is the server name, not your email."; color: quiet; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Item { visible: page === "Mods"; width: parent.width; height: visible ? 16 : 0 }
                        Text { visible: page === "Mods"; text: "CONTROLLER BUTTON PROMPTS"; color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7 }
                        Row { visible: page === "Mods"; spacing: 8
                            GlassButton { text: "Xbox"; width: 112; height: 35; highlighted: launcher.promptChoice === "Xbox"; onClicked: launcher.setPrompt("Xbox") }
                            GlassButton { text: "PlayStation"; width: 112; height: 35; highlighted: launcher.promptChoice === "PlayStation"; onClicked: launcher.setPrompt("PlayStation") }
                            Text { text: "Applies after restart"; color: quiet; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Text { visible: page === "Mods"; text: "INSTALLED MODS"; color: mint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.7 }
                        Repeater {
                            model: page === "Mods" ? launcher.mods : []
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
                        Row { visible: page === "Advanced"; spacing: 8
                            GlassButton { text: "Open port folder"; onClicked: launcher.action("openPort") }
                            GlassButton { text: "Open bbport.ini"; onClicked: launcher.action("openConfig") }
                            GlassButton { text: "Clear shader cache"; onClicked: launcher.action("clearCache") }
                            GlassButton { text: "Releases ↗"; onClicked: launcher.action("releases") }
                        }
                        Row { visible: page === "Advanced"; spacing: 8
                            GlassButton { text: "Desktop shortcut"; onClicked: launcher.createShortcut() }
                            GlassButton { text: "Add to Steam"; onClicked: launcher.addToSteam() }
                            GlassButton { text: "Check for updates"; onClicked: launcher.checkForUpdates(true) }
                            GlassButton { text: "Install update"; visible: launcher.hasUpdate; onClicked: launcher.installUpdate() }
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

            Rectangle {
                id: footer
                x: 0; y: parent.height - 69; width: parent.width; height: 69
                color: "#d20a0e14"; border.color: "#34ffffff"
                Text { x: 30; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 320; elide: Text.ElideRight; text: launcher.notice; color: "#cbd2d8"; font.pixelSize: 12 }
                Row { anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter; spacing: 10
                    GlassButton { text: "STOP"; enabled: launcher.running; onClicked: launcher.stop(); width: 106; height: 42 }
                    GlassButton { text: launcher.running ? "RUNNING" : "▶  PLAY"; enabled: !launcher.running; highlighted: true; onClicked: launcher.play(); width: 196; height: 42 }
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
