import QtQuick
import QtQuick.Controls
PanelFrame {
    id: panel
    required property var peer
    required property var model
    property bool localOnline: false
    property string submittedText: ""
    signal actionChosen(string action,string text,bool busy)
    signal confirmed()
    title: "친구에게 쪽지"
    initialFocusItem: editor.editor
    function acceptEvent(e) {
        if(!pending || e.action!=="note" || e.actorId!==model.localPeerId || e.targetId!==peer.id || e.text!==submittedText) return false
        pending=false; confirmed(); return true
    }
    function submitNote() {
        if(editor.inputMethodComposing || pending || !localOnline || !peer.reachable || !model.textValid("note",editor.text)) return
        submittedText=model.normalizedText(editor.text); pending=true; statusText="보내는 중…"
        actionChosen("note",editor.text,false)
    }
    Text {
        width: panel.bodyWidth; text: panel.peer.busy?"busy · 쪽지는 옆에 조용히 두어요.":"말을 걸고 싶을 때, 짧게 한 장."
        wrapMode: Text.Wrap; color: "#c6d2c5"; font.pixelSize: 11
    }
    MemoField {
        id: editor; editorName: "noteEditor"; width: panel.bodyWidth
        height: Math.max(36,Math.min(78,panel.viewportHeight-8))
        label: "보낼 쪽지"; placeholderText: "전하고 싶은 말"; readOnly: panel.pending
        onCommitReady: panel.submitNote()
    }
    Text { objectName: "textCount"; width: panel.bodyWidth; text: (panel.model?panel.model.textLength(editor.draftText):0)+" / 120"; color: "#c6d2c5"; font.pixelSize: 11; horizontalAlignment: Text.AlignRight }
    PanelButton {
        objectName: "sendNote"; width: panel.bodyWidth; primary: true; text: panel.pending?"보내는 중…":"쪽지 보내기"
        enabled: panel.model && panel.localOnline && panel.peer.reachable && !panel.pending && !editor.commitRequested && panel.model.textValid("note",editor.draftText)
        onClicked: {
            if(!enabled) return
            editor.requestCommit()
        }
    }
    Text {
        objectName: "panelStatus"; width: panel.bodyWidth
        text: !panel.localOnline || !panel.peer.reachable?"offline · 둘 다 접속 중일 때 보낼 수 있어요.":panel.statusText
        textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#eee2b4"; font.pixelSize: 11
    }
}
