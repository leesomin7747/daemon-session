import QtQuick
import QtQuick.Controls
ScrollView {
    id: field
    property alias editor: input
    property alias text: input.text
    property alias readOnly: input.readOnly
    readonly property bool inputMethodComposing: input.inputMethodComposing
    readonly property string draftText: input.text.slice(0,input.cursorPosition)+input.preeditText+input.text.slice(input.cursorPosition)
    property bool commitRequested: false
    signal commitReady()
    function requestCommit() {
        commitRequested=true
        Qt.inputMethod.commit()
        Qt.callLater(finishCommit)
    }
    function finishCommit() {
        if(!commitRequested || input.inputMethodComposing) return
        commitRequested=false; commitReady()
    }
    property string editorName: ""
    property string label: ""
    property string placeholderText: ""
    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    background: Rectangle { radius: 5; color: "#202c23"; border.color: input.activeFocus?"#bad8ba":"#637768"; border.width: input.activeFocus?2:1 }
    MemoEditor {
        id: input; objectName: field.editorName
        width: field.availableWidth; height: Math.max(implicitHeight,field.availableHeight)
        Accessible.name: field.label; placeholderText: field.placeholderText
        background: null
        onInputMethodComposingChanged: Qt.callLater(field.finishCommit)
    }
}
