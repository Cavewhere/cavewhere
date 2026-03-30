import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: itemId

    QC.GroupBox {
        title: "Git Identity"

        ColumnLayout {
            RowLayout {
                Text {
                    text: "Name"
                    Layout.minimumWidth: 60
                }

                QC.TextField {
                    placeholderText: "Your name"
                    text: RootData.account.name
                    onEditingFinished: RootData.account.name = text
                    Layout.fillWidth: true
                }
            }

            RowLayout {
                Text {
                    text: "Email"
                    Layout.minimumWidth: 60
                }

                QC.TextField {
                    placeholderText: "your@email.com"
                    text: RootData.account.email
                    onEditingFinished: RootData.account.email = text
                    Layout.fillWidth: true
                }
            }
        }
    }
}
