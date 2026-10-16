import QtQuick
import QtQuick.Controls
Button {
    id: button
    property bool primary: false
    implicitHeight: 34
    font.pixelSize: 12
    Accessible.name: text
    background: Rectangle {
        radius: 5
        color: button.primary?(button.down?"#a1be9c":"#bbd6b7"):(button.down?"#455a49":button.hovered?"#3a4d40":"#26352b")
        border.color: button.activeFocus?"#eeeae1":"#617368"
        border.width: button.activeFocus?2:1
        opacity: button.enabled?1:0.4
    }
    contentItem: Text {
        text: button.text; font: button.font; textFormat: Text.PlainText
        color: button.primary?"#263b29":"#eeeae1"; opacity: button.enabled?1:0.45
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
}
