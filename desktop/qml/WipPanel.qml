import QtQuick
import QtQuick.Controls
PanelFrame {
    id: panel
    required property var peer
    required property var model
    property bool localPeer: false
    property bool localOnline: false
    property string submittedText: ""
    signal actionChosen(string action,string text,bool busy)
    signal navigate(string kind)
    title: "WIP · "+(localPeer?"나":"친구")
    initialFocusItem: editorLoader.item?editorLoader.item.editor:friendActions
    function acceptEvent(e) {
        if(!pending || e.action!=="setWip" || e.actorId!==peer.id || e.targetId!==peer.id || e.text!==submittedText) return false
        pending=false; statusText=e.caption
        if(editorLoader.item) editorLoader.item.text=e.text
        return true
    }
    Loader {
        id: editorLoader; width: panel.bodyWidth; active: panel.localPeer
        sourceComponent: MemoField {
            editorName: "wipEditor"; width: editorLoader.width
            height: Math.max(36,Math.min(78,panel.viewportHeight-8))
            label: "내 WIP"; placeholderText: "지금 작업 중인 것"
            readOnly: panel.pending
            Component.onCompleted: text=panel.peer.wip
        }
    }
    Text {
        objectName: "friendWip"; visible: !panel.localPeer; width: panel.bodyWidth
        text: panel.peer.wip||"null"; textFormat: Text.PlainText; wrapMode: Text.Wrap
        color: "#eeeae1"; font.pixelSize: 13
    }
    Text {
        objectName: "textCount"; visible: panel.localPeer; width: panel.bodyWidth
        text: (panel.model?panel.model.textLength(editorLoader.item?editorLoader.item.text:""):0)+" / 80"
        color: "#c6d2c5"; font.pixelSize: 11; horizontalAlignment: Text.AlignRight
    }
    PanelButton {
        objectName: "saveWip"; visible: panel.localPeer; width: panel.bodyWidth; primary: true
        text: panel.pending?"저장 중…":"WIP 저장"
        enabled: panel.model && panel.localOnline && !panel.pending && editorLoader.item && !editorLoader.item.inputMethodComposing && panel.model.textValid("setWip",editorLoader.item.text)
        onClicked: {
            if(!enabled) return
            panel.submittedText=panel.model.normalizedText(editorLoader.item.text); panel.pending=true; panel.statusText="저장 중…"
            panel.actionChosen("setWip",editorLoader.item.text,false)
        }
    }
    Row {
        width: panel.bodyWidth; spacing: 8
        PanelButton {
            id: friendActions; width: (parent.width-8)/2
            text: panel.localPeer?(panel.peer.busy?"busy 풀기":"busy"):"장난·선물"
            enabled: panel.localPeer?panel.localOnline:!panel.pending
            onClicked: panel.localPeer?panel.actionChosen("setBusy","",!panel.peer.busy):panel.navigate("menu")
        }
        PanelButton {
            objectName: "wipSecondary"; width: (parent.width-8)/2
            text: panel.localPeer?"받은 것 · "+panel.peer.inbox.length:"쪽지 보내기"
            onClicked: panel.navigate(panel.localPeer?"inbox":"note")
        }
    }
    PanelButton {
        objectName: "openAppMenu"; visible: panel.localPeer; width: panel.bodyWidth
        text: "움직임·앱 메뉴"; onClicked: panel.navigate("menu")
    }
    Text { objectName: "panelStatus"; width: panel.bodyWidth; text: panel.statusText||(!panel.localOnline?"offline · 다시 연결한 뒤 저장할 수 있어요.":""); textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#eee2b4"; font.pixelSize: 11 }
}
