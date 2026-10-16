import QtQuick
import QtQuick.Controls
PanelFrame {
    id: panel
    required property var peer
    required property var model
    property bool localOnline: false
    signal actionChosen(string action,string text,bool busy,string requestId)
    title: "받은 것 · "+peer.inbox.length
    function acceptEvent(e) {
        if(!pending || e.requestId!==pendingRequestId || e.action!=="ackInbox" || e.actorId!==peer.id || e.targetId!==peer.id) return false
        pending=false; statusText=e.caption; return true
    }
    Text { objectName: "inboxEmpty"; visible: panel.peer.inbox.length===0; width: panel.bodyWidth; text: "아직 놓인 게 없어요.\n커피나 쪽지가 오면 여기서 살펴봐요."; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#c6d2c5"; font.pixelSize: 12 }
    Repeater {
        model: panel.peer.inbox
        delegate: Column {
            required property var modelData
            width: panel.bodyWidth; spacing: 4
            Row {
                spacing: 6
                Prop { kind: modelData.kind; width: 20; height: 23 }
                Text { text: modelData.kind==="note"?"쪽지":modelData.kind==="coffee"?"커피":"에너지음료"; color: "#c6d2c5"; font.pixelSize: 11 }
            }
            Text { objectName: "inboxBody_"+modelData.eventId; width: parent.width; text: modelData.text; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#eeeae1"; font.pixelSize: 13 }
            Rectangle { width: parent.width; height: 1; color: "#526655" }
        }
    }
    PanelButton {
        objectName: "ackInbox"; width: panel.bodyWidth; primary: true
        text: panel.pending?"정리 중…":"확인하고 정리"
        enabled: panel.localOnline && !panel.pending && panel.peer.inbox.length>0
        onClicked: { if(!enabled) return; panel.pendingRequestId=panel.model.newRequestId(); panel.pending=true; panel.statusText="정리 중…"; panel.actionChosen("ackInbox","",false,panel.pendingRequestId) }
    }
    Text { objectName: "panelStatus"; width: panel.bodyWidth; text: panel.statusText||(!panel.localOnline?"offline · 읽을 수 있어요. 정리는 다시 연결한 뒤 해주세요.":""); textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#eee2b4"; font.pixelSize: 11 }
}
