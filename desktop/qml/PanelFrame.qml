import QtQuick
import QtQuick.Window
import QtQuick.Controls
FocusScope {
    id: panel
    default property alias contentData: content.data
    property string title: ""
    property string statusText: ""
    property bool pending: false
    property Item initialFocusItem: closeButton
    readonly property real bodyWidth: content.width
    readonly property real viewportHeight: viewport.height
    signal closed()
    Accessible.role: Accessible.Pane
    Accessible.name: title
    function focusFirst() { if(initialFocusItem) initialFocusItem.forceActiveFocus(Qt.OtherFocusReason) }
    function fail(code) {
        pending=false
        statusText=code==="offline"?"연결을 확인한 뒤 다시 시도해주세요.":code==="busy"?"지금은 busy예요. 선물이나 쪽지를 건네보세요.":"처리하지 못했어요. 내용을 확인하고 다시 시도해주세요."
    }
    function reveal(item) {
        if(!item) return
        var ancestor=item
        while(ancestor && ancestor!==content) ancestor=ancestor.parent
        if(!ancestor) return
        var top=item.mapToItem(content,0,0).y
        var bottom=top+Math.min(item.height,viewport.height)
        var offset=viewport.contentY
        if(top<offset) offset=top
        else if(bottom>offset+viewport.height) offset=bottom-viewport.height
        viewport.contentY=Math.max(0,Math.min(offset,Math.max(0,viewport.contentHeight-viewport.height)))
    }
    Keys.onEscapePressed: function(event) {
        var item=panel.Window.window?panel.Window.window.activeFocusItem:null
        if(item && item.inputMethodComposing) { event.accepted=false; return }
        closed()
    }
    Keys.onPressed: function(event) {
        if(event.key!==Qt.Key_PageDown && event.key!==Qt.Key_PageUp) return
        var step=viewport.height*(event.key===Qt.Key_PageDown?0.85:-0.85)
        viewport.contentY=Math.max(0,Math.min(viewport.contentY+step,Math.max(0,viewport.contentHeight-viewport.height)))
        event.accepted=true
    }
    data: [Connections {
        target: panel.Window.window
        function onActiveFocusItemChanged() { panel.reveal(panel.Window.window.activeFocusItem) }
    },
    Rectangle { anchors.fill: parent; color: "#303d33"; radius: 9; border.color: "#617368" },
    Text { x: 14; y: 13; text: panel.title; textFormat: Text.PlainText; color: "#eeeae1"; font.pixelSize: 13; font.weight: Font.DemiBold },
    PanelButton { id: closeButton; objectName: "panelClose"; x: parent.width-width-12; y: 8; width: 42; height: 28; text: "닫기"; onClicked: panel.closed() },
    Flickable {
        id: viewport
        x: 14; y: 44; width: parent.width-28; height: Math.max(1,parent.height-y-12)
        clip: true; contentWidth: width; contentHeight: content.height
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { focusPolicy: Qt.NoFocus }
        Column { id: content; width: viewport.width; spacing: 8 }
        onContentHeightChanged: Qt.callLater(function(){if(panel.Window.window) panel.reveal(panel.Window.window.activeFocusItem)})
    }]
}
