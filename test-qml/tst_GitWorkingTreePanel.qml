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

    SignalSpy {
        id: commitRequestedSpy
        target: rootId.currentPanel
        signalName: "commitRequested"
    }

    TestCase {
        name: "GitWorkingTreePanel"
        when: windowShown

        function cleanup() {
            if (rootId.currentPanel) {
                rootId.currentPanel.destroy()
                rootId.currentPanel = null
            }
            commitRequestedSpy.clear()
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

        function test_commitClearsFields() {
            let panel = rootId.createPanel()
            verify(panel !== null)

            let subjectField = findChild(panel, "subjectField")
            let descriptionField = findChild(panel, "descriptionField")
            let commitButton = findChild(panel, "commitButton")
            verify(subjectField !== null, "subjectField not found")
            verify(descriptionField !== null, "descriptionField not found")
            verify(commitButton !== null, "commitButton not found")

            subjectField.text = "  Fix the thing  "
            descriptionField.text = "  more detail  "

            // Button is disabled without uncommitted files; emit clicked directly
            // to exercise the onClicked handler.
            commitButton.clicked()

            compare(commitRequestedSpy.count, 1, "commitRequested should fire once")
            compare(commitRequestedSpy.signalArguments[0][0], "Fix the thing")
            compare(commitRequestedSpy.signalArguments[0][1], "more detail")
            compare(subjectField.text, "", "subjectField should be cleared after commit")
            compare(descriptionField.text, "", "descriptionField should be cleared after commit")
        }
    }
}
