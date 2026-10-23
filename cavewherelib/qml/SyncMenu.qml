import QtQuick.Controls as QC
import cavewherelib

QC.Menu {
    id: syncMenuId

    required property bool hasRemote
    required property bool syncInProgress
    required property bool needsInstallation
    required property bool usesTokenAuth
    required property bool loggedIn

    signal syncRequested()
    signal remoteSettingsRequested()
    signal historyRequested()
    signal setupRemoteRequested()
    signal loginRequested()
    signal logoutRequested()
    signal installRequested()

    QC.MenuItem {
        objectName: "setupRemoteMenuItem"
        text: "Set up remote…"
        visible: !syncMenuId.hasRemote
        height: visible ? implicitHeight : 0
        onTriggered: syncMenuId.setupRemoteRequested()
    }

    QC.MenuItem {
        objectName: "syncNowMenuItem"
        text: "Sync now"
        visible: syncMenuId.hasRemote
        height: visible ? implicitHeight : 0
        enabled: !syncMenuId.syncInProgress
        onTriggered: syncMenuId.syncRequested()
    }

    QC.MenuItem {
        objectName: "installAppMenuItem"
        text: qsTr("Install CaveWhere on GitHub")
        visible: syncMenuId.hasRemote && syncMenuId.needsInstallation
        height: visible ? implicitHeight : 0
        onTriggered: syncMenuId.installRequested()
    }

    QC.MenuItem {
        objectName: "remoteSettingsMenuItem"
        text: "Remote settings…"
        onTriggered: syncMenuId.remoteSettingsRequested()
    }

    QC.MenuItem {
        objectName: "historyMenuItem"
        text: "History…"
        onTriggered: syncMenuId.historyRequested()
    }

    QC.MenuSeparator {
        objectName: "loginSeparator"
        visible: syncMenuId.hasRemote && syncMenuId.usesTokenAuth
        height: visible ? implicitHeight : 0
    }

    QC.MenuItem {
        objectName: "loginMenuItem"
        text: syncMenuId.loggedIn ? qsTr("Log out") : qsTr("Log in")
        visible: syncMenuId.hasRemote && syncMenuId.usesTokenAuth
        height: visible ? implicitHeight : 0
        onTriggered: {
            if (syncMenuId.loggedIn) {
                syncMenuId.logoutRequested()
            } else {
                syncMenuId.loginRequested()
            }
        }
    }
}
