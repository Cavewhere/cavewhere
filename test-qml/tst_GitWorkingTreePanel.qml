import QtQuick
import QtTest
import QQuickGit as QG

Item {
    id: rootId
    width: 800
    height: 600

    QG.GitRepository {
        id: testRepo
    }

    Component {
        id: panelComponent
        QG.GitWorkingTreePanel {
            anchors.fill: parent
        }
    }

    property var currentPanel: null

    function createPanel(props) {
        if (currentPanel) {
            currentPanel.destroy()
            currentPanel = null
        }

        let defaults = {
            repository: testRepo
        }

        let merged = Object.assign(defaults, props || {})
        currentPanel = panelComponent.createObject(rootId, merged)
        return currentPanel
    }

    TestCase {
        name: "GitWorkingTreePanel"
        when: windowShown

        function cleanup() {
            if (rootId.currentPanel) {
                rootId.currentPanel.destroy()
                rootId.currentPanel = null
            }
        }

        function test_directoryPathNotUndefined() {
            // Verify that repository.directoryPath is accessible from QML.
            // QDir doesn't expose path as a Q_PROPERTY, so repository.directory.path
            // returns undefined. We need a dedicated directoryPath string property.
            var dirPath = testRepo.directoryPath
            verify(dirPath !== undefined, "directoryPath should not be undefined, got: " + dirPath)
        }

        function test_panelCreates() {
            let panel = rootId.createPanel()
            verify(panel !== null)
        }
    }
}
