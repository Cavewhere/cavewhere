import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: settingsId
    property JobSettings jobSettings: RootData.settings.jobSettings
    readonly property int memoryStepMb: 256
    readonly property int megabytesPerGigabyte: 1024

    function gigabytesText(megabytes: int) : string {
        return (megabytes / megabytesPerGigabyte).toFixed(1) + " GB";
    }

    QC.GroupBox {
        title: "Job Settings"

        ColumnLayout {
            RowLayout {
                InformationButton {
                    showItemOnClick: threadHelpAreaId
                }

                QC.Label {
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
                text: "By default, CaveWhere uses every usable thread except two, which it leaves free for the interface and the 3D view. Lowering Max Number of Threads runs fewer jobs at once, so they take longer to finish. If your computer runs hot while CaveWhere works, lowering Max Number of Threads may help."
            }

            QC.Label {
                text: "Usable threads: " + settingsId.jobSettings.idleThreadCount
            }

            QC.Label {
                objectName: "defaultThreadCountLabel"
                text: "Default: " + settingsId.jobSettings.defaultThreadCount + " of " + settingsId.jobSettings.idleThreadCount
            }

            RowLayout {
                InformationButton {
                    showItemOnClick: memoryHelpAreaId
                }

                QC.Label {
                    text: "Memory Budget"
                }

                QC.SpinBox {
                    id: memoryBudgetSpinBoxId
                    objectName: "memoryBudgetSpinBox"

                    from: 0
                    to: settingsId.jobSettings.totalMemoryMb
                    stepSize: settingsId.memoryStepMb
                    value: settingsId.jobSettings.memoryBudgetMb
                    textFromValue: function(value, locale) {
                        if (value === 0) {
                            return "Automatic";
                        }
                        return settingsId.gigabytesText(value);
                    }

                    onValueModified: {
                        settingsId.jobSettings.memoryBudgetMb = value;
                    }
                }

                QC.Label {
                    objectName: "memoryBudgetAutomaticLabel"

                    // SpinBox builds textFromValue before valueModified stores the value, so the cap lives in this binding
                    text: settingsId.jobSettings.memoryBudgetMb === 0
                          ? "(" + settingsId.gigabytesText(settingsId.jobSettings.resolvedMemoryBudgetMb) + ")"
                          : ""
                }
            }

            HelpArea {
                id: memoryHelpAreaId
                Layout.fillWidth: true
                text: "How much memory CaveWhere's background jobs may use for images and meshes at once. Automatic, the recommended setting, sizes it from the memory your device has free. Lower it if your computer slows down while CaveWhere processes notes; raise it to let more jobs run side by side."
            }
        }
    }

    RestoreDefaultsButton {
        settings: settingsId.jobSettings
    }
}
