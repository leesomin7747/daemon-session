import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#22262b"
    property int backgroundClicks: 0
    Column {
        x: 36; y: 32; spacing: 16
        Text { text: "daemon-session / overlay probe"; color: "#eeedf4"; font.pixelSize: 26 }
        Text { text: environment; color: "#acb4c1"; font.pixelSize: 15 }
        Row {
            spacing: 12
            Button { text: "펫 이동"; onClicked: scene.movePet() }
            Button { text: "ping"; onClicked: scene.ping() }
            Button { text: "WIP 입력"; onClicked: scene.edit() }
            CheckBox { text: "3초마다 자동 ping"; onToggled: scene.automatic = checked }
        }
        Text {
            Accessible.role: Accessible.StaticText
            Accessible.name: text
            text: "펫 클릭: " + scene.petClicks + "   |   뒤 창 클릭: " + root.backgroundClicks
                  + "   |   ping: " + scene.pings
            color: "#c9c3ff"; font.pixelSize: 20
        }
        Text { text: "아래 입력창에 타이핑하면서 자동 ping 중 포커스가 유지되는지 확인하세요."; color: "#c4cad2" }
        TextArea {
            id: work
            width: Math.min(800, root.width - 72); height: 160
            placeholderText: "작업창 역할 · 이곳에 계속 입력"
            Accessible.name: "작업창 포커스 검증 입력"
            background: Rectangle { color: "#30363e"; border.color: work.activeFocus ? "#c9c3ff" : "#59606a"; radius: 8 }
            color: "#eeedf4"; font.pixelSize: 20
        }
        Text { text: "저장한 WIP: " + scene.savedText; color: "#c4cad2"; Accessible.role: Accessible.StaticText; Accessible.name: text }
        Button { text: "검증 종료"; onClicked: Qt.quit() }
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 180; color: "#313b43"
        Text {
            anchors.horizontalCenter: parent.horizontalCenter; y: 20
            text: "이 띠의 빈 곳 / 펫이 떠난 자리를 클릭 → 뒤 창 클릭 수 증가"
            color: "#b8c4cd"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: { root.backgroundClicks++; console.info("BACKGROUND CLICK", root.backgroundClicks, mouse.x, mouse.y) }
        }
    }
}
