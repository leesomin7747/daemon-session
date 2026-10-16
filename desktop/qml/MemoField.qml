import QtQuick
import QtQuick.Controls
ScrollView {
    id: field
    property alias editor: input
    property alias text: input.text
    property alias readOnly: input.readOnly
    readonly property bool inputMethodComposing: input.inputMethodComposing
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
    }
}
