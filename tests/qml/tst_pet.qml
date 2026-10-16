import QtQuick
import QtQuick.Window
import QtTest
import Daemon 1.0
import "../../desktop/qml" as Desktop

Item {
    width: 1000; height: 680
    Component { id: modelFactory; SceneModel { localPeerId: "a" } }
    Component { id: sceneFactory; Desktop.Main { width: 800; height: 320 } }
    TestCase {
        name: "PetScene"; when: windowShown
        property var modelA
        property var modelB
        property var a
        property var b
        function init() {
            modelA=createTemporaryObject(modelFactory,this)
            modelB=createTemporaryObject(modelFactory,this)
            modelB.localPeerId="b"
            fixtures.reset(modelA,0.75,false); fixtures.reset(modelB,0.75,false)
            a=createTemporaryObject(sceneFactory,parent,{model:modelA})
            b=createTemporaryObject(sceneFactory,parent,{model:modelB,width:500,y:330})
            verify(a && b); wait(0)
            verify(a.petFor("b")!==null,"Snapshot must create both pet delegates")
        }
        function event(action,x,revision,busy,queued,text) {
            return {eventId:"e"+revision,requestId:"r"+revision,epoch:"epoch-1",revision:String(revision),
                action:action,actorId:action==="setBusy"?"b":"a",targetId:"b",caption:"동일한 안내",
                bubble:queued||action==="setBusy"?"":"같은 말풍선",finalX:x,
                busy:busy||false,queued:queued||false,text:text||""}
        }
        function test_identicalEventAndPinnedPosition() {
            var e=event("rope",0.37,1)
            fixtures.deliver(modelA,e); fixtures.deliver(modelB,e)
            tryCompare(a,"displayedBubble","같은 말풍선")
            compare(a.displayedBubble,b.displayedBubble)
            compare(a.petFor("b").tint,b.petFor("b").tint)
            verify(a.petFor("b")!==null)
            tryCompare(a.petFor("b"),"moving",false,2300)
            compare(a.petFor("b").peer.x,0.37); compare(b.petFor("b").peer.x,0.37)
            compare(a.petFor("b").peer.pinned,true)
            var x=a.petFor("b").x
            fixtures.deliver(modelA,event("coffee",0.37,2))
            wait(100); compare(a.petFor("b").x,x)
        }
        function test_snapshotInterruptsAnimationWithoutReplay() {
            fixtures.deliver(modelA,event("lure",0.41,1)); tryCompare(a,"effectKind","lure")
            fixtures.reset(modelA,0.81,false,1)
            tryCompare(a,"effectKind","")
            compare(a.petFor("b").moving,false); compare(a.petFor("b").peer.x,0.81)
            compare(a.displayedBubble,"")
        }
        function test_reducedMotionKeepsFinalPositionAndBubble() {
            a.reducedMotion=true
            fixtures.deliver(modelA,event("rope",0.37,1))
            tryCompare(a,"displayedBubble","같은 말풍선")
            compare(a.petFor("b").moving,false)
            fuzzyCompare(a.petFor("b").x,12+0.37*(800-98-24),0.1)
        }
        function test_busyQueuesWithoutAnimationOrMenu() {
            fixtures.reset(modelA,0.75,true)
            fixtures.deliver(modelA,event("coffee",0.75,1,true,true,"커피를 두었어요"))
            tryCompare(a.petFor("b"),"queuedCount",1)
            compare(a.petFor("b").busy,true); compare(a.effectKind,"")
            compare(a.displayedBubble,"")
            compare(a.menuOpen,false)
        }
        function test_edgesAndPropsStayWithinScene() {
            a.width=320
            fixtures.reset(modelA,1,true)
            fixtures.deliver(modelA,event("note",1,1,true,true,"쪽지"))
            wait(0)
            var pet=a.petFor("b"); verify(pet!==null)
            verify(pet.x>=0 && pet.x+pet.width<=a.width)
            var rects=a.interactiveRegions
            verify(rects.length>=3)
            for(var i=0;i<rects.length;i++) {
                verify(rects[i].x>=0 && rects[i].y>=0)
                verify(rects[i].x+rects[i].width<=a.width+0.1)
                verify(rects[i].y+rects[i].height<=a.height+0.1)
            }
            var pile=findChild(pet,"pileButton"); verify(pile!==null)
            var p=pile.mapToItem(a,pile.width/2,pile.height/2)
            var hit=false
            for(i=0;i<rects.length;i++) hit=hit||(p.x>=rects[i].x && p.x<=rects[i].x+rects[i].width
                && p.y>=rects[i].y && p.y<=rects[i].y+rects[i].height)
            verify(hit,"A visible queued prop must be interactive")
        }
        function test_planeAndRopeFollowCurrentPetPositions() {
            a.reducedMotion=true
            fixtures.deliver(modelA,event("rope",0.37,1))
            fixtures.deliver(modelA,event("plane",0.37,2))
            tryCompare(a,"effectKind","plane")
            fuzzyCompare(a.effectSourceX,a.petFor("a").x+33,0.1)
            fuzzyCompare(a.effectTargetX,a.petFor("b").x+33,0.1)
        }
        function test_menuClickIsIntentAndEscapeCloses() {
            a.width=500; wait(0)
            a.openMenu("b"); tryCompare(a,"menuOpen",true)
            var rope=findChild(a,"action_rope"); verify(rope!==null)
            verify(waitForRendering(rope))
            mouseClick(rope); tryCompare(a,"menuOpen",false)
            compare(a.petFor("b").peer.x,0.75); compare(a.effectKind,"")
            a.openMenu("a"); tryCompare(a,"menuOpen",true)
            wait(0); keyClick(Qt.Key_Escape); tryCompare(a,"menuOpen",false)
        }
        function test_busyMenuDisablesOnlyPranks() {
            fixtures.reset(modelA,0.75,true)
            a.openMenu("b"); wait(0)
            var ping=findChild(a,"action_ping"), coffee=findChild(a,"action_coffee")
            verify(ping!==null && coffee!==null)
            compare(ping.enabled,false); compare(coffee.enabled,true)
        }
        function test_keyboardFocusStaysVisible_data() {
            return [{tag:"normal",height:380},{tag:"short",height:250}]
        }
        function test_keyboardFocusStaysVisible(data) {
            a.height=data.height
            a.openMenu("b"); tryCompare(a,"menuOpen",true)
            var close=findChild(a,"menuClose"), rope=findChild(a,"action_rope")
            verify(close!==null && rope!==null)
            verify(waitForRendering(rope)); tryCompare(close,"activeFocus",true)
            var viewport=rope.parent
            while(viewport && typeof viewport.contentY==="undefined") viewport=viewport.parent
            verify(viewport!==null)
            function checkVisible() {
                var focused=a.Window.window.activeFocusItem
                verify(focused!==null)
                var p=focused.mapToItem(viewport,0,0)
                verify(p.y>=-0.1 && p.y+focused.height<=viewport.height+0.1,
                    "Focused control must be visible: "+focused.objectName+" y="+p.y)
                verify(viewport.contentY>=0 && viewport.contentY<=Math.max(0,viewport.contentHeight-viewport.height))
            }
            for(var i=0;i<8;i++) { keyClick(Qt.Key_Tab); wait(0); checkVisible() }
            verify(viewport.contentY>0,"Lower controls require scrolling")
            for(i=0;i<8;i++) { keyClick(Qt.Key_Tab,Qt.ShiftModifier); wait(0); checkVisible() }
            compare(close.activeFocus,true)
            keyClick(Qt.Key_Tab); keyClick(Qt.Key_Tab)
            tryCompare(rope,"activeFocus",true)
            keyClick(Qt.Key_Space); tryCompare(a,"menuOpen",false)
        }
    }
}
