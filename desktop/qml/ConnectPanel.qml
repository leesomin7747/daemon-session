import QtQuick
import QtQuick.Controls
PanelFrame {
    id: panel
    required property var client
    required property var model
    property string serverUrl: "ws://127.0.0.1:8787/"
    property bool confirmUnpair: false
    property string activeControlId: ""
    readonly property bool connecting: client && ["loading_credentials","registering","saving_credentials","authenticating","connecting","forgetting"].indexOf(client.status)>=0
    signal serverSelected(string url)
    title: "같은 세션에 접속"
    initialFocusItem: address
    function errorText(code) {
        if(code==="secret_store") return "비밀 저장소를 열지 못했어요. 잠금을 확인하고 다시 접속해주세요."
        if(code==="tls") return "서버 인증서를 확인할 수 없어요. 서버 주소와 인증서를 확인해주세요."
        if(code==="refused" || code==="invite_expired") return "초대를 확인하지 못했어요. 새 코드로 다시 시도해주세요."
        if(code==="rate_limited") return "잠깐 쉬었다가 다시 시도해주세요."
        if(code==="unsafe_url") return "서버 주소를 확인해주세요. 외부 서버는 wss:// 주소를 사용해요."
        return "연결을 마치지 못했어요. 상태를 확인하고 다시 시도해주세요."
    }
    Connections {
        target: panel.client
        function onError(requestId,code) {
            if(requestId && panel.activeControlId && requestId!==panel.activeControlId) return
            panel.activeControlId=""; panel.statusText=panel.errorText(code)
        }
        function onControlResult(requestId,body) {if(requestId===panel.activeControlId) {panel.activeControlId=""; panel.statusText=""}}
    }
    Text { width: panel.bodyWidth; text: "세션 서버"; color: "#c6d2c5"; font.pixelSize: 11 }
    TextField {
        id: address; objectName: "serverAddress"; width: panel.bodyWidth; height: 34
        text: panel.serverUrl; placeholderText: "wss://세션 서버 주소"; readOnly: panel.connecting || (panel.client && panel.client.connected)
        Accessible.name: "세션 서버 주소"; selectByMouse: true; font.pixelSize: 12
        color: "#eeeae1"; placeholderTextColor: "#b5c3b8"; selectionColor: "#bad8ba"; selectedTextColor: "#202c23"
        background: Rectangle { radius: 5; color: "#202c23"; border.color: address.activeFocus?"#bad8ba":"#637768"; border.width: address.activeFocus?2:1 }
    }
    PanelButton {
        objectName: "connectServer"; width: panel.bodyWidth; primary: true
        text: panel.connecting?"접속 중…":panel.client && panel.client.connected?"접속 종료":"세션 서버에 접속"
        enabled: panel.client && !panel.connecting && panel.client.endpointAllowed(address.text)
        onClicked: {
            panel.statusText=""; panel.confirmUnpair=false
            if(panel.client.connected) panel.client.stop(); else panel.serverSelected(address.text)
        }
    }
    Text {
        width: panel.bodyWidth; wrapMode: Text.Wrap; textFormat: Text.PlainText; color: "#c6d2c5"; font.pixelSize: 11
        text: !panel.client?"로컬 미리보기예요.":panel.client.status==="reconnecting"?"연결을 다시 찾고 있어요. 쪽지는 자동으로 다시 보내지 않아요.":panel.client.status==="awaiting_approval"?"초대한 기기의 승인을 기다리고 있어요.":panel.client.paired?"연결을 기억했어요. 다음에도 같은 세션으로 돌아와요.":panel.client.connected?"초대를 만들거나 친구의 코드를 입력해요.":"한 번 연결하면 다음 접속은 자동으로 이어져요."
    }
    Column {
        visible: panel.client && panel.client.connected && !panel.client.paired
        width: panel.bodyWidth; spacing: 8
        PanelButton {
            objectName: "createInvite"; width: parent.width; text: "초대 코드 만들기"
            enabled: panel.client && !panel.connecting && panel.client.status!=="awaiting_approval" && panel.activeControlId===""
            onClicked: panel.activeControlId=panel.client.requestControl("invite.create",{})
        }
        TextArea {
            objectName: "inviteCode"; visible: panel.client && panel.client.inviteCode!==""; width: parent.width
            text: panel.client?panel.client.inviteCode:""; textFormat: TextEdit.PlainText
            readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; font.pixelSize: 12
            color: "#bad8ba"; selectionColor: "#bad8ba"; selectedTextColor: "#202c23"; background: null; padding: 0
            Accessible.name: "10분 동안 유효한 초대 코드"
        }
        Text { visible: panel.client && panel.client.inviteCode!==""; width: parent.width; text: "10분 동안 유효해요. 초대한 친구의 요청인지 확인하고 승인해주세요."; wrapMode: Text.Wrap; color: "#c6d2c5"; font.pixelSize: 11 }
        TextField {
            id: join; objectName: "joinCode"; width: parent.width; height: 34
            placeholderText: "친구의 초대 코드"; Accessible.name: "친구의 초대 코드"; selectByMouse: true
            color: "#eeeae1"; placeholderTextColor: "#b5c3b8"; selectionColor: "#bad8ba"; selectedTextColor: "#202c23"; font.pixelSize: 12
            background: Rectangle { radius: 5; color: "#202c23"; border.color: join.activeFocus?"#bad8ba":"#637768"; border.width: join.activeFocus?2:1 }
        }
        PanelButton {
            objectName: "joinSession"; width: parent.width; text: "참여 요청 보내기"
            enabled: panel.client && !panel.connecting && panel.client.status!=="awaiting_approval" && panel.activeControlId==="" && /^[A-Za-z0-9_-]{43}$/.test(join.text)
            onClicked: panel.activeControlId=panel.client.requestControl("invite.join",{invite:join.text})
        }
    }
    Column {
        width: panel.bodyWidth; spacing: 8; visible: panel.client && panel.client.joinRequestId!==""
        Text { width: parent.width; text: "참여 요청 · "+(panel.client?panel.client.pendingPeerId:""); textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#eeeae1"; font.pixelSize: 12 }
        Text { width: parent.width; text: "초대한 기기의 요청이 맞는지 확인해주세요."; wrapMode: Text.Wrap; color: "#c6d2c5"; font.pixelSize: 11 }
        PanelButton {
            objectName: "approveJoin"; width: parent.width; text: "같은 세션에 연결"; primary: true
            enabled: panel.client && panel.client.connected && !panel.connecting && panel.activeControlId===""
            onClicked: panel.activeControlId=panel.client.requestControl("invite.approve",{joinRequestId:panel.client.joinRequestId})
        }
    }
    CheckBox {
        objectName: "showOwnOffline"; text: "offline에도 내 데몬 두기"; checked: panel.model?panel.model.showLocalOffline:true
        palette.windowText: "#eeeae1"; palette.text: "#eeeae1"; palette.highlight: "#bad8ba"; font.pixelSize: 11
        palette.base: "#202c23"; palette.button: "#26352b"; palette.buttonText: "#bad8ba"
        onClicked: panel.model.showLocalOffline=checked
    }
    PanelButton {
        objectName: "startUnpair"; visible: panel.client && panel.client.paired; width: panel.bodyWidth
        text: "친구와 연결 해제"; enabled: !panel.connecting
        onClicked: panel.confirmUnpair=true
    }
    Column {
        visible: panel.confirmUnpair; width: panel.bodyWidth; spacing: 8
        Text { width: parent.width; text: "두 기기의 연결을 해제해요. 다시 함께하려면 새 초대가 필요해요."; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#eee2b4"; font.pixelSize: 11 }
        PanelButton { objectName: "confirmUnpair"; width: parent.width; text: "연결 해제하기"; enabled: panel.client && panel.client.connected && !panel.connecting; onClicked: {panel.confirmUnpair=false; panel.client.disconnectAndForget()} }
    }
    Text { objectName: "connectionStatus"; width: panel.bodyWidth; text: panel.statusText; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#eee2b4"; font.pixelSize: 11 }
}
