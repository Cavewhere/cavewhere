/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// Everything cwExternalCenterlineManager knows about one attachment
// owner — a Trip or a Cave, both of which expose id — handed to the
// hosting panel or card as plain properties.
//
// The manager answers every one of these imperatively (isOwnerBusy,
// attachmentDir, missingCopyPath, fileOwnsDeclination and the status
// model's statusFor are Q_INVOKABLE reads with no property NOTIFY), so
// this object re-reads each answer when the signal that can move it
// arrives, and re-reads all of them when the owner changes.
QQ.QtObject {
    id: root

    property QQ.QtObject owner: null
    property ExternalCenterlineManager manager: RootData.externalCenterlineManager

    // True while an attach, replace, reload or detach for this owner is
    // still in flight; hosts disable their actions on it.
    property bool ownerBusy: false

    // Absolute on-disk path of the owner's entry file: the attachment
    // directory the manager holds, joined with the project-relative
    // entryFile. An attach that fills it lands with solveNeeded.
    property string entryFilePath: ""

    // Whether the file this owner's copy came from has moved on since the
    // copy was made.
    property bool sourceChangedSinceCopy: false

    // Project-relative path of the owner's in-project copy while that file
    // is gone from disk; empty otherwise.
    property string missingCopyPath: ""

    // Defaults to true (file-owned) until the manager's scan says
    // otherwise; the scan apply emits solveNeeded whenever the
    // declination flags change.
    property bool fileOwnsDeclination: true

    function updateOwnerBusy() {
        ownerBusy = owner !== null && manager.isOwnerBusy(owner.id)
    }

    function updateEntryFilePath() {
        entryFilePath = owner === null
                ? "" : FileRevealer.resolvedPath(manager.attachmentDir(owner.id),
                                                 owner.externalCenterline.entryFile)
    }

    function updateSourceChangedSinceCopy() {
        sourceChangedSinceCopy = owner !== null
                && manager.sourceStatusModel.statusFor(owner.id)
                   === ExternalSourceStatusModel.Changed
    }

    function updateMissingCopyPath() {
        missingCopyPath = owner === null ? "" : manager.missingCopyPath(owner.id)
    }

    function updateFileOwnsDeclination() {
        fileOwnsDeclination = owner === null || manager.fileOwnsDeclination(owner.id)
    }

    // Every answer read in one go — for an owner that just arrived, and
    // for the host's first frame.
    function refresh() {
        updateOwnerBusy()
        updateFileOwnsDeclination()
        updateMissingCopyPath()
        updateEntryFilePath()
        updateSourceChangedSinceCopy()
    }

    onOwnerChanged: refresh()

    QQ.Component.onCompleted: refresh()

    readonly property QQ.Connections statusConnections: QQ.Connections {
        target: root.manager.sourceStatusModel

        function onStatusesChanged() {
            root.updateSourceChangedSinceCopy()
        }
    }

    readonly property QQ.Connections managerConnections: QQ.Connections {
        target: root.manager

        function onOwnerBusyChanged(ownerId) {
            root.updateOwnerBusy()
        }

        function onSolveNeeded() {
            root.updateFileOwnsDeclination()
            root.updateEntryFilePath()
        }

        function onMissingCopiesChanged() {
            root.updateMissingCopyPath()
        }
    }
}
