import QtQuick as QQ
import QtQuick.Controls as Controls
import QtQuick.Layouts
import cavewherelib

Controls.RoundButton {
    id: chatButtonId

    icon.source: "qrc:/twbs-icons/icons/chat-right-text.svg"

    component Online : QQ.Rectangle {
        implicitWidth: 10
        implicitHeight: implicitWidth
        radius: implicitWidth * 0.5

        property bool online: false
        property bool yellow: false


        color: {
            if(online) {
                return "#5b9279" //green
            } else if(yellow) {
                return "#F8D824" //yellow
            } else {
                return "#676767" //gray
            }
        }
    }

    Controls.Popup {
        padding: 10
        //popupType: Controls.Popup.Item

        visible: chatButtonId.hovered

        y: parent.implicitHeight
        x: -popupContentId.implicitWidth

        contentItem: ColumnLayout{
            id: popupContentId

            Text {
                text: "Join the CaveWhere community"
                font.pixelSize: 20
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter

                Online {
                    //Shows gray dot
                    visible: DiscordStatusChecker.devsOnlineCount == 0
                }

                Text {
                    text: {
                        if(DiscordStatusChecker.devsOnlineCount) {
                            return "🌟 Support is online! 🌟"
                        } else {
                            return "Support offline"
                        }
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter

                Online {
                    online: DiscordStatusChecker.userCount > 0
                }

                Text {
                    id: onlineTextId
                    text: DiscordStatusChecker.userCount + " online"
                }
            }

        }
    }

    onClicked: {
        Qt.openUrlExternally(DiscordStatusChecker.server)
    }

    Online {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        online: DiscordStatusChecker.devsOnlineCount > 0
        yellow: DiscordStatusChecker.userCount > 0
        z:1
    }
}
