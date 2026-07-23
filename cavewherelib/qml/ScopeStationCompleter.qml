/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

/**
  Station-name autocomplete for the shared shadow editor: a dropdown of
  in-scope stations driven by a trip-scoped cwScopeStationListModel. Inert
  unless both textInput and scopeModel are set, so every non-station field
  using the shadow editor is unaffected.

  This item does not live inside GlobalShadowTextInput — that stays a generic
  editor. A single instance is placed in the main overlay, tracks the shared
  editor's geometry, and reads its scope model from the field currently being
  edited (GlobalShadowTextInput.coreClickInput.stationScopeModel).

  Interaction: type to filter; the dropdown lists matches; Tab fills the field
  with the top (or highlighted) match; Up/Down live-preview a match into the
  field (Enter then commits it); click a row to pick and commit. A typed name
  that is in no in-scope station raises a non-blocking advisory — it never
  blocks committing, matching the permissive cwStationValidator.
 */
QQ.Item {
    id: completer

    property GlobalTextInputHelper textInput
    property ScopeStationListModel scopeModel

    readonly property Trip trip: scopeModel !== null ? scopeModel.trip : null
    readonly property bool active: textInput !== null && scopeModel !== null && textInput.visible
    readonly property string currentText: textInput !== null ? textInput.text : ""

    // Completion list is keyed on what the USER typed, held stable while Up/Down
    // navigate (which write a full match into the field). typedText mirrors the
    // field only on a genuine edit — see the onTextChanged guard below.
    property string typedText: ""
    // -1 = showing the typed text; >= 0 = a highlighted match written live.
    property int highlightIndex: -1
    property bool applyingInternally: false

    readonly property list<string> matches: active ? scopeModel.matchingStations(typedText) : []
    // True when Tab would extend the typed text (the top match is longer than
    // what's typed, and nothing is already highlighted). Gates the Tab handler
    // so a fully-typed name lets Tab fall through to field navigation.
    readonly property bool hasSuggestion: active && highlightIndex < 0
                                          && matches.length > 0
                                          && matches[0].length > typedText.length

    // Typed a non-empty name that matches nothing and is not even a known
    // station in this scope: the non-blocking "not in this trip" advisory.
    readonly property bool outOfScope: active && typedText.length > 0
                                       && matches.length === 0
                                       && !scopeModel.containsStation(typedText)

    readonly property bool dropdownVisible: active && (matches.length > 0 || outOfScope)

    signal pickCommitted()

    function resetForNewEdit() {
        highlightIndex = -1
        typedText = currentText
    }

    function applyName(name) {
        // name can be undefined if highlightIndex outran a matches list that
        // shrank underneath us (e.g. a background re-solve mid-navigation) —
        // never write that into the field.
        if (textInput === null || name === undefined) {
            return
        }
        applyingInternally = true
        textInput.text = name
        applyingInternally = false
    }

    // Tab: fill the field with the highlighted row, or the top match. Leaves
    // the editor open (Enter or a click commits) — unlike pickRow.
    function acceptTop() {
        if (matches.length === 0) {
            return
        }
        const index = highlightIndex >= 0 ? highlightIndex : 0
        applyName(matches[index])
        typedText = matches[index]
        highlightIndex = -1
    }

    function moveDown() {
        if (matches.length === 0) {
            return
        }
        highlightIndex = Math.min(highlightIndex + 1, matches.length - 1)
        applyName(matches[highlightIndex])
    }

    function moveUp() {
        if (matches.length === 0) {
            return
        }
        if (highlightIndex <= 0) {
            highlightIndex = -1
            applyName(typedText)
        } else {
            highlightIndex = highlightIndex - 1
            applyName(matches[highlightIndex])
        }
    }

    function pickRow(index) {
        if (index < 0 || index >= matches.length) {
            return
        }
        highlightIndex = index
        applyName(matches[index])
        completer.pickCommitted()
    }

    onActiveChanged: {
        if (active) {
            resetForNewEdit()
        }
    }

    // If the match list shrinks underneath a live highlight (a re-solve mid-edit),
    // pull the highlight back into range so navigation never indexes past the end.
    onMatchesChanged: {
        if (highlightIndex >= matches.length) {
            highlightIndex = matches.length - 1
        }
    }

    QQ.Connections {
        target: completer.textInput

        function onTextChanged() {
            // A genuine user edit (not our own applyName) resets navigation and
            // re-seeds the completion prefix.
            if (!completer.applyingInternally) {
                completer.highlightIndex = -1
                completer.typedText = completer.textInput.text
            }
        }

        function onPressKeyPressed() {
            if (!completer.active) {
                return
            }
            const event = completer.textInput.pressKeyEvent
            if (event.key === Qt.Key_Down) {
                completer.moveDown()
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                completer.moveUp()
                event.accepted = true
            } else if (event.key === Qt.Key_Tab
                       && (completer.highlightIndex >= 0 || completer.hasSuggestion)) {
                completer.acceptTop()
                event.accepted = true
            }
        }
    }

    // Dropdown of in-scope matches, plus the out-of-scope advisory, below the field.
    QQ.Rectangle {
        id: dropdown

        anchors.top: parent.bottom
        anchors.left: parent.left
        width: Math.max(parent.width, Theme.fontSizeBody * 12)
        height: dropdownColumn.implicitHeight + Theme.tightSpacing * 2
        visible: completer.dropdownVisible
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        radius: Theme.floatingWidgetRadius

        QQ.Column {
            id: dropdownColumn

            anchors.fill: parent
            anchors.margins: Theme.tightSpacing

            QQ.Repeater {
                id: rowRepeater
                model: completer.matches

                QQ.Item {
                    id: rowRect

                    required property int index
                    required property string modelData

                    width: dropdownColumn.width
                    height: rowLabel.implicitHeight + Theme.delegatePadding

                    // Painted only when highlighted or hovered — no transparent
                    // geometry node for the resting rows.
                    QQ.Rectangle {
                        anchors.fill: parent
                        radius: Theme.floatingWidgetRadius
                        visible: rowRect.index === completer.highlightIndex || rowHover.hovered
                        color: rowRect.index === completer.highlightIndex ? Theme.highlight : Theme.hover
                    }

                    QC.Label {
                        id: rowLabel
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.delegatePadding
                        anchors.verticalCenter: parent.verticalCenter
                        text: rowRect.modelData
                        color: Theme.text
                        font.family: Theme.fontFamilyMono
                        font.pixelSize: Theme.fontSizeBody
                    }

                    QQ.HoverHandler {
                        id: rowHover
                    }

                    QQ.TapHandler {
                        onTapped: completer.pickRow(rowRect.index)
                    }
                }
            }

            QC.Label {
                id: advisoryLabel
                width: dropdownColumn.width
                visible: completer.outOfScope
                text: completer.trip !== null && completer.trip.parentCave !== null
                      ? qsTr("“%1” isn't in this trip — tie into %2?")
                        .arg(completer.typedText).arg(completer.trip.parentCave.name)
                      : qsTr("“%1” isn't in this trip").arg(completer.typedText)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: QC.Label.WordWrap
                padding: Theme.delegatePadding
            }
        }
    }
}
