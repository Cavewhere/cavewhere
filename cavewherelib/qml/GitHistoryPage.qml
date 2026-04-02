import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
import QQuickGit as QG

StandardPage {
    id: page
    objectName: "gitHistoryPage"

    function navigateToFileDiff(filePath: string, isBinary: bool, isImage: bool,
                                statusText: string, isWorkingTree: bool): void {
        if (isImage) {
            var imgPage = RootData.pageSelectionModel.registerPage(
                page.PageView.page, "Diff",
                imageComparePageComponent,
                {
                    repository: RootData.project.repository,
                    commitSha: historyView.selectedSha,
                    parentIndex: 0,
                    filePath: filePath,
                    statusText: statusText,
                    workingTree: isWorkingTree
                }
            )
            RootData.pageSelectionModel.gotoPage(imgPage)
        } else if (!isBinary) {
            var diffPage = RootData.pageSelectionModel.registerPage(
                page.PageView.page, "Diff",
                diffPageComponent,
                {
                    repository: RootData.project.repository,
                    commitSha: historyView.selectedSha,
                    parentIndex: 0,
                    filePath: filePath,
                    statusText: statusText,
                    workingTree: isWorkingTree
                }
            )
            RootData.pageSelectionModel.gotoPage(diffPage)
        }
    }

    onVisibleChanged: {
        if (visible) {
            RootData.project.repository.checkStatusAsync()
        }
    }

    QQ.Connections {
        target: RootData.project
        enabled: page.visible
        function onSaveFlushCompleted() {
            RootData.project.repository.checkStatusAsync()
        }
    }

    QC.SplitView {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        orientation: Qt.Horizontal

        QQ.Item {
            QC.SplitView.preferredWidth: 400
            QC.SplitView.minimumWidth: 200

            QG.GitHistoryView {
                id: historyView
                anchors.fill: parent
                anchors.rightMargin: Theme.pageMargin
                repository: RootData.project.repository
                laneColors: Theme.laneColors
                highlightColor: Theme.highlight
                syntheticBackground: Theme.surfaceMuted
                syntheticBorderColor: Theme.border
                syntheticIconSource: "qrc:/twbs-icons/icons/pencil.svg"
                scrollBarSpacing: Theme.pageMargin
            }
        }

        QQ.Item {
            QC.SplitView.fillWidth: true
            QC.SplitView.minimumWidth: 200

            QQ.Loader {
                id: detailLoader
                anchors.fill: parent
                anchors.leftMargin: Theme.pageMargin

                sourceComponent: historyView.selectedIsUncommitted
                                 ? workingTreeComponent : commitDetailComponent
            }
        }
    }

    QQ.Component {
        id: commitDetailComponent

        QG.GitCommitDetailPanel {
            repository: RootData.project.repository
            commitSha: historyView.selectedSha
            addedColor: Theme.diffAddedText
            deletedColor: Theme.diffDeletedText
            modifiedColor: Theme.warning
            renamedColor: Theme.info
            errorColor: Theme.danger
            errorBackground: Theme.diffDeletedBackground

            onFileClicked: (filePath, isBinary, isImage, statusText) => {
                page.navigateToFileDiff(filePath, isBinary, isImage, statusText, false)
            }
        }
    }

    QQ.Component {
        id: workingTreeComponent

        QG.GitWorkingTreePanel {
            repository: RootData.project.repository
            addedColor: Theme.diffAddedText
            deletedColor: Theme.diffDeletedText
            modifiedColor: Theme.warning
            untrackedColor: Theme.textSubtle

            onFileClicked: (filePath, isBinary, isImage, statusText) => {
                page.navigateToFileDiff(filePath, isBinary, isImage, statusText, true)
            }

            onCommitRequested: (subject, description) => {
                RootData.project.safeCommitAll(subject, description)
            }
        }
    }

    QQ.Component {
        id: diffPageComponent
        GitFileDiffPage { }
    }

    QQ.Component {
        id: imageComparePageComponent
        GitImageComparePage { }
    }
}
