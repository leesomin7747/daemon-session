import QtQuick
import QtQuick.Controls

FocusScope {
    id: menu
    required property var peer
    property bool localPeer: false
    property bool reducedMotion: false
    property bool hasClient: false
    signal actionChosen(string action,bool busy)
    signal motionToggled()
    signal closed()
    signal quitRequested()
    signal panelRequested(string kind)
    signal connectionRequested()
    Accessible.role: Accessible.Pane
    Accessible.name: localPeer?"내 데몬 메뉴":"친구 데몬 메뉴"
    Keys.onEscapePressed: closed()
    function focusFirst() { closeButton.forceActiveFocus() }
    function reveal(control) {
        var top=control.mapToItem(content,0,0).y
        var bottom=top+control.height
        var offset=viewport.contentY
        if(top<offset) offset=top
        else if(bottom>offset+viewport.height) offset=bottom-viewport.height
        viewport.contentY=Math.max(0,Math.min(offset,Math.max(0,viewport.contentHeight-viewport.height)))
    }
    Rectangle { anchors.fill: parent; color: "#303d33"; radius: 9; border.color: "#617368" }
    Flickable {
        id: viewport
        anchors.fill: parent; anchors.margins: 12; clip: true
        contentWidth: width; contentHeight: content.height
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }
        Column {
            id: content; width: parent.width; spacing: 8
            Item {
                width: parent.width; height: 30
                Text { text: menu.localPeer?"나":"친구"; color: "#eeeae1"; font.pixelSize: 13; font.weight: Font.DemiBold }
                Button {
                    id: closeButton; objectName: "menuClose"; x: parent.width-34; width: 34; height: 30
                    text: "닫기"; font.pixelSize: 11; onClicked: menu.closed()
                    onActiveFocusChanged: if(activeFocus) menu.reveal(this)
                    background: Rectangle { radius: 4; color: closeButton.hovered?"#455a49":"transparent"; border.color: closeButton.activeFocus?"#bad8ba":"transparent" }
                    contentItem: Text { text: closeButton.text; color: "#eeeae1"; font: closeButton.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }
            Text {
                width: parent.width; text: "WIP · "+(menu.peer.wip||"null"); color: "#d5dfd0"
                font.pixelSize: 12; textFormat: Text.PlainText; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
            }
            Text { visible: !menu.localPeer && menu.peer.busy; width: parent.width; text: "busy · 장난은 쉬고, 선물은 옆에 두어요."; color: "#eee2b4"; font.pixelSize: 11; wrapMode: Text.Wrap }
            Flow {
                width: parent.width; spacing: 6
                Repeater {
                    model: menu.localPeer?[{action:"setBusy",label:menu.peer.busy?"busy 풀기":"busy"}]:[
                        {action:"ping",label:"ping 보내기"},{action:"rope",label:"밧줄 끌기"},
                        {action:"lure",label:"간식으로 꼬시기"},{action:"blanket",label:"담요 덮기"},
                        {action:"plane",label:"종이비행기"},{action:"coffee",label:"커피 건네기"},
                        {action:"energy",label:"에너지음료"}]
                    delegate: Button {
                        id: actionButton; required property var modelData
                        objectName: "action_"+modelData.action
                        width: (parent.width-6)/2; height: 34
                        enabled: menu.peer.reachable && (menu.localPeer || !menu.peer.busy || ["coffee","energy"].indexOf(modelData.action)>=0)
                        text: modelData.label; font.pixelSize: 11
                        Accessible.name: text
                        onActiveFocusChanged: if(activeFocus) menu.reveal(this)
                        onClicked: menu.actionChosen(modelData.action,menu.localPeer?!menu.peer.busy:false)
                        background: Rectangle { radius: 5; color: actionButton.hovered?"#455a49":"#26352b"; border.color: actionButton.activeFocus?"#bad8ba":"#526657"; opacity: actionButton.enabled?1:0.4 }
                        contentItem: Row {
                            spacing: 5; opacity: actionButton.enabled?1:0.4
                            Prop { visible: !menu.localPeer; kind: actionButton.modelData.action; width: 20; height: 24 }
                            Text { text: actionButton.text; color: "#eeeae1"; font: actionButton.font; height: parent.height; verticalAlignment: Text.AlignVCenter }
                        }
                    }
                }
            }
            CheckBox {
                onActiveFocusChanged: if(activeFocus) menu.reveal(this)
                text: "움직임 줄이기"; checked: menu.reducedMotion; onClicked: menu.motionToggled()
                palette.windowText: "#eeeae1"; palette.text: "#eeeae1"; palette.highlight: "#bad8ba"
                font.pixelSize: 11
            }
            Row {
                width: parent.width; spacing: 6
                PanelButton { width: (parent.width-6)/2; text: "WIP"; onActiveFocusChanged: if(activeFocus) menu.reveal(this); onClicked: menu.panelRequested("wip") }
                PanelButton { objectName: "composeNote"; width: (parent.width-6)/2; text: menu.localPeer?"받은 것":"쪽지 보내기"; onActiveFocusChanged: if(activeFocus) menu.reveal(this); onClicked: menu.panelRequested(menu.localPeer?"inbox":"note") }
            }
            Row {
                spacing: 12
                Text { text: menu.hasClient?"같은 세션":"로컬 미리보기"; color: "#c0d0c1"; font.pixelSize: 10 }
                Button { visible: menu.localPeer; text: "종료"; flat: true; width: 48; height: 22; font.pixelSize: 10
                    onActiveFocusChanged: if(activeFocus) menu.reveal(this)
                    palette.buttonText: "#eeeae1"; onClicked: menu.quitRequested() }
            }
            PanelButton { visible: menu.localPeer && menu.hasClient; width: parent.width; text: "세션 연결"; onActiveFocusChanged: if(activeFocus) menu.reveal(this); onClicked: menu.connectionRequested() }
        }
    }
}
