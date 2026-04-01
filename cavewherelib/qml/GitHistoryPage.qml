import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
import QQuickGit as QG

StandardPage {
    id: page
    objectName: "gitHistoryPage"

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
        }
    }
}
