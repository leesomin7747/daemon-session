import QtQuick
import QtQuick.Controls

Item {
    id: scene
    property real position: 0.25
    property int petClicks: 0
    property int pings: 0
    property bool automatic: false
    property bool editorOpen: false
    property string savedText: ""

    function syncRegions() {
        if (!pet || !friend || !editor) return
        let regions = [Qt.rect(pet.x, pet.y, pet.width, pet.height),
                       Qt.rect(friend.x, friend.y, friend.width, friend.height)]
        if (editorOpen) regions.push(Qt.rect(editor.x, editor.y, editor.width, editor.height))
        overlayWindow.setInteractiveRegions(regions)
    }
    function movePet() { position = position < 0.5 ? 0.5 : 0.25 }
    function ping() {
        pings++
        pulse.restart()
        console.info("PING", pings)
    }
    function edit() {
        editorOpen = true
        syncRegions()
        overlayWindow.setEditorActive(true)
        input.forceActiveFocus()
    }
    function closeEditor() {
        savedText = input.text
        editorOpen = false
        overlayWindow.setEditorActive(false)
        syncRegions()
        console.info("EDITOR CLOSED", savedText)
    }
    onWidthChanged: syncRegions()
    onHeightChanged: syncRegions()
    onEditorOpenChanged: syncRegions()
    Component.onCompleted: syncRegions()
    Timer { interval: 3000; repeat: true; running: scene.automatic; onTriggered: scene.ping() }
    Rectangle {
        id: pet
        Accessible.role: Accessible.Button
        Accessible.name: "내 데몬 · 클릭 검증"
        Accessible.onPressAction: { scene.petClicks++ }
        x: { scene.width; return overlayWindow.petX(scene.position, width, 12) }
        y: scene.height - height - 18
        width: 72; height: 72; radius: 18; color: "#bcb5ff"
        onXChanged: scene.syncRegions()
        Text { anchors.centerIn: parent; text: "•ᴗ•"; font.pixelSize: 26; color: "#26223b" }
        MouseArea {
            anchors.fill: parent
            onClicked: { scene.petClicks++; console.info("PET CLICK", scene.petClicks) }
            onDoubleClicked: scene.edit()
        }
    }
    Rectangle {
        id: friend
        Accessible.role: Accessible.Button
        Accessible.name: "친구 데몬 · ping"
        Accessible.onPressAction: scene.ping()
        x: { scene.width; return overlayWindow.petX(0.72, width, 12) }
        y: scene.height - height - 18
        width: 72; height: 72; radius: 18; color: "#a9d4bf"
        onXChanged: scene.syncRegions()
        Text { anchors.centerIn: parent; text: "˘ᴗ˘"; font.pixelSize: 26; color: "#17372c" }
        MouseArea { anchors.fill: parent; onClicked: scene.ping() }
        SequentialAnimation {
            id: pulse
            ColorAnimation { target: friend; property: "color"; to: "#f2cc75"; duration: 180 }
            ColorAnimation { target: friend; property: "color"; to: "#a9d4bf"; duration: 500 }
        }
    }
    Rectangle {
        id: editor
        x: Math.max(0, Math.min(pet.x, scene.width - width)); y: 0
        width: Math.min(340, scene.width); height: 84
        visible: scene.editorOpen; radius: 8; color: "#eeedf5"
        TextField {
            id: input; x: 8; y: 8; width: parent.width - 88; height: 40
            placeholderText: "WIP · 한글 / emoji"
            Accessible.name: "WIP 검증 입력"
            onAccepted: scene.closeEditor()
            Keys.onEscapePressed: scene.closeEditor()
        }
        Button { x: parent.width - 72; y: 8; width: 64; text: "닫기"; onClicked: scene.closeEditor() }
        Text { x: 12; y: 56; text: "Enter 또는 Esc로 닫기"; color: "#494454" }
    }
}
