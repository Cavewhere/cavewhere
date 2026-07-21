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
    required property string statusText
    property bool workingTree: false

    readonly property string _parentSha: commitInfo.parentShas.length > parentIndex
                                         ? commitInfo.parentShas[parentIndex] : ""
    readonly property bool _isAdded: page.statusText === "Added"
    readonly property bool _isDeleted: page.statusText === "Deleted"

    // Divider position as a ratio (0.0–1.0) so it survives image/window resizes
    property real _dividerRatio: 0.5

    function _gitImageUrl(sha: string, path: string): string {
        return "image://gitcommit/%1/%2/%3".arg(page.repository.imageProviderId).arg(sha).arg(path)
    }

    readonly property string _beforeSource: {
        if (page._isAdded || page._parentSha === "")
            return ""
        if (page.workingTree)
            return page._gitImageUrl(commitInfo.commitSha, page.filePath)
        return page._gitImageUrl(page._parentSha, page.filePath)
    }

    readonly property string _afterSource: {
        if (page._isDeleted)
            return ""
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
                font.pixelSize: Theme.fontSizeBody
                font.weight: QQ.Font.DemiBold
                elide: QC.Label.ElideLeft
            }

            QC.Label {
                objectName: "beforeLabel"
                text: qsTr("Before")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.textSubtle
                visible: !page._isAdded
            }

            QQ.Rectangle {
                width: 12
                height: 12
                radius: 2
                color: Theme.diffDeletedBackground
                visible: !page._isAdded
            }

            QC.Label {
                objectName: "afterLabel"
                text: qsTr("After")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.textSubtle
                visible: !page._isDeleted
            }

            QQ.Rectangle {
                width: 12
                height: 12
                radius: 2
                color: Theme.diffAddedBackground
                visible: !page._isDeleted
            }
        }

        QQ.Item {
            id: compareArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            QQ.Image {
                id: primaryImage
                objectName: "primaryImage"
                anchors.centerIn: parent
                asynchronous: true

                readonly property bool _failed: status === QQ.Image.Error
                                                || status === QQ.Image.Null
                readonly property real _scale: sourceSize.width > 0 && sourceSize.height > 0
                    ? Math.min(parent.width / sourceSize.width,
                               parent.height / sourceSize.height, 1.0)
                    : 1.0
                width: _failed ? parent.width * 0.8 : sourceSize.width * _scale
                height: _failed ? parent.height * 0.8 : sourceSize.height * _scale

                fillMode: QQ.Image.PreserveAspectFit
                source: page._isDeleted ? page._beforeSource : page._afterSource
            }

            // Shown when the primary image (after for Added, before for Deleted) fails
            QQ.Rectangle {
                anchors.fill: primaryImage
                visible: primaryImage._failed
                color: Theme.surfaceMuted

                QC.Label {
                    anchors.centerIn: parent
                    text: page._isDeleted
                          ? qsTr("Previous version unavailable")
                          : qsTr("Current version unavailable")
                    font.pixelSize: Theme.fontSizeBody
                    color: Theme.textSubtle
                }
            }

            QQ.Item {
                id: beforeClip
                objectName: "beforeClip"
                anchors.top: primaryImage.top
                anchors.bottom: primaryImage.bottom
                x: primaryImage.x
                width: dividerHandle.x - primaryImage.x
                clip: true

                QQ.Image {
                    id: beforeImage
                    objectName: "beforeImage"
                    width: primaryImage.width
                    height: primaryImage.height
                    asynchronous: true
                    fillMode: QQ.Image.PreserveAspectFit
                    source: page._beforeSource
                    visible: !page._isAdded && status === QQ.Image.Ready
                }

                QQ.Rectangle {
                    width: primaryImage.width
                    height: primaryImage.height
                    visible: page._isAdded
                             || (beforeImage.status === QQ.Image.Error
                                 && !page._isDeleted)
                    color: Theme.surfaceMuted

                    QC.Label {
                        anchors.centerIn: parent
                        text: page._isAdded
                              ? qsTr("No previous version")
                              : qsTr("Previous version unavailable")
                        font.pixelSize: Theme.fontSizeBody
                        color: Theme.textSubtle
                    }
                }
            }

            QQ.Item {
                id: afterClip
                objectName: "afterClip"
                anchors.top: primaryImage.top
                anchors.bottom: primaryImage.bottom
                x: dividerHandle.x + dividerHandle.width
                width: (primaryImage.x + primaryImage.width) - x
                clip: true
                visible: page._isDeleted

                QQ.Rectangle {
                    x: primaryImage.x - afterClip.x
                    width: primaryImage.width
                    height: primaryImage.height
                    color: Theme.surfaceMuted

                    QC.Label {
                        anchors.centerIn: parent
                        text: qsTr("File was deleted")
                        font.pixelSize: Theme.fontSizeBody
                        color: Theme.textSubtle
                    }
                }
            }

            QQ.Rectangle {
                id: dividerHandle
                objectName: "dividerHandle"
                x: primaryImage.x + primaryImage.width * page._dividerRatio - width * 0.5
                anchors.top: primaryImage.top
                anchors.bottom: primaryImage.bottom
                width: 3
                color: Theme.text
                opacity: 0.8

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
                        font.pixelSize: Theme.fontSizeBody
                    }
                }

                QQ.MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.SplitHCursor
                    drag.target: dividerHandle
                    drag.axis: QQ.Drag.XAxis
                    drag.minimumX: primaryImage.x
                    drag.maximumX: primaryImage.x + primaryImage.width - dividerHandle.width

                    onPositionChanged: {
                        if (drag.active && primaryImage.width > 0) {
                            page._dividerRatio = (dividerHandle.x + dividerHandle.width * 0.5 - primaryImage.x)
                                                 / primaryImage.width
                        }
                    }
                }
            }
        }
    }
}
