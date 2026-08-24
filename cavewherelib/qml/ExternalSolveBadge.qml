/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// A single dot on a cave or trip row saying that the owner's external
// centerline kept it out of the solve (danger) or solved with warnings
// (warning). Hidden for a clean owner and for an owner with no external
// centerline at all, so a native row's layout is untouched.
//
// The attached-centerlines model answers per owner imperatively
// (errorFor / warningCountFor are Q_INVOKABLE reads with no property
// NOTIFY), so this badge re-reads on the model's own row signals — the
// same shape ExternalCenterlineOwnerState uses for the manager.
QQ.Rectangle {
    id: root
    objectName: "externalSolveBadge"

    // A Cave or a Trip — both expose a QUuid id.
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

    // The model row's answers for this owner, re-read on its signals.
    property string modelError: ""
    property int warningCount: 0

    readonly property string reason: modelError !== "" ? modelError : fallbackError
    readonly property bool hasError: reason !== ""
    readonly property int dotSize: Theme.fontSizeCaption

    implicitWidth: dotSize
    implicitHeight: dotSize
    radius: dotSize / 2
    visible: externallyBacked && (hasError || warningCount > 0)
    color: hasError ? Theme.danger : Theme.warning

    QC.ToolTip.visible: hoverHandlerId.hovered
    QC.ToolTip.text: root.hasError
                     ? root.reason
                     : root.warningCount === 1
                       ? qsTr("Solved with 1 warning")
                       : qsTr("Solved with %1 warnings").arg(root.warningCount)

    function refresh() {
        modelError = owner === null ? "" : attachedModel.errorFor(owner.id)
        warningCount = owner === null ? 0 : attachedModel.warningCountFor(owner.id)
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
