/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// A red dot on a source root's or trip's row saying that the owner's
// external centerline kept it out of the solve. The row's warnings belong
// to its ErrorIconBar.
//
// The attached-centerlines model answers per owner imperatively (errorFor
// is a Q_INVOKABLE read with no property NOTIFY), so this badge re-reads on
// the model's own row signals — the same shape ExternalCenterlineOwnerState
// uses for the manager.
QQ.Rectangle {
    id: root
    objectName: "externalSolveBadge"

    // A survey node or a Trip — both expose a QUuid id.
    property QQ.QtObject owner: null

    // Whether this owner is backed by an external centerline at all. The
    // predicate differs by kind, so the row that hosts the badge binds it.
    property bool externallyBacked: false

    // A trip's own account of why its stations are missing
    // (cwTrip::externalStationsError), shown when the model row carries
    // no error. A cave owner has no equivalent and leaves this empty.
    property string fallbackError: ""

    // Named for its role rather than `model`, which every delegate that
    // hosts this badge already has in scope as its row's data.
    readonly property AttachedCenterlinesModel attachedModel: RootData.externalCenterlineManager.attachedCenterlinesModel

    // The model row's answer for this owner, re-read on its signals.
    property string modelError: ""

    readonly property string reason: modelError !== "" ? modelError : fallbackError
    readonly property bool hasError: reason !== ""
    readonly property int dotSize: Theme.fontSizeCaption

    implicitWidth: dotSize
    implicitHeight: dotSize
    radius: dotSize / 2
    visible: externallyBacked && hasError
    color: Theme.danger

    QC.ToolTip.visible: hoverHandlerId.hovered
    QC.ToolTip.text: root.reason

    function refresh() {
        modelError = owner === null ? "" : attachedModel.errorFor(owner.id)
    }

    onOwnerChanged: refresh()

    QQ.Component.onCompleted: refresh()

    QQ.HoverHandler {
        id: hoverHandlerId
    }

    QQ.TapHandler {
        objectName: "externalSolveBadgeTap"
        onTapped: RootData.pageSelectionModel.gotoPageByName(null, "Cavern")
    }

    QQ.Connections {
        target: root.attachedModel

        function onModelReset() {
            root.refresh()
        }

        function onDataChanged() {
            root.refresh()
        }

        function onRowsInserted() {
            root.refresh()
        }

        function onRowsRemoved() {
            root.refresh()
        }
    }
}
