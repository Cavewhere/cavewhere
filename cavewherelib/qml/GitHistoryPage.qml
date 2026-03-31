import QtQuick as QQ
import cavewherelib
import QQuickGit as QG

StandardPage {
    id: page
    objectName: "gitHistoryPage"

    QG.GitHistoryView {
        anchors.fill: parent
        repository: RootData.project.repository
        laneColors: Theme.laneColors
    }
}
