import QtQuick
import QtTest
import Daemon 1.0
import "../../desktop/qml" as Desktop
Item {
    width: 900; height: 500
    Component { id: modelFactory; SceneModel { localPeerId: "a" } }
    Component { id: sceneFactory; Desktop.Main { width: 800; height: 380; reducedMotion: true } }
    Component {
        id: clientFactory
        QtObject {
            property bool connected: false
            property bool paired: false
            property string status: "offline"
            property string inviteCode: ""
            property string joinRequestId: ""
            property string pendingPeerId: ""
            property string serverUrl: "ws://127.0.0.1:8787/"
            property int approvals: 0
            property int revokes: 0
            signal error(string requestId,string code)
            signal controlResult(string requestId,var body)
            signal noticeReceived(string text)
            signal connectionChanged()
            function endpointAllowed(url) { return String(url).indexOf("wss://")===0 || String(url).indexOf("ws://127.0.0.1:")===0 }
            function connectTo(url) { serverUrl=String(url); status="connecting" }
            function stop() { connected=false; status="offline" }
            function requestControl(op,body) { if(op==="invite.approve") approvals++; return "control-id" }
            function disconnectAndForget() { revokes++ }
        }
    }
    TestCase {
        name: "ConnectionUI"; when: windowShown
        property var model
        property var client
        property var scene
        function init() {
            model=createTemporaryObject(modelFactory,this); fixtures.panelState(model)
            client=createTemporaryObject(clientFactory,this)
            scene=createTemporaryObject(sceneFactory,parent,{model:model,sessionClient:client})
            verify(scene!==null); wait(0)
        }
        function open() {
            if(typeof scene.openConnection!=="function") { fail("Connection settings must be explicitly openable"); return false }
            scene.openConnection(); wait(0); compare(scene.panelKind,"connect"); return true
        }
        function control(name) { var item=findChild(scene,name); verify(item!==null,name+" must exist"); return item }
        function activate(name) {var item=control(name); item.forceActiveFocus(); keyClick(Qt.Key_Space); wait(0)}
        function test_validServerAndBusyState() {
            if(!open()) return
            var address=control("serverAddress"), button=control("connectServer")
            address.text="ws://example.test/"; compare(button.enabled,false)
            address.text="wss://example.test/"; compare(button.enabled,true)
            activate("connectServer"); compare(client.serverUrl,"wss://example.test/")
            compare(button.enabled,false)
        }
        function test_approvalAlwaysRequiresExplicitAction() {
            client.connected=true; client.status="connected"; client.joinRequestId="join"; client.pendingPeerId="requesting-device"
            if(!open()) return
            compare(client.approvals,0); verify(control("approveJoin").visible)
            activate("approveJoin"); compare(client.approvals,1)
        }
        function test_unpairHasExplicitConfirmation() {
            client.connected=true; client.paired=true; client.status="connected"
            if(!open()) return
            activate("startUnpair"); compare(client.revokes,0)
            verify(control("confirmUnpair").visible)
            activate("confirmUnpair"); compare(client.revokes,1)
        }
        function test_inviteTextIsPlainAndInputIsValidated() {
            client.connected=true; client.status="connected"; client.inviteCode="a".repeat(43)
            if(!open()) return
            compare(control("inviteCode").text,client.inviteCode)
            compare(control("inviteCode").textFormat,TextEdit.PlainText)
            control("joinCode").text="short"; compare(control("joinSession").enabled,false)
            control("joinCode").text="a".repeat(43); compare(control("joinSession").enabled,true)
        }
        function test_optionalOwnOfflinePet() {
            if(!open()) return
            compare(model.showLocalOffline,true)
            activate("showOwnOffline"); compare(model.showLocalOffline,false)
        }
        function test_receivedRequestNeverAutoOpensSettings() {
            if(typeof scene.openConnection!=="function") {fail("Connection UI must exist"); return}
            client.joinRequestId="pending"; client.pendingPeerId="peer"; client.connectionChanged(); wait(0)
            compare(scene.panelKind,"")
            verify(scene.caption.indexOf("참여")>=0,"Approval requests need a passive indication")
        }
        function test_keyboardCanOpenAndClose() {
            if(!open()) return
            tryCompare(control("serverAddress"),"activeFocus",true)
            keyClick(Qt.Key_Tab); compare(control("connectServer").activeFocus,true)
            keyClick(Qt.Key_Escape); compare(scene.panelKind,"")
        }
        function test_receivedGiftStillPlaysWithConnectionOpen() {
            if(!open()) return
            fixtures.deliver(model,{eventId:"connection-gift",requestId:"remote-coffee",epoch:"epoch-1",revision:"1",
                action:"coffee",actorId:"b",targetId:"a",caption:"커피 두고 갈게",bubble:"한 모금만",finalX:0.25,text:"",busy:false,queued:false})
            wait(0)
            compare(scene.effectKind,"coffee"); compare(scene.displayedBubble,"한 모금만")
            compare(scene.panelKind,"connect")
        }
    }
}
