import QtQuick
import QtQuick.Controls
PanelFrame {
    id: panel
    required property var peer
    required property var model
    property bool localPeer: false
    property bool localOnline: false
    property string submittedText: ""
    property string pendingAction: ""
    property bool submittedBusy: false
    signal actionChosen(string action,string text,bool busy,string requestId)
    signal navigate(string kind)
    title: "WIP · "+(localPeer?"나":"친구")
    initialFocusItem: editorLoader.item?editorLoader.item.editor:friendActions
    function acceptEvent(e) {
        if(!pending || e.requestId!==pendingRequestId || e.action!==pendingAction || e.actorId!==peer.id || e.targetId!==peer.id) return false
        if((pendingAction==="setWip" && e.text!==submittedText) || (pendingAction==="setBusy" && e.busy!==submittedBusy)) return false
        pending=false; statusText=e.caption
        if(pendingAction==="setWip" && editorLoader.item) editorLoader.item.text=e.text
        return true
    }
    function submitWip() {
        var field=editorLoader.item
        if(!field || field.inputMethodComposing || pending || !localOnline || !model.textValid("setWip",field.text)) return
        submittedText=model.normalizedText(field.text); pendingAction="setWip"; pendingRequestId=model.newRequestId(); pending=true; statusText="저장 중…"
        actionChosen("setWip",field.text,false,pendingRequestId)
    }
    function submitBusy() {
        if(pending || !localOnline || !model) return
        submittedBusy=!peer.busy; pendingAction="setBusy"; pendingRequestId=model.newRequestId(); pending=true; statusText="busy 변경 중…"
        actionChosen("setBusy","",submittedBusy,pendingRequestId)
    }
    Loader {
        id: editorLoader; width: panel.bodyWidth; active: panel.localPeer
        sourceComponent: MemoField {
            editorName: "wipEditor"; width: editorLoader.width
            height: Math.max(36,Math.min(78,panel.viewportHeight-8))
            label: "내 WIP"; placeholderText: "지금 작업 중인 것"
            readOnly: panel.pending
            onCommitReady: panel.submitWip()
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
        text: (panel.model?panel.model.textLength(editorLoader.item?editorLoader.item.draftText:""):0)+" / 80"
        color: "#c6d2c5"; font.pixelSize: 11; horizontalAlignment: Text.AlignRight
    }
    PanelButton {
        objectName: "saveWip"; visible: panel.localPeer; width: panel.bodyWidth; primary: true
        text: panel.pending && panel.pendingAction==="setWip"?"저장 중…":"WIP 저장"
        enabled: panel.model && panel.localOnline && !panel.pending && editorLoader.item && !editorLoader.item.commitRequested && panel.model.textValid("setWip",editorLoader.item.draftText)
        onClicked: {
            if(!enabled) return
            editorLoader.item.requestCommit()
        }
    }
    Row {
        width: panel.bodyWidth; spacing: 8
        PanelButton {
            id: friendActions; width: (parent.width-8)/2
            text: panel.localPeer?(panel.peer.busy?"busy 풀기":"busy"):"장난·선물"
            enabled: !panel.pending && (!panel.localPeer || panel.localOnline)
            onClicked: panel.localPeer?panel.submitBusy():panel.navigate("menu")
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
