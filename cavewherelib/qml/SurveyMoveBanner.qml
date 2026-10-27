/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// The banner above the Data page's tree while Move to… is armed. A move is
// armed from a menu and finished by clicking a row that can be screens away,
// so the tree says what it is waiting for while it waits — the shape of the
// survey editor's splay-move banner. A move that ties stations, joins names or
// carries fixes asks here first; the region's root, which owns no row, is
// offered as a button.
QQ.Rectangle {
    id: bannerId
    objectName: "surveyMoveBanner"

    //The node a question is waiting to land the move under, null standing for
    //the region's root.
    property QQ.QtObject pendingTarget: null

    //The question asked before the move lands, empty while the banner is
    //waiting for a click instead.
    property string question: ""

    readonly property bool confirming: bannerId.question !== ""

    //Lands the armed move under \a target, null for the region's root, or asks
    //first when the move does more than move. A row that cannot take the move
    //changes nothing.
    function requestMoveTo(target: QQ.QtObject) {
        const index = RegionSurveyTree.indexOf(target)
        if(!RegionSurveyTree.isMoveTarget(index)) {
            return
        }

        const question = RegionSurveyTree.moveConfirmation(index)
        if(question === "") {
            RegionSurveyTree.commitMove(index)
            return
        }

        bannerId.pendingTarget = target
        bannerId.question = question
    }

    function confirm() {
        const index = RegionSurveyTree.indexOf(bannerId.pendingTarget)
        bannerId.clearQuestion()
        RegionSurveyTree.commitMove(index)
    }

    function cancel() {
        bannerId.clearQuestion()
        RegionSurveyTree.cancelMove()
    }

    function clearQuestion() {
        bannerId.pendingTarget = null
        bannerId.question = ""
    }

    implicitHeight: contentRowId.implicitHeight + 2 * Theme.delegatePadding
    radius: Theme.bannerRadius
    color: Theme.highlight
    border.color: Theme.focusRing
    border.width: Theme.attentionBorderWidth

    //A new move, or none, has nothing to do with the question on screen.
    QQ.Connections {
        target: RegionSurveyTree

        function onMoveChanged() {
            bannerId.clearQuestion()
        }
    }

    RowLayout {
        id: contentRowId

        anchors.fill: parent
        anchors.margins: Theme.delegatePadding
        spacing: Theme.delegatePadding

        QC.Label {
            objectName: "surveyMoveBannerLabel"

            Layout.fillWidth: true

            text: bannerId.confirming
                  ? bannerId.question
                  : qsTr("Click where to move %1 — Esc cancels").arg(RegionSurveyTree.moveSubjectName)
            color: Theme.text
            wrapMode: QQ.Text.Wrap
        }

        QC.Button {
            objectName: "surveyMoveTopLevelButton"

            visible: !bannerId.confirming && RegionSurveyTree.moveRootIsTarget
            text: qsTr("Move to top level")

            onClicked: bannerId.requestMoveTo(null)
        }

        QC.Button {
            objectName: "surveyMoveConfirmButton"

            visible: bannerId.confirming
            text: qsTr("Move")

            onClicked: bannerId.confirm()
        }

        QC.Button {
            objectName: "surveyMoveCancelButton"

            text: qsTr("Cancel")

            onClicked: bannerId.cancel()
        }
    }
}
