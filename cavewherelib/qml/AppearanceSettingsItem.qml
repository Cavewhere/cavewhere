import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    spacing: 20

    // ── Font Family ─────────────────────────────────────────────────────────

    QC.GroupBox {
        title: "Font"

        QC.ButtonGroup { id: fontFamilyGroup }

        RowLayout {
            spacing: 12

            QQ.Repeater {
                model: RootData.settings.fontSettings.fontEntries

                delegate: QQ.Rectangle {
                    required property var modelData

                    implicitWidth: 110
                    implicitHeight: delegateFontLayout.implicitHeight + 24
                    radius: 6
                    color: Theme.surface
                    border.width: familyBtn.checked ? 2 : 1
                    border.color: familyBtn.checked ? Theme.accent : Theme.border

                    ColumnLayout {
                        id: delegateFontLayout
                        anchors.centerIn: parent
                        spacing: 8

                        QC.Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Aa"
                            font.family: modelData.family !== "" ? modelData.family : Qt.application.font.family
                            font.pixelSize: Theme.fontSizeTitle  // intentionally not scaled — always show at a readable preview size
                        }

                        QC.RadioButton {
                            id: familyBtn
                            objectName: modelData.label.replace(/ /g, "") + "RadioButton"
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.label
                            checked: RootData.settings.fontSettings.fontFamily === modelData.family
                            QC.ButtonGroup.group: fontFamilyGroup
                            onClicked: RootData.settings.fontSettings.fontFamily = modelData.family
                        }
                    }
                }
            }
        }
    }

    // ── Font Size ────────────────────────────────────────────────────────────

    QC.GroupBox {
        title: "Size"

        RowLayout {
            spacing: 12

            // Smaller card
            QQ.Rectangle {
                implicitWidth: largerCard.implicitWidth
                implicitHeight: largerCard.implicitHeight
                radius: 6
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: sizeLayout0
                    anchors.centerIn: parent
                    spacing: 8

                    QC.Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Aa"
                        font.family: RootData.settings.fontSettings.fontFamily
                        font.pixelSize: Math.max(RootData.settings.fontSettings.minFontBaseSize, RootData.settings.fontSettings.fontBaseSize - 2)
                    }

                    QC.Button {
                        objectName: "smallerButton"
                        Layout.alignment: Qt.AlignHCenter
                        text: "Smaller"
                        enabled: RootData.settings.fontSettings.fontBaseSize > RootData.settings.fontSettings.minFontBaseSize
                        onClicked: RootData.settings.fontSettings.fontBaseSize -= 2
                    }
                }
            }

            // Default card — border highlights when size is at default
            QQ.Rectangle {
                id: defaultCard
                objectName: "defaultCard"
                readonly property bool active: RootData.settings.fontSettings.fontBaseSize === RootData.settings.fontSettings.defaultFontBaseSize

                implicitWidth: largerCard.implicitWidth
                implicitHeight: largerCard.implicitHeight
                radius: 6
                color: Theme.surface
                border.width: active ? 2 : 1
                border.color: active ? Theme.accent : Theme.border

                ColumnLayout {
                    id: sizeLayout1
                    anchors.centerIn: parent
                    spacing: 8

                    QC.Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Aa"
                        font.family: RootData.settings.fontSettings.fontFamily
                        font.pixelSize: RootData.settings.fontSettings.defaultFontBaseSize
                    }

                    QC.Button {
                        objectName: "defaultButton"
                        Layout.alignment: Qt.AlignHCenter
                        text: "Default"
                        enabled: !defaultCard.active
                        onClicked: RootData.settings.fontSettings.fontBaseSize = RootData.settings.fontSettings.defaultFontBaseSize
                    }
                }
            }

            // Larger card
            QQ.Rectangle {
                id: largerCard
                implicitWidth: sizeLayout2.implicitWidth + 24
                implicitHeight: sizeLayout2.implicitHeight + 24
                radius: 6
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: sizeLayout2
                    anchors.centerIn: parent
                    spacing: 8

                    QC.Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Aa"
                        font.family: RootData.settings.fontSettings.fontFamily
                        font.pixelSize: Math.min(RootData.settings.fontSettings.maxFontBaseSize, RootData.settings.fontSettings.fontBaseSize + 2)
                    }

                    QC.Button {
                        objectName: "largerButton"
                        Layout.alignment: Qt.AlignHCenter
                        text: "Larger"
                        enabled: RootData.settings.fontSettings.fontBaseSize < RootData.settings.fontSettings.maxFontBaseSize
                        onClicked: RootData.settings.fontSettings.fontBaseSize += 2
                    }
                }
            }

            QC.Label {
                text: RootData.settings.fontSettings.fontBaseSize + "px"
                color: Theme.textSubtle
            }
        }
    }

    // ── Preview ──────────────────────────────────────────────────────────────

    QC.GroupBox {
        title: "Preview"
        Layout.fillWidth: true

        ColumnLayout {
            id: previewLayout
            width: parent.width
            spacing: 6

            QC.Label { text: "Section Heading"; font.bold: true; font.pixelSize: Theme.fontSizeTitle }
            QC.Label { text: "Standard UI text — buttons, labels, menus" }
            BodyText { text: "Body copy — longer descriptions and help text." }
            QC.Label { text: "Small / secondary text"; font.pixelSize: Theme.fontSizeSmall; color: Theme.textSubtle }
        }
    }

    RestoreDefaultsButton {
        settings: RootData.settings.fontSettings
    }

    QQ.Item { Layout.fillHeight: true }
}
