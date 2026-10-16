import QtQuick
import QtQuick.Controls
TextArea {
    id: editor
    textFormat: TextEdit.PlainText
    wrapMode: TextEdit.Wrap
    selectByMouse: true
    font.pixelSize: 13
    color: "#eeeae1"; placeholderTextColor: "#b5c3b8"
    selectionColor: "#bad8ba"; selectedTextColor: "#202c23"
    padding: 8
    background: Rectangle { radius: 5; color: "#202c23"; border.color: editor.activeFocus?"#bad8ba":"#637768"; border.width: editor.activeFocus?2:1 }
    Keys.onTabPressed: function(event) {
        if(inputMethodComposing) { event.accepted=false; return }
        nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason)
    }
    Keys.onBacktabPressed: function(event) {
        if(inputMethodComposing) { event.accepted=false; return }
        nextItemInFocusChain(false).forceActiveFocus(Qt.BacktabFocusReason)
    }
}
