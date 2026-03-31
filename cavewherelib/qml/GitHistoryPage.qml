import QtQuick as QQ
import cavewherelib
import QQuickGit as QG

StandardPage {
    id: page
    objectName: "gitHistoryPage"

    QG.GitHistoryView {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        repository: RootData.project.repository
        laneColors: Theme.laneColors
        scrollBarSpacing: Theme.pageMargin
    }
}
