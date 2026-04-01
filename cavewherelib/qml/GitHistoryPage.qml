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
        highlightColor: Theme.highlight
        syntheticBackground: Theme.surfaceMuted
        syntheticBorderColor: Theme.border
        syntheticIconSource: "qrc:/twbs-icons/icons/pencil.svg"
        scrollBarSpacing: Theme.pageMargin
    }
}
