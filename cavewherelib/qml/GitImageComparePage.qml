import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib
import QQuickGit as QG

StandardPage {
    id: page

    required property QG.GitRepository repository
    required property string commitSha
    required property int parentIndex
    required property string filePath
    property bool workingTree: false
    property int imageProviderRepoId: -1

    readonly property string _parentSha: commitInfo.parentShas.length > parentIndex
                                         ? commitInfo.parentShas[parentIndex] : ""
    readonly property bool _isAdded: _parentSha === ""

    // Divider position as a ratio (0.0–1.0) so it survives image/window resizes
    property real _dividerRatio: 0.5

    function _gitImageUrl(sha: string, path: string): string {
        return "image://gitcommit/%1/%2/%3".arg(page.imageProviderRepoId).arg(sha).arg(path)
    }

    readonly property string _beforeSource: {
        if (page._isAdded)
            return ""
        if (page.workingTree)
            return page._gitImageUrl(commitInfo.commitSha, page.filePath)
        return page._gitImageUrl(page._parentSha, page.filePath)
    }

    readonly property string _afterSource: {
        if (page.workingTree)
            return "file://" + page.repository.directoryPath + "/" + page.filePath
        return page._gitImageUrl(page.commitSha, page.filePath)
    }

    QG.GitCommitInfo {
        id: commitInfo
        repository: page.repository
        commitSha: page.workingTree ? "" : page.commitSha
        parentIndex: page.parentIndex
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.pageMargin
            spacing: 8

            QC.Label {
                Layout.fillWidth: true
                text: page.filePath
                font.pixelSize: 14
                font.weight: QQ.Font.DemiBold
                elide: QQ.Text.ElideLeft
            }

            QC.Label {
                text: qsTr("Before")
                font.pixelSize: 12
                color: Theme.textSubtle
            }

            QQ.Rectangle {
                width: 12
                height: 12
                radius: 2
                color: Theme.diffDeletedBackground
            }

            QC.Label {
                text: qsTr("After")
                font.pixelSize: 12
                color: Theme.textSubtle
            }

            QQ.Rectangle {
                width: 12
                height: 12
                radius: 2
                color: Theme.diffAddedBackground
            }
        }

        QQ.Item {
            id: compareArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            QQ.Image {
                id: afterImage
                anchors.centerIn: parent
                asynchronous: true

                readonly property real _scale: sourceSize.width > 0 && sourceSize.height > 0
                    ? Math.min(parent.width / sourceSize.width,
                               parent.height / sourceSize.height, 1.0)
                    : 1.0
                width: sourceSize.width * _scale
                height: sourceSize.height * _scale

                fillMode: QQ.Image.PreserveAspectFit
                source: page._afterSource
            }

            QQ.Item {
                id: beforeClip
                anchors.top: afterImage.top
                anchors.bottom: afterImage.bottom
                x: afterImage.x
                width: dividerHandle.x - afterImage.x
                clip: true
                visible: !page._isAdded

                QQ.Image {
                    id: beforeImage
                    x: 0
                    y: 0
                    width: afterImage.width
                    height: afterImage.height
                    asynchronous: true
                    fillMode: QQ.Image.PreserveAspectFit
                    source: page._beforeSource
                }
            }

            QQ.Rectangle {
                id: dividerHandle
                x: afterImage.x + afterImage.width * page._dividerRatio - width * 0.5
                anchors.top: afterImage.top
                anchors.bottom: afterImage.bottom
                width: 3
                color: Theme.text
                opacity: 0.8
                visible: !page._isAdded

                QQ.Rectangle {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    radius: 12
                    color: Theme.surface
                    border.color: Theme.border
                    border.width: 1

                    QC.Label {
                        anchors.centerIn: parent
                        text: "\u2194"
                        font.pixelSize: 14
                        color: Theme.text
                    }
                }

                QQ.MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.SplitHCursor
                    drag.target: dividerHandle
                    drag.axis: QQ.Drag.XAxis
                    drag.minimumX: afterImage.x
                    drag.maximumX: afterImage.x + afterImage.width - dividerHandle.width

                    onPositionChanged: {
                        if (drag.active && afterImage.width > 0) {
                            page._dividerRatio = (dividerHandle.x + dividerHandle.width * 0.5 - afterImage.x)
                                                 / afterImage.width
                        }
                    }
                }
            }

            QC.Label {
                anchors.centerIn: parent
                text: qsTr("No previous version")
                visible: page._isAdded
                font.pixelSize: 14
                color: Theme.textSubtle
            }
        }
    }
}
