import QtQuick
import QtQuick.Shapes
import Daemon 1.0
Item {
    id: scene
    required property SceneModel model
    property var nativeOverlay: null
    property var sessionClient: null
    property string serverUrl: "ws://127.0.0.1:8787/"
    property bool reducedMotion: false
    property var interactiveRegions: []
    property string effectKind: ""
    property string displayedBubble: ""
    property real effectSourceX: 0
    property real effectTargetX: 0
    property string effectActor: ""
    property string effectTarget: ""
    property real effectFinalX: 0
    property real flightProgress: 1
    property string caption: ""
    property var previousPositions: ({})
    property string menuPeerId: ""
    property string panelKind: ""
    property string panelPeerId: ""
    property string returnMenuPeerId: ""
    property string lastJoinRequest: ""
    readonly property bool localOnline: model && model.peerStates[model.localPeerId]!==undefined && model.peerStates[model.localPeerId].reachable
    readonly property bool menuOpen: menuPeerId!==""
    function petFor(id) {
        for(var i=0;i<pets.count;i++) { var p=pets.itemAt(i); if(p && p.peer.id===id) return p }
        return null
    }
    function syncRegions() {
        var result=[]
        for(var i=0;i<pets.count;i++) {
            var p=pets.itemAt(i); if(!p) continue
            var rects=p.inputRects(scene)
            for(var j=0;j<rects.length;j++) {
                var r=rects[j], x=Math.max(0,r.x), y=Math.max(0,r.y)
                var right=Math.min(width,r.x+r.width), bottom=Math.min(height,r.y+r.height)
                if(right>x && bottom>y) result.push(Qt.rect(x,y,right-x,bottom-y))
            }
        }
        if(menuLoader.item) result.push(Qt.rect(menuLoader.x,menuLoader.y,menuLoader.width,menuLoader.height))
        if(panelLoader.item) result.push(Qt.rect(panelLoader.x,panelLoader.y,panelLoader.width,panelLoader.height))
        interactiveRegions=result
        if(nativeOverlay) nativeOverlay.setInteractiveRegions(result)
    }
    function stopEffects() {
        effectTimer.stop(); flight.stop()
        for(var i=0;i<pets.count;i++) if(pets.itemAt(i)) pets.itemAt(i).stopAction()
        effectKind=""; displayedBubble=""; caption=""
        Qt.callLater(syncRegions)
    }
    function capturePositions() {
        var positions={}
        for(var i=0;i<pets.count;i++) {var p=pets.itemAt(i); if(p) positions[p.peer.id]=p.x}
        previousPositions=positions; stopEffects()
    }
    function playEvent() {
        var e=model.lastEvent; if(!e.eventId) return
        if(panelLoader.item && typeof panelLoader.item.acceptEvent==="function" && panelLoader.item.acceptEvent(e)) { caption=e.caption; effectTimer.restart() }
        var p=petFor(e.targetId); if(!p) return
        if(e.queued) { Qt.callLater(syncRegions); return }
        if(["setBusy","setWip","ackInbox","note"].indexOf(e.action)>=0) return
        effectActor=e.actorId; effectTarget=e.targetId; effectFinalX=e.finalX
        effectKind=e.action; displayedBubble=e.bubble; caption=e.caption
        p.play(e,previousPositions[e.targetId]===undefined?p.x:previousPositions[e.targetId])
        flightProgress=reducedMotion?1:0
        if(e.action==="plane" && !reducedMotion) flight.start()
        effectTimer.restart(); Qt.callLater(syncRegions)
    }
    function closeMenu() {
        if(!menuOpen) return
        menuPeerId=""; if(nativeOverlay && panelKind==="") nativeOverlay.setEditorActive(false)
        Qt.callLater(syncRegions)
    }
    function openMenu(id) {
        if(!petFor(id)) return
        panelKind=""; panelPeerId=""; returnMenuPeerId=""
        menuPeerId=id
        Qt.callLater(function() { syncRegions(); if(nativeOverlay) nativeOverlay.setEditorActive(true)
            if(menuLoader.item) menuLoader.item.focusFirst() })
    }
    function openPanel(kind,id) {
        if(!model || model.peerStates[id]===undefined || ["wip","note","inbox"].indexOf(kind)<0) return
        if(kind==="inbox" && id!==model.localPeerId) return
        if(kind==="note" && id===model.localPeerId) return
        if(menuOpen) returnMenuPeerId=menuPeerId
        menuPeerId=""; panelPeerId=id; panelKind=kind
        Qt.callLater(function() {
            syncRegions(); if(nativeOverlay) nativeOverlay.setEditorActive(true)
            if(panelLoader.item) panelLoader.item.focusFirst()
        })
    }
    function openConnection() {
        if(menuOpen) returnMenuPeerId=menuPeerId
        menuPeerId=""; panelPeerId=""; panelKind="connect"
        Qt.callLater(function() {syncRegions(); if(nativeOverlay) nativeOverlay.setEditorActive(true); if(panelLoader.item) panelLoader.item.focusFirst()})
    }
    function closePanel(returnToMenu) {
        var previous=returnMenuPeerId
        panelKind=""; panelPeerId=""; returnMenuPeerId=""
        if(returnToMenu && previous!=="" && petFor(previous)) { openMenu(previous); return }
        if(nativeOverlay) nativeOverlay.setEditorActive(false)
        Qt.callLater(syncRegions)
    }
    onWidthChanged: Qt.callLater(syncRegions)
    onHeightChanged: Qt.callLater(syncRegions)
    onReducedMotionChanged: { if(reducedMotion) {flight.stop(); flightProgress=1} }
    Component.onCompleted: Qt.callLater(syncRegions)
    Connections {
        target: scene.model
        function onEventAboutToApply() { scene.capturePositions() }
        function onLastEventChanged() { scene.playEvent() }
        function onSceneReset() { scene.stopEffects(); scene.closeMenu() }
        function onPeersChanged() {
            if(scene.panelKind!=="" && scene.panelKind!=="connect" && scene.model.peerStates[scene.panelPeerId]===undefined) scene.closePanel(false)
            Qt.callLater(scene.syncRegions)
        }
        function onRequestFailed(requestId,code) {
            if(!requestId) return
            if(panelLoader.item) {if(panelLoader.item.pendingRequestId===requestId) panelLoader.item.fail(code); return}
            scene.caption="보내지 못했어요. 연결을 확인한 뒤 다시 시도해주세요."; effectTimer.restart()
        }
    }
    Connections {
        target: scene.sessionClient
        function onNoticeReceived(text) {scene.caption=text; effectTimer.restart()}
        function onConnectionChanged() {
            if(!scene.sessionClient) return
            var request=scene.sessionClient.joinRequestId
            if(request && request!==scene.lastJoinRequest) {scene.caption="세션 참여 요청이 있어요. 메뉴바에서 확인해주세요."; effectTimer.restart()}
            scene.lastJoinRequest=request
        }
    }
    Repeater {
        id: pets; model: scene.model?scene.model.peers.length:0
        delegate: Pet {
            required property int index
            peer: scene.model?scene.model.peers[index]:({id:"",x:0,busy:false,pinned:true,inbox:[]})
            localPeer: scene.model && peer.id===scene.model.localPeerId
            tint: peer.tint || (index===0?"#bad8ba":"#e2b5a2")
            reducedMotion: scene.reducedMotion; suspendIdle: scene.effectKind!==""
            stageWidth: scene.width
            onPressed: scene.openPanel("wip",peer.id)
            onInboxPressed: scene.openPanel(localPeer?"inbox":"wip",peer.id)
            onGeometryChanged: scene.syncRegions()
        }
        onItemAdded: Qt.callLater(scene.syncRegions)
        onItemRemoved: Qt.callLater(scene.syncRegions)
    }
    Binding { target: scene; property: "effectSourceX"; value: scene.petFor(scene.effectActor)?scene.petFor(scene.effectActor).x+33:0 }
    Binding { target: scene; property: "effectTargetX"; value: scene.petFor(scene.effectTarget)?scene.petFor(scene.effectTarget).x+33:0 }
    Shape {
        anchors.fill: parent; visible: scene.effectKind==="rope"
        ShapePath {
            fillColor: "transparent"; strokeColor: "#c7b28c"; strokeWidth: 2.5
            strokeStyle: ShapePath.DashLine; dashPattern: [4,2]
            PathSvg { path: "M"+scene.effectSourceX+" "+(scene.height-71)+"Q"+
                ((scene.effectSourceX+scene.effectTargetX)/2)+" "+(scene.height-92)+" "+scene.effectTargetX+" "+(scene.height-71) }
        }
    }
    Prop {
        visible: scene.effectKind==="plane"; kind: "plane"; width: 36; height: 30; tint: "#ece8d8"
        x: Math.max(0,Math.min(scene.width-width,scene.effectSourceX+(scene.effectTargetX-scene.effectSourceX)*scene.flightProgress-width/2))
        y: scene.height-158-38*Math.sin(scene.flightProgress*Math.PI)+scene.flightProgress*24
        rotation: -10+35*scene.flightProgress
    }
    Prop {
        visible: scene.effectKind==="lure"; kind: "lure"; width: 23; height: 25; tint: "#dfb77f"
        x: Math.max(0,Math.min(scene.width-width,12+scene.effectFinalX*Math.max(0,scene.width-98-24)+21))
        y: scene.height-62
    }
    NumberAnimation { id: flight; target: scene; property: "flightProgress"; to: 1; duration: 1500; easing.type: Easing.InOutQuad }
    Timer { id: effectTimer; interval: 2400; onTriggered: scene.stopEffects() }
    Rectangle {
        visible: scene.displayedBubble!==""; radius: 8; color: "#e8e3d7"
        width: Math.min(scene.width-24,bubble.implicitWidth+20); height: bubble.height+12
        x: Math.max(12,Math.min(scene.width-width-12,scene.effectTargetX-width/2))
        y: Math.max(0,scene.height-126-height)
        Text { id: bubble; x: 10; y: 6; width: Math.min(implicitWidth,scene.width-44)
            text: scene.displayedBubble; color: "#29382d"; font.pixelSize: 11
            textFormat: Text.PlainText; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
    }
    Text {
        visible: scene.caption!=="" && scene.panelKind==="" && !scene.menuOpen; text: scene.caption; textFormat: Text.PlainText
        width: Math.min(scene.width-24,480); x: (scene.width-width)/2; y: 16
        wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
        color: "#eeeae1"; style: Text.Outline; styleColor: "#202c23"; font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
    }
    Loader {
        id: menuLoader; active: scene.menuOpen; z: 10
        width: Math.min(350,scene.width-24); height: Math.min(260,Math.max(80,scene.height-122))
        x: Math.max(12,Math.min(scene.width-width-12,scene.petFor(scene.menuPeerId)?scene.petFor(scene.menuPeerId).x:12))
        y: Math.max(0,scene.height-116-height-6)
        sourceComponent: ActionMenu {
            peer: scene.petFor(scene.menuPeerId)?scene.petFor(scene.menuPeerId).peer:({id:"",wip:"",busy:false,reachable:false})
            localPeer: scene.model && scene.menuPeerId===scene.model.localPeerId
            reducedMotion: scene.reducedMotion
            hasClient: !!scene.sessionClient
            onClosed: scene.closeMenu()
            onMotionToggled: scene.reducedMotion=!scene.reducedMotion
            onActionChosen: function(action,busy) { scene.model.requestAction(action,scene.menuPeerId,"",busy); scene.closeMenu() }
            onQuitRequested: Qt.quit()
            onPanelRequested: function(kind) { scene.openPanel(kind,scene.menuPeerId) }
            onConnectionRequested: scene.openConnection()
        }
        onLoaded: Qt.callLater(scene.syncRegions)
    }
    Loader {
        id: panelLoader; active: scene.panelKind!==""; z: 11
        width: Math.min(350,scene.width-24); height: Math.min(260,Math.max(128,scene.height-122))
        x: Math.max(12,Math.min(scene.width-width-12,scene.petFor(scene.panelPeerId)?scene.petFor(scene.panelPeerId).x:12))
        y: Math.max(0,scene.height-116-height-6)
        sourceComponent: scene.panelKind==="connect"?connectComponent:scene.panelKind==="wip"?wipComponent:scene.panelKind==="note"?noteComponent:inboxComponent
        onLoaded: Qt.callLater(scene.syncRegions)
    }
    readonly property var panelPeer: model && model.peerStates[panelPeerId]!==undefined?model.peerStates[panelPeerId]:({id:"",wip:"",busy:false,reachable:false,inbox:[]})
    Component {
        id: connectComponent
        ConnectPanel {
            client: scene.sessionClient; model: scene.model; serverUrl: scene.serverUrl
            onClosed: scene.closePanel(true)
            onServerSelected: function(url) {scene.serverUrl=url; if(scene.sessionClient) scene.sessionClient.connectTo(url)}
        }
    }
    Component {
        id: wipComponent
        WipPanel {
            peer: scene.panelPeer; model: scene.model; localPeer: scene.model && scene.panelPeerId===scene.model.localPeerId; localOnline: scene.localOnline
            onClosed: scene.closePanel(true)
            onNavigate: function(kind) { if(kind==="menu") scene.openMenu(scene.panelPeerId); else scene.openPanel(kind,scene.panelPeerId) }
            onActionChosen: function(action,text,busy,requestId) { scene.model.requestAction(action,scene.panelPeerId,text,busy,requestId) }
        }
    }
    Component {
        id: noteComponent
        NoteComposer {
            peer: scene.panelPeer; model: scene.model; localOnline: scene.localOnline
            onClosed: scene.closePanel(true)
            onConfirmed: scene.closePanel(false)
            onActionChosen: function(action,text,busy,requestId) { scene.model.requestAction(action,scene.panelPeerId,text,busy,requestId) }
        }
    }
    Component {
        id: inboxComponent
        InboxPanel {
            peer: scene.panelPeer; model: scene.model; localOnline: scene.localOnline
            onClosed: scene.closePanel(true)
            onActionChosen: function(action,text,busy,requestId) { scene.model.requestAction(action,scene.panelPeerId,text,busy,requestId) }
        }
    }
}
