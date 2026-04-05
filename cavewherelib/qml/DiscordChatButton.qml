import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

RoundButton {
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

    QC.Popup {
        padding: 10
        //popupType: QC.Popup.Item

        visible: chatButtonId.hovered

        y: parent.implicitHeight
        x: -popupContentId.implicitWidth

        contentItem: ColumnLayout{
            id: popupContentId

            QC.Label {
                text: "Join the CaveWhere community"
                font.pixelSize: Theme.fontSizeTitle
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter

                Online {
                    //Shows gray dot
                    visible: DiscordStatusChecker.devsOnlineCount == 0
                }

                QC.Label {
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

                QC.Label {
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
