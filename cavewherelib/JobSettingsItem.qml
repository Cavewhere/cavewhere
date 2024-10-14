import QtQuick 2.0 as QQ
import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.12
import cavewherelib

ColumnLayout {
    id: settingsId
    property JobSettings jobSettings: RootData.settings.jobSettings

    QC.GroupBox {
        title: "Job Settings"

        ColumnLayout {
            RowLayout {
                InformationButton {
                    showItemOnClick: threadHelpAreaId
                }

                Text {
                    text: "Max Number of Threads"
                }

                QC.SpinBox {
                    from: 1
                    to: settingsId.jobSettings.idleThreadCount
                    value: settingsId.jobSettings.threadCount
                    onValueChanged: {
                        settingsId.jobSettings.threadCount = value;
                    }
                }
            }

            HelpArea {
                id: threadHelpAreaId
                Layout.fillWidth: true
                text: "By default and the recommend setting, CaveWhere uses all the usable threads provide by your system. Decreasing Max Number of Threads will limit the number concurrent jobs that CaveWhere execute and will reduce CaveWhere's performance. If your computer is experiencing over heating issues from CaveWhere, reducing the Max Number of Threads may help."
            }

            Text {
                text: "Usable threads: " + settingsId.jobSettings.idleThreadCount
            }
        }
    }
}
