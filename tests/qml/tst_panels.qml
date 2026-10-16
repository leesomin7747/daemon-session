import QtQuick
import QtQuick.Window
import QtTest
import Daemon 1.0
import "../../desktop/qml" as Desktop

Item {
    width: 900; height: 500
    Component { id: modelFactory; SceneModel { localPeerId: "a" } }
    Component { id: sceneFactory; Desktop.Main { width: 800; height: 380; reducedMotion: true } }
    TestCase {
        name: "Panels"; when: windowShown
        property var model
        property var scene
        SignalSpy { id: requests; signalName: "actionRequested" }
        function init() {
            model=createTemporaryObject(modelFactory,this)
            fixtures.panelState(model)
            scene=createTemporaryObject(sceneFactory,parent,{model:model})
            verify(scene!==null); requests.target=model; requests.clear(); wait(0)
        }
        function open(kind,id) {
            if(typeof scene.openPanel!=="function") { fail("Scene must expose explicit panel navigation"); return false }
            scene.openPanel(kind,id); wait(0)
            compare(scene.panelKind,kind)
            return true
        }
        function control(name) { var item=findChild(scene,name); verify(item!==null,name+" must exist"); return item }
        function click(name) { var item=control(name); verify(waitForRendering(item)); mouseClick(item); wait(0) }
        function deliver(action,actor,target,revision,text,busy,queued) {
            fixtures.deliver(model,{eventId:"e"+revision,requestId:"r"+revision,epoch:"epoch-1",revision:String(revision),
                action:action,actorId:actor,targetId:target,caption:"확정된 서버 안내",bubble:"",finalX:target==="a"?0.25:0.75,
                text:text||"",busy:busy||false,queued:queued||false})
            wait(0)
        }
        function test_petClickOpensOwnWipAndFriendIsReadOnly() {
            var pet=scene.petFor("a"); verify(pet!==null)
            verify(waitForRendering(pet)); mouseClick(pet,33,45); wait(0)
            if(typeof scene.panelKind==="undefined") { fail("Pet click must open WIP"); return }
            compare(scene.panelKind,"wip"); compare(control("wipEditor").text,"알고리즘 과제")
            keyClick(Qt.Key_Escape); compare(scene.panelKind,"")
            if(!open("wip","b")) return
            compare(findChild(scene,"wipEditor"),null)
            compare(control("friendWip").text,"<b>UX 점검</b>")
            compare(control("friendWip").textFormat,Text.PlainText)
        }
        function test_wipSaveWaitsForEventAndCanClear() {
            if(!open("wip","a")) return
            control("wipEditor").text="  <b>과제</b>\n알고리즘 😀  "
            click("saveWip")
            compare(requests.count,1); compare(requests.signalArguments[0][0],9)
            compare(requests.signalArguments[0][2],"<b>과제</b>\n알고리즘 😀")
            compare(scene.petFor("a").peer.wip,"알고리즘 과제")
            compare(control("saveWip").enabled,false)
            deliver("setWip","a","a",1,"<b>과제</b>\n알고리즘 😀")
            compare(scene.petFor("a").peer.wip,"<b>과제</b>\n알고리즘 😀")
            compare(control("saveWip").enabled,true)
            control("wipEditor").text=" \n "; click("saveWip")
            compare(requests.count,2); compare(requests.signalArguments[1][2],"")
            deliver("setWip","a","a",2,""); compare(scene.petFor("a").peer.wip,"")
        }
        function test_unicodeBoundaries_data() {
            return [{tag:"WIP",kind:"wip",peer:"a",editor:"wipEditor",button:"saveWip",limit:80},
                {tag:"note",kind:"note",peer:"b",editor:"noteEditor",button:"sendNote",limit:120}]
        }
        function test_unicodeBoundaries(data) {
            if(!open(data.kind,data.peer)) return
            var editor=control(data.editor), button=control(data.button)
            editor.text="😀".repeat(data.limit)
            compare(button.enabled,true); compare(control("textCount").text,data.limit+" / "+data.limit)
            editor.text+="가"; compare(button.enabled,false)
            editor.text="\ud800"; compare(button.enabled,false)
            editor.text="  \n "; compare(button.enabled,data.kind==="wip")
            compare(requests.count,0)
        }
        function test_noteWaitsForOwnReceiptAndKeepsDraftOnError() {
            if(!open("note","b")) return
            control("noteEditor").text="  커피 마시고 쉬자 😀\n<b>메모</b>  "
            click("sendNote"); compare(requests.count,1)
            compare(requests.signalArguments[0][0],7)
            compare(requests.signalArguments[0][1],"b")
            compare(requests.signalArguments[0][2],"커피 마시고 쉬자 😀\n<b>메모</b>")
            compare(scene.panelKind,"note"); compare(control("sendNote").enabled,false)
            model.actionRejected("offline"); wait(0)
            compare(control("sendNote").enabled,true)
            verify(control("noteEditor").text.indexOf("커피")>=0)
            verify(control("panelStatus").text.indexOf("연결")>=0)
            click("sendNote")
            deliver("note","b","a",1,"다른 쪽지")
            compare(scene.panelKind,"note")
            deliver("note","a","b",2,"커피 마시고 쉬자 😀\n<b>메모</b>")
            compare(scene.panelKind,""); compare(scene.caption,"확정된 서버 안내")
        }
        function test_offlineDisablesSendingButReceivedItemsRemainReadable() {
            fixtures.panelState(model,true,false,false,"알고리즘 과제",true)
            if(!open("note","b")) return
            control("noteEditor").text="안녕"; compare(control("sendNote").enabled,false)
            verify(control("panelStatus").text.indexOf("offline")>=0)
            if(!open("inbox","a")) return
            compare(control("inboxBody_note").text,"<b>과제</b>\n끝나면 쉬자 ☕")
            compare(control("inboxBody_note").textFormat,Text.PlainText)
            fixtures.panelState(model,false,false,false,"알고리즘 과제",true)
            if(!open("inbox","a")) return
            compare(control("ackInbox").enabled,false)
            verify(control("inboxBody_note")!==null)
        }
        function test_inboxOwnOnlyAndAcknowledgementWaitsForEvent() {
            fixtures.panelState(model,true,true,false,"알고리즘 과제",true)
            if(!open("inbox","a")) return
            click("ackInbox"); compare(requests.count,1); compare(requests.signalArguments[0][0],10)
            compare(scene.petFor("a").queuedCount,2)
            deliver("ackInbox","a","a",1,"")
            compare(scene.petFor("a").queuedCount,0)
            verify(control("inboxEmpty").visible)
            scene.openPanel("inbox","b"); wait(0)
            compare(scene.panelPeerId,"a")
        }
        function test_busyAndUnbusyNeverAutoOpenInbox() {
            fixtures.panelState(model,true,true,true)
            if(typeof scene.panelKind==="undefined") { fail("Panels must remain explicitly opened"); return }
            deliver("coffee","b","a",1,"커피를 두고 갔어요",true,true)
            compare(scene.panelKind,""); compare(scene.petFor("a").queuedCount,1)
            deliver("note","b","a",2,"옆에 있어",true,true)
            compare(scene.panelKind,""); compare(scene.petFor("a").queuedCount,2)
            deliver("setBusy","a","a",3,"",false,false)
            compare(scene.panelKind,""); compare(scene.petFor("a").queuedCount,2)
            var pile=findChild(scene.petFor("a"),"pileButton"); verify(pile!==null)
            mouseClick(pile); wait(0); compare(scene.panelKind,"inbox")
        }
        function test_keyboardNavigationAndMenuReturn() {
            if(!open("wip","a")) return
            var editor=control("wipEditor"); tryCompare(editor,"activeFocus",true)
            keyClick(Qt.Key_Tab); compare(control("saveWip").activeFocus,true)
            keyClick(Qt.Key_Tab,Qt.ShiftModifier); compare(editor.activeFocus,true)
            scene.openMenu("b"); wait(0)
            for(var i=0;i<10;i++) keyClick(Qt.Key_Tab)
            tryCompare(control("composeNote"),"activeFocus",true)
            keyClick(Qt.Key_Space); wait(0)
            compare(scene.panelKind,"note"); tryCompare(control("noteEditor"),"activeFocus",true)
            keyClick(Qt.Key_Escape); compare(scene.panelKind,"")
            compare(scene.menuOpen,true); tryCompare(control("menuClose"),"activeFocus",true)
            keyClick(Qt.Key_Escape); compare(scene.menuOpen,false)
        }
        function test_shortPanelFocusAndInputRegion() {
            scene.height=250
            if(!open("wip","a")) return
            for(var i=0;i<4;i++) {
                keyClick(Qt.Key_Tab); wait(0)
                var focused=scene.Window.window.activeFocusItem
                verify(focused!==null)
                var p=focused.mapToItem(scene,focused.width/2,focused.height/2)
                verify(p.y>=0 && p.y<=scene.height)
                var hit=false
                for(var j=0;j<scene.interactiveRegions.length;j++) {
                    var r=scene.interactiveRegions[j]
                    hit=hit||(p.x>=r.x && p.x<=r.x+r.width && p.y>=r.y && p.y<=r.y+r.height)
                }
                verify(hit,"Focused panel control must be in input mask")
            }
        }
        function test_trimMatchesSharedProtocol() {
            if(!open("wip","a")) return
            control("wipEditor").text="\ufeff작업\ufeff"
            click("saveWip"); compare(requests.count,1)
            compare(requests.signalArguments[0][2],"\ufeff작업\ufeff")
            deliver("setWip","a","a",1,"\ufeff작업\ufeff")
            compare(control("saveWip").enabled,true)
        }
        function test_ownWipCanOpenAppMenu() {
            if(!open("wip","a")) return
            var button=findChild(scene,"openAppMenu")
            verify(button!==null,"Own WIP must retain access to motion and app controls")
            if(!button) return
            button.forceActiveFocus(); keyClick(Qt.Key_Space); wait(0)
            compare(scene.menuOpen,true)
            compare(scene.panelKind,"")
        }
        function test_keyboardCanReadLongInboxWithoutAcknowledging() {
            fixtures.panelInbox(model)
            if(!open("inbox","a")) return
            var viewport=control("inboxBody_note-0").parent
            while(viewport && typeof viewport.contentY==="undefined") viewport=viewport.parent
            verify(viewport!==null)
            compare(viewport.contentY,0)
            keyClick(Qt.Key_PageDown); wait(0)
            verify(viewport.contentY>0,"Keyboard reader must be able to scroll received text")
            keyClick(Qt.Key_PageUp); wait(0); compare(viewport.contentY,0)
            compare(scene.petFor("a").queuedCount,12); compare(requests.count,0)
        }
        function test_multilineEditorKeepsCaretVisible_data() {
            return [{tag:"WIP",kind:"wip",id:"a",editor:"wipEditor",height:380},
                {tag:"note",kind:"note",id:"b",editor:"noteEditor",height:380},
                {tag:"short-WIP",kind:"wip",id:"a",editor:"wipEditor",height:250},
                {tag:"short-note",kind:"note",id:"b",editor:"noteEditor",height:250}]
        }
        function test_multilineEditorKeepsCaretVisible(data) {
            scene.height=data.height
            if(!open(data.kind,data.id)) return
            var editor=control(data.editor)
            editor.text="a\n".repeat(19)+"z"; editor.cursorPosition=editor.text.length
            editor.forceActiveFocus(); verify(waitForRendering(editor)); wait(0)
            var viewport=editor.parent
            while(viewport && typeof viewport.contentY==="undefined") viewport=viewport.parent
            verify(viewport!==null)
            function checkCaret() {
                var r=editor.cursorRectangle, p=editor.mapToItem(viewport,r.x,r.y)
                verify(p.y>=-0.1 && p.y+r.height<=viewport.height+0.1,
                    "Actual caret must be inside the editor viewport: y="+p.y+" height="+viewport.height)
            }
            checkCaret()
            verify(viewport.clip)
            for(var i=0;i<25;i++) keyClick(Qt.Key_Up)
            wait(0); checkCaret()
            editor.text=""
            for(i=0;i<10;i++) { keyClick(Qt.Key_A); keyClick(Qt.Key_Return) }
            keyClick(Qt.Key_Z); wait(0); checkCaret()
            compare(editor.text,"a\n".repeat(10)+"z")
            compare(requests.count,0)
        }
    }
}
