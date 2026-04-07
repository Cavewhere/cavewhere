import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib
import QQuickGit as QG

StandardPage {
    id: diffPage

    required property QG.GitRepository repository
    required property string commitSha
    required property int parentIndex
    required property string filePath
    required property string statusText
    property bool workingTree: false

    enum DisplayState {
        Loading,
        Error,
        Binary,
        Lfs,
        TooLarge,
        Diff
    }

    readonly property int displayState: {
        if (filePatch.loading) {
            return GitFileDiffPage.DisplayState.Loading;
        }
        if (filePatch.errorMessage.length > 0) {
            return GitFileDiffPage.DisplayState.Error;
        }
        if (filePatch.isBinary) {
            return GitFileDiffPage.DisplayState.Binary;
        }
        if (filePatch.isLfsPointer) {
            return GitFileDiffPage.DisplayState.Lfs;
        }
        if (filePatch.tooLarge) {
            return GitFileDiffPage.DisplayState.TooLarge;
        }
        return GitFileDiffPage.DisplayState.Diff;
    }

    QG.GitFilePatch {
        id: filePatch
        repository: diffPage.repository
        commitSha: diffPage.commitSha
        parentIndex: diffPage.parentIndex
        filePath: diffPage.filePath
        workingTree: diffPage.workingTree
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 4

            QC.Label {
                text: diffPage.filePath
                font.bold: true
                elide: QC.Label.ElideMiddle
                Layout.fillWidth: true
            }

            QQ.Rectangle {
                implicitWidth: statusLabel.implicitWidth + 12
                implicitHeight: statusLabel.implicitHeight + 4
                radius: 3
                color: Theme.surfaceMuted
                border.color: Theme.borderSubtle

                QC.Label {
                    id: statusLabel
                    anchors.centerIn: parent
                    text: diffPage.statusText
                    font.pointSize: 10
                    color: Theme.textSecondary
                }
            }
        }

        QC.Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: diffPage.displayState === GitFileDiffPage.DisplayState.Loading
            text: qsTr("Loading diff\u2026")
            color: Theme.textSubtle
            horizontalAlignment: QC.Label.AlignHCenter
            verticalAlignment: QC.Label.AlignVCenter
        }

        QC.Label {
            objectName: "errorLabel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: diffPage.displayState === GitFileDiffPage.DisplayState.Error
            text: filePatch.errorMessage
            color: Theme.danger
            horizontalAlignment: QC.Label.AlignHCenter
            verticalAlignment: QC.Label.AlignVCenter
            wrapMode: QC.Label.Wrap
        }

        QC.Label {
            objectName: "binaryLabel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: diffPage.displayState === GitFileDiffPage.DisplayState.Binary
            text: qsTr("Binary file")
            color: Theme.textSubtle
            horizontalAlignment: QC.Label.AlignHCenter
            verticalAlignment: QC.Label.AlignVCenter
        }

        QC.Label {
            objectName: "lfsLabel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: diffPage.displayState === GitFileDiffPage.DisplayState.Lfs
            text: qsTr("LFS file")
            color: Theme.textSubtle
            horizontalAlignment: QC.Label.AlignHCenter
            verticalAlignment: QC.Label.AlignVCenter
        }

        QC.Label {
            objectName: "tooLargeLabel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: diffPage.displayState === GitFileDiffPage.DisplayState.TooLarge
            text: qsTr("Diff too large to display")
            color: Theme.textSubtle
            horizontalAlignment: QC.Label.AlignHCenter
            verticalAlignment: QC.Label.AlignVCenter
        }

        QQ.ListView {
            id: diffListView
            objectName: "diffListView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: diffPage.displayState === GitFileDiffPage.DisplayState.Diff
            model: filePatch
            clip: true

            QC.ScrollBar.vertical: QC.ScrollBar { }

            delegate: QQ.Rectangle {
                id: lineDel
                width: diffListView.width
                implicitHeight: lineRow.implicitHeight
                color: {
                    switch (lineDel.origin) {
                    case "+": return Theme.diffAddedBackground;
                    case "-": return Theme.diffDeletedBackground;
                    case "H": return Theme.diffHunkBackground;
                    default:  return Theme.diffContextBackground;
                    }
                }

                required property string text
                required property string origin
                required property int oldLineNo
                required property int newLineNo
                required property int index

                RowLayout {
                    id: lineRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 0

                    QC.Label {
                        Layout.preferredWidth: 50
                        horizontalAlignment: QC.Label.AlignRight
                        rightPadding: 6
                        leftPadding: 4
                        text: lineDel.origin === "H" ? "" : (lineDel.oldLineNo >= 0 ? lineDel.oldLineNo : "")
                        font.family: "monospace"
                        font.pointSize: 11
                        color: Theme.textSubtle
                    }

                    QC.Label {
                        Layout.preferredWidth: 50
                        horizontalAlignment: QC.Label.AlignRight
                        rightPadding: 6
                        text: lineDel.origin === "H" ? "" : (lineDel.newLineNo >= 0 ? lineDel.newLineNo : "")
                        font.family: "monospace"
                        font.pointSize: 11
                        color: Theme.textSubtle
                    }

                    QQ.Rectangle {
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        color: Theme.borderSubtle
                    }

                    QC.Label {
                        Layout.fillWidth: true
                        leftPadding: 6
                        rightPadding: 4
                        text: lineDel.text
                        font.family: "monospace"
                        font.pointSize: 11
                        textFormat: QC.Label.PlainText
                        color: {
                            switch (lineDel.origin) {
                            case "+": return Theme.diffAddedText;
                            case "-": return Theme.diffDeletedText;
                            case "H": return Theme.diffHunkText;
                            default:  return Theme.text;
                            }
                        }
                    }
                }
            }
        }
    }
}
