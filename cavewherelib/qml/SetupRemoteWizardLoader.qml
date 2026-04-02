import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
import QQuickGit

QQ.Item {
    id: rootId
    width: 0
    height: 0

    function open() {
        loaderId.active = true
        loaderId.item.open()
    }

    QQ.Loader {
        id: loaderId
        parent: QC.Overlay.overlay
        active: false
        sourceComponent: SetupRemoteWizard {
            anchors.centerIn: QC.Overlay.overlay
            gitHubIntegration: RootData.remote.gitHubIntegration
            accountCoordinator: RootData.remote.accountCoordinator
            repository: RootData.project.repository
            onRemoteSetupComplete: function(syncNow) {
                close()
                loaderId.active = false
                if (syncNow) { RootData.project.sync() }
            }
            onRemoteSetupCancelled: {
                close()
                loaderId.active = false
            }
        }
    }
}
