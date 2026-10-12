import QtQuick
import QtQuick.Controls

Item {
    id: workspace
    objectName: "settingsWorkspace"
    signal backToLauncher()
    signal openSetup(int step)
    property string category: ""
    property string section: ""
    property string targetKey: ""
    property color cream: "#f1ece3"
    property color quiet: "#a8b2bd"
    property color accent: "#d8a993"
    property var categories: [
        {name:"Visuals", icon:"visuals", warm:true, detail:"Graphics, display & frame rate"},
        {name:"Controls", icon:"controls", warm:false, detail:"Controller & mouse"},
        {name:"Game & Saves", icon:"game-and-saves", warm:false, detail:"Files, language & save data"},
        {name:"Online", icon:"online", warm:true, detail:"Account & connection"},
        {name:"Mods & Patches", icon:"mods-and-patches", warm:true, detail:"Load order & gameplay tweaks"},
        {name:"System", icon:"system", warm:false, detail:"Startup, updates & diagnostics"}
    ]
    function openAt(nextCategory, nextSection, nextKey) {
        category = nextCategory || ""
        section = nextSection || (category ? launcher.categorySections(category)[0] : "")
        targetKey = nextKey || ""
        hub.clearSearch()
        detailScroll.contentY = 0
        if (targetKey !== "") Qt.callLater(focusTarget)
    }
    function focusTarget() {
        for (var i = 0; i < fieldRepeater.count; i++) {
            var row = fieldRepeater.itemAt(i)
            if (row && row.settingKey === targetKey) {
                detailScroll.contentY = Math.max(0, row.y - 54)
                break
            }
        }
    }
    function selectSection(nextSection) { section = nextSection; detailScroll.contentY = 0 }

    Rectangle { anchors.fill: parent; color: "#0a1017" }
    Image {
        visible: category !== ""
        x: 0; y: 0; width: parent.width; height: 232
        source: Qt.resolvedUrl("../art/hero.png")
        fillMode: Image.PreserveAspectCrop; verticalAlignment: Image.AlignTop
        horizontalAlignment: Image.AlignRight
        smooth: true
    }
    Rectangle {
        visible: category !== ""
        x: 0; y: 0; width: parent.width; height: 232
        gradient: Gradient { orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#ce080d13" }
            GradientStop { position: 0.34; color: "#8c080d13" }
            GradientStop { position: 0.75; color: "#32080d13" }
            GradientStop { position: 1; color: "#68080d13" }
        }
    }
    Rectangle {
        visible: category !== ""
        x: 0; y: 0; width: parent.width; height: 248
        gradient: Gradient {
            GradientStop { position: 0; color: "#000a1017" }
            GradientStop { position: 0.62; color: "#390a1017" }
            GradientStop { position: 1; color: "#0a1017" }
        }
    }
    Row {
        visible: category !== ""
        x: 30; y: 22; spacing: 18
        GlassButton { text: "←  Back to launcher"; width: 174; height: 36; onClicked: workspace.backToLauncher() }
        GlassButton { visible: category !== ""; text: "All settings"; width: 112; height: 36; onClicked: workspace.openAt("", "") }
    }
    Text { visible: category !== ""; x: 34; y: 76; text: "SETTINGS  /  " + category.toUpperCase(); color: "#e2b7a3"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2.4 }
    Text { visible: category !== ""; x: 32; y: 91; text: category; color: cream; font.family: "Georgia"; font.pixelSize: 40 }
    Text { visible: category !== ""; x: 34; y: 145; text: "Choose a section. Changes save as you make them."; color: "#e1e3e3"; font.pixelSize: 14 }

    SettingsHub {
        id: hub
        anchors.fill: parent
        visible: category === ""
        categories: workspace.categories
        onBackToLauncher: workspace.backToLauncher()
        onDestinationSelected: (nextCategory, nextSection, nextKey) =>
            workspace.openAt(nextCategory, nextSection, nextKey)
    }

    Item {
        x: 30; y: 188; width: parent.width - 60; height: parent.height - 208
        visible: category !== ""
        Rectangle {
            id: sectionNav
            x: 0; y: 0; width: 211; height: parent.height
            radius: 9; color: "#bd121b25"; border.color: "#54717d85"
            Column {
                x: 10; y: 12; width: parent.width - 20; spacing: 5
                Text { text: "SECTIONS"; color: accent; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2; leftPadding: 9; bottomPadding: 7 }
                Repeater {
                    model: launcher.categorySections(workspace.category)
                    delegate: GlassButton {
                        text: modelData; width: sectionNav.width - 20; height: 38
                        highlighted: workspace.section === modelData
                        onClicked: workspace.selectSection(modelData)
                    }
                }
            }
        }
        Rectangle {
            x: 225; y: 0; width: parent.width - 225; height: parent.height
            radius: 9; color: "#d40e151d"; border.color: "#54717d85"
            Flickable {
                id: detailScroll
                x: 19; y: 17; width: parent.width - 38; height: parent.height - 34
                clip: true; boundsBehavior: Flickable.StopAtBounds
                contentHeight: detailColumn.implicitHeight + 10
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Column {
                    id: detailColumn
                    width: detailScroll.width - 13; spacing: 10
                    Text { text: workspace.section; color: cream; font.family: "Georgia"; font.pixelSize: 28 }
                    Text {
                        visible: workspace.category === "Mods & Patches" && workspace.section === "Gameplay patches"
                        width: parent.width; wrapMode: Text.WordWrap
                        text: "Optional gameplay changes. All are off by default and apply on the next launch."
                        color: quiet; font.pixelSize: 12
                    }
                    Repeater {
                        id: fieldRepeater
                        model: launcher.sectionFields(workspace.category, workspace.section)
                        delegate: FieldRow { width: detailColumn.width; field: modelData }
                    }

                    // Extra actions live beside the task that calls for them.
                    Row { visible: workspace.category === "Controls" && workspace.section === "Button prompts"; spacing: 9
                        GlassButton { text: "Xbox"; width: 116; height: 37; highlighted: launcher.promptChoice === "Xbox"; onClicked: launcher.setPrompt("Xbox") }
                        GlassButton { text: "PlayStation"; width: 130; height: 37; highlighted: launcher.promptChoice === "PlayStation"; onClicked: launcher.setPrompt("PlayStation") }
                        Text { text: "Applies next launch"; color: quiet; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                    }
                    Row { visible: workspace.category === "Visuals" && workspace.section === "Upscaling"; spacing: 10
                        Text { text: launcher.fsrStatus; color: quiet; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        GlassButton { text: "Download FSR 4 assets"; width: 174; height: 35; onClicked: launcher.downloadFsr4() }
                    }
                    Row { visible: workspace.category === "Game & Saves" && workspace.section === "Game files"; spacing: 9
                        GlassButton { text: launcher.setupRunning ? "Installing…" : "Install from ZIP / PKG"; width: 185; height: 36; enabled: !launcher.setupRunning && !launcher.running; onClicked: launcher.installFromArchive() }
                        GlassButton { text: "Check again"; width: 104; height: 36; onClicked: launcher.refreshReadiness() }
                        GlassButton { text: "Re-run setup"; width: 116; height: 36; onClicked: workspace.openSetup(0) }
                    }
                    Repeater {
                        model: workspace.category === "Game & Saves" && workspace.section === "Game files" ? launcher.readiness.slice(0, 1) :
                               workspace.category === "Game & Saves" && workspace.section === "Your data" ? launcher.readiness.slice(1) : []
                        delegate: Text { width: detailColumn.width; text: modelData.label + ": " + modelData.detail; color: modelData.level === "error" ? "#edb090" : "#bbd9c3"; font.pixelSize: 12; wrapMode: Text.WordWrap }
                    }
                    Row { visible: workspace.category === "Game & Saves" && workspace.section === "Your data"; spacing: 9
                        GlassButton { text: "Import trophy keys"; width: 168; height: 36; onClicked: launcher.importTrophyKeys() }
                        GlassButton { text: "Open saves folder"; width: 157; height: 36; onClicked: launcher.action("openSaves") }
                        GlassButton { text: "Check again"; width: 104; height: 36; onClicked: launcher.refreshReadiness() }
                    }
                    Row { visible: workspace.category === "Online" && workspace.section === "Play online"; spacing: 9
                        GlassButton { text: "Create shadNet account"; width: 189; height: 36; onClicked: launcher.openShadNetRegistration() }
                        GlassButton { text: "Test connection"; width: 147; height: 36; onClicked: launcher.testConnection() }
                    }
                    Text { visible: workspace.category === "Online" && workspace.section === "Play online"; width: parent.width; text: "Enter an account ID, not an email address. Test connection checks server reachability only."; color: quiet; font.pixelSize: 12; wrapMode: Text.WordWrap }
                    Repeater {
                        model: workspace.category === "Mods & Patches" && workspace.section === "Installed mods" ? launcher.contentMods : []
                        delegate: Rectangle { width: detailColumn.width; height: 43; radius: 6; color: "#4b293440"
                            GlassSwitch { x: 9; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; checked: modelData.enabled; onToggled: launcher.setMod(modelData.name, checked) }
                            Row { anchors.right: parent.right; anchors.rightMargin: 7; anchors.verticalCenter: parent.verticalCenter; spacing: 4
                                GlassButton { text: "↑"; width: 34; height: 29; onClicked: launcher.moveMod(modelData.name, -1) }
                                GlassButton { text: "↓"; width: 34; height: 29; onClicked: launcher.moveMod(modelData.name, 1) }
                            }
                        }
                    }
                    Row { visible: workspace.category === "Mods & Patches" && workspace.section === "Installed mods"; spacing: 9
                        GlassButton { text: "Refresh mods"; width: 130; height: 35; onClicked: launcher.refresh_addons() }
                        GlassButton { text: "Open mods folder"; width: 151; height: 35; onClicked: launcher.action("openMods") }
                    }
                    Repeater { model: workspace.category === "Mods & Patches" && workspace.section === "External patches" ? launcher.patches : []
                        delegate: GlassSwitch { text: modelData.name; checked: modelData.enabled; onToggled: launcher.setPatch(modelData.name, checked) }
                    }
                    GlassButton { visible: workspace.category === "Mods & Patches" && workspace.section === "External patches"; text: "Bloodborne Enhanced ↗"; width: 192; height: 35; onClicked: launcher.action("mod19") }
                    Column { visible: workspace.category === "System" && workspace.section === "Launch & integration"; spacing: 8
                        GlassSwitch { text: "Launch game directly on startup"; checked: launcher.launchDirectly; onToggled: launcher.setValue("launchDirectly", checked) }
                        GlassSwitch { text: "Close launcher after Play"; checked: launcher.closeOnPlay; onToggled: launcher.setValue("close_on_play", checked) }
                        Row { spacing: 9
                            GlassButton { text: "Desktop shortcut"; width: 145; height: 36; onClicked: launcher.createShortcut() }
                            GlassButton { text: "Add to Steam"; width: 123; height: 36; onClicked: launcher.addToSteam() }
                        }
                        Text { text: "Hold Shift or Ctrl while starting to open the launcher."; color: quiet; font.pixelSize: 12 }
                    }
                    Row { visible: workspace.category === "System" && workspace.section === "Updates"; spacing: 9
                        GlassSwitch { text: "Check for updates automatically"; checked: launcher.checked("check_updates"); onToggled: launcher.setValue("check_updates", checked) }
                        GlassButton { text: "Check now"; width: 100; height: 35; onClicked: launcher.checkForUpdates(true) }
                    }
                    Row { visible: workspace.category === "System" && workspace.section === "Updates"; spacing: 9
                        GlassButton { visible: launcher.hasUpdate; text: "Install update"; width: 127; height: 35; onClicked: launcher.installUpdate() }
                        GlassButton { text: "Releases ↗"; width: 112; height: 35; onClicked: launcher.action("releases") }
                    }
                    Row { visible: workspace.category === "System" && workspace.section === "Maintenance"; spacing: 9
                        GlassButton { text: "Open port folder"; width: 143; height: 35; onClicked: launcher.action("openPort") }
                        GlassButton { text: "Open bbport.ini"; width: 129; height: 35; onClicked: launcher.action("openConfig") }
                        GlassButton { text: "Clear shader cache"; width: 154; height: 35; onClicked: launcher.action("clearCache") }
                    }
                    GlassButton { visible: workspace.category === "System" && workspace.section === "Maintenance"; text: "Re-run setup"; width: 129; height: 35; onClicked: workspace.openSetup(0) }
                    Row { visible: workspace.category === "System" && workspace.section === "Activity & logs"; spacing: 9
                        GlassButton { text: "Copy log"; width: 104; height: 35; onClicked: launcher.action("copyLog") }
                        GlassButton { text: "Clear log"; width: 104; height: 35; onClicked: launcher.action("clearLog") }
                    }
                    TextArea {
                        visible: workspace.category === "System" && workspace.section === "Activity & logs"
                        width: detailColumn.width; height: visible ? 350 : 0; readOnly: true
                        wrapMode: TextEdit.NoWrap; text: launcher.log; color: cream; selectByMouse: true
                        background: Rectangle { color: "#a0090e14"; radius: 7; border.color: "#54717d85" }
                    }
                }
            }
        }
    }
}
