import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import QQuickGit

ColumnLayout {

    QC.GroupBox {
        title: "Git Identity"
        Layout.fillWidth: true

        PersonEdit {
            width: parent.width
            account: RootData.account
        }
    }
}
