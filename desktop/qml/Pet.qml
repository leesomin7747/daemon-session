import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

Item {
    id: pet
    required property var peer
    property bool localPeer: false
    property color tint: "#bad8ba"
    property bool reducedMotion: false
    property bool suspendIdle: false
    property real stageWidth: parent.width
    readonly property bool busy: peer.busy
    readonly property int queuedCount: peer.inbox.length
    readonly property bool moving: moveAnimation.running
    property string activeAction: ""
    property real moveOffset: 0
    property real idleOffset: 0
    property real bob: 0
    property real tilt: 0
    property real jitter: 0
    readonly property real baseX: Math.min(12,Math.max(0,(stageWidth-width)/2))+peer.x*Math.max(0,stageWidth-width-24)
    objectName: "pet_"+peer.id
    width: 98; height: 100
    x: Math.max(0,Math.min(stageWidth-width,baseX+moveOffset+idleOffset))
    y: parent.height-height-16
    signal pressed()
    signal inboxPressed()
    signal geometryChanged()
    onXChanged: geometryChanged()
    onYChanged: geometryChanged()
    onQueuedCountChanged: Qt.callLater(geometryChanged)
    onBusyChanged: Qt.callLater(geometryChanged)
    onReducedMotionChanged: if(reducedMotion) stopAction()
    function inputRects(scene) {
        var result=[avatar.mapToItem(scene,Qt.rect(0,0,avatar.width,avatar.height)),
                    name.mapToItem(scene,Qt.rect(0,0,name.width,name.height))]
        if(pile.visible) result.push(pile.mapToItem(scene,Qt.rect(0,0,pile.width,pile.height)))
        if(feed.visible) result.push(feed.mapToItem(scene,Qt.rect(0,0,feed.width,feed.height)))
        if(desk.visible) result.push(desk.mapToItem(scene,Qt.rect(0,0,desk.width,desk.height)))
        return result
    }
    function stopAction() {
        moveAnimation.stop(); hop.stop(); shake.stop(); perk.stop(); bonk.stop()
        moveOffset=0; bob=0; tilt=0; jitter=0; activeAction=""
    }
    function play(event,fromX) {
        stopAction()
        if(event.queued || busy || ["setBusy","setWip","ackInbox","note"].indexOf(event.action)>=0) return
        activeAction=event.action
        if(reducedMotion) return
        if(event.action==="rope" || event.action==="lure") { moveOffset=fromX-baseX; moveAnimation.start() }
        else if(event.action==="ping") hop.start()
        else if(event.action==="coffee") perk.start()
        else if(event.action==="energy") shake.start()
        else if(event.action==="plane") bonk.start()
    }
    SequentialAnimation {
        id: idle; running: !pet.peer.pinned && !pet.busy && !pet.reducedMotion && !pet.suspendIdle
        loops: Animation.Infinite
        NumberAnimation { target: pet; property: "idleOffset"; to: 10; duration: 8500; easing.type: Easing.InOutSine }
        NumberAnimation { target: pet; property: "idleOffset"; to: -10; duration: 8500; easing.type: Easing.InOutSine }
        onRunningChanged: if(!running) pet.idleOffset=0
    }
    NumberAnimation { id: moveAnimation; target: pet; property: "moveOffset"; to: 0; duration: 1800; easing.type: Easing.InOutCubic }
    SequentialAnimation {
        id: hop; loops: 2
        NumberAnimation { target: pet; property: "bob"; to: -12; duration: 240; easing.type: Easing.OutCubic }
        NumberAnimation { target: pet; property: "bob"; to: 0; duration: 340; easing.type: Easing.OutCubic }
    }
    SequentialAnimation {
        id: perk
        NumberAnimation { target: pet; property: "bob"; to: -7; duration: 600; easing.type: Easing.OutCubic }
        NumberAnimation { target: pet; property: "bob"; to: 0; duration: 600; easing.type: Easing.OutCubic }
    }
    SequentialAnimation {
        id: shake; loops: 8
        NumberAnimation { target: pet; property: "jitter"; to: -3; duration: 90 }
        NumberAnimation { target: pet; property: "jitter"; to: 3; duration: 90 }
        onStopped: pet.jitter=0
    }
    SequentialAnimation {
        id: bonk
        PauseAnimation { duration: 1050 }
        NumberAnimation { target: pet; property: "tilt"; to: 13; duration: 150 }
        NumberAnimation { target: pet; property: "tilt"; to: 0; duration: 250; easing.type: Easing.OutCubic }
    }
    Item {
        id: avatar; x: pet.jitter; y: 17+pet.bob; width: 68; height: 67; rotation: pet.tilt
        onXChanged: pet.geometryChanged()
        onYChanged: pet.geometryChanged()
        onRotationChanged: pet.geometryChanged()
        Accessible.role: Accessible.Button
        Accessible.name: (pet.localPeer?"내 데몬":"친구 데몬")+(pet.busy?" · busy":"")+" · 메뉴 열기"
        Accessible.onPressAction: pet.pressed()
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: pet.pressed() }
        Shape {
            width: 80; height: 80; scale: 0.825; transformOrigin: Item.TopLeft
            ShapePath {
                fillColor: pet.tint; strokeWidth: -1
                PathSvg { path: "M21 56Q12 70 23 72H57Q68 70 59 56L61 19L48 27Q40 24 32 27L19 19Z" }
            }
            ShapePath {
                fillColor: "transparent"; strokeColor: pet.tint; strokeWidth: 7; capStyle: ShapePath.RoundCap
                PathSvg { path: "M23 64Q10 58 10 48" }
            }
            ShapePath {
                fillColor: "transparent"; strokeColor: "#26382d"; strokeWidth: 2.5; capStyle: ShapePath.RoundCap
                PathSvg { path: "M30 43V46M50 43V46M37 52Q40 55 43 52" }
            }
            ShapePath {
                fillColor: "transparent"; strokeColor: "#7c8971"; strokeWidth: 2; capStyle: ShapePath.RoundCap
                PathSvg { path: "M25 27L28 32M55 27L52 32" }
            }
            ShapePath {
                fillColor: "transparent"; strokeColor: pet.busy?"#283c31":"transparent"; strokeWidth: 2.5
                PathSvg { path: "M26 38H34Q37 38 37 41V47Q37 50 34 50H26Q23 50 23 47V41Q23 38 26 38ZM46 38H54Q57 38 57 41V47Q57 50 54 50H46Q43 50 43 47V41Q43 38 46 38ZM37 42H43M18 40H23M57 40H62" }
            }
        }
        Rectangle {
            visible: pet.activeAction==="blanket"; x: 2; y: 36; width: 63; height: 30
            radius: 10; color: "#a5b2d3"
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 5; radius: 2; color: "#7e90b6" }
        }
        Prop { id: feed; x: 57; y: 30; width: 25; height: 27
            kind: pet.activeAction; visible: ["coffee","energy"].indexOf(pet.activeAction)>=0
            MouseArea { anchors.fill: parent; onClicked: pet.pressed() }
            onVisibleChanged: Qt.callLater(pet.geometryChanged)
        }
    }
    Shape {
        id: desk; visible: pet.busy; x: 0; y: 61; width: 72; height: 30
        MouseArea { anchors.fill: parent; onClicked: pet.pressed() }
        ShapePath { fillColor: "transparent"; strokeColor: "#c5b28d"; strokeWidth: 4
            PathSvg { path: "M3 8H69M10 8V29M62 8V29" } }
        ShapePath { fillColor: "#334439"; strokeColor: "#9cae98"; strokeWidth: 1.5
            PathSvg { path: "M26 8V0H48V8Z" } }
    }
    Text {
        id: name; x: 0; y: 85; width: 68; height: 15; text: (pet.localPeer?"나":"친구")+(pet.busy?" · busy":"")
        color: "#eeeae1"; style: Text.Outline; styleColor: "#202c23"; font.pixelSize: 10
        textFormat: Text.PlainText; horizontalAlignment: Text.AlignHCenter
        MouseArea { anchors.fill: parent; onClicked: pet.pressed() }
    }
    Button {
        id: pile; objectName: "pileButton"; x: 66; y: 43; width: 32; height: 44
        visible: pet.queuedCount>0; onClicked: pet.inboxPressed()
        Accessible.name: (pet.localPeer?"내":"친구")+" 받은 선물과 쪽지 "+pet.queuedCount+"개"
        background: Rectangle { color: pile.hovered?"#374a3d":"#2b382f"; radius: 5; border.color: "#627269" }
        contentItem: Item {
            Prop { x: 5; y: 0; width: 22; height: 24; kind: pet.queuedCount?pet.peer.inbox[pet.queuedCount-1].kind:"note" }
            Text { y: 25; width: parent.width; text: pet.queuedCount; font.pixelSize: 10; color: "#eeeae1"; horizontalAlignment: Text.AlignHCenter }
        }
    }
}
