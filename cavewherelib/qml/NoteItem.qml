/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

ImageItem {
    id: noteArea

    objectName: "noteArea"

    property Note note;
    //Set by NotesGallery's CARPET state. Named the same on NoteLiDARItem so
    //the gallery sets one property on every note editor it drives.
    property bool editingOverlaysVisible: false

    //The gallery's tool state, pulled rather than pushed. Modes this editor
    //declares no State for are torn down by the catch-all transition below —
    //QML keeps the unknown name rather than reverting to the base state, so
    //without it the previous tool would stay active.
    property string toolMode: NoteToolMode.none
    property bool isNarrow: false
    property alias scrapCount: scrapViewId.count

    readonly property list<ScrapPointView> _pointViews: {
        const s = scrapViewId.selectedScrapItem
        if(s === null) return []
        return [s.outlinePointView, s.stationView, s.leadView]
    }

    readonly property bool hasSelection: {
        for(let i = 0; i < _pointViews.length; i++) {
            if(_pointViews[i] !== null && _pointViews[i].hasSelection) return true
        }
        return false
    }

    function deleteSelected() {
        for(let i = 0; i < _pointViews.length; i++) {
            if(_pointViews[i] !== null) _pointViews[i].removeSelectedItem()
        }
    }

    imageRotation: note ? note.rotate : 0
    image.source: note ? RootData.cavewhereImageUrl(note.image, RootData.project.absolutePath(note)) : ""

    clip: true

    state: toolMode

    onVisibleChanged: {
        if(!visible) {
            interactionManagerId.active(null)
        } else {
            interactionManagerId.active(interactionManagerId.defaultInteraction)
        }
    }

    PanZoomInteraction {
        id: panZoomInteraction
        target: noteArea.targetItem
    }

    //We need this container to mimic the image.
    //Scrap points are added to this container, like outline points, stations, leads
    //They need to be hidden but the image still needs to be shown
    QQ.Item {
        id: scrapPointsContainer
        x: noteArea.targetItem.x
        y: noteArea.targetItem.y
        scale: noteArea.targetItem.scale
        rotation: noteArea.targetItem.rotation
        width: noteArea.targetItem.width
        height: noteArea.targetItem.height
        visible: noteArea.editingOverlaysVisible
    }

    ImageBusyIndicator {
        image: noteArea.image
    }

    ScrapInteraction {
        id: addScrapInteraction
        anchors.fill: parent
        target: noteArea.targetItem
        note: noteArea.note
        scrapView: scrapViewId
        zoom: noteArea.targetItem.scale
        outlinePointView: {
            if(scrapViewId.selectedScrapItem !== null) {
                return scrapViewId.selectedScrapItem.outlinePointView
            }
            return null;
        }
    }

    NoteStationInteraction {
        id: addStationInteraction
        anchors.fill: parent
        scrapView: scrapViewId
        target: noteArea.targetItem
    }

    NoteLeadInteraction {
        id: addLeadInteraction
        objectName: "addLeadInteraction"
        anchors.fill: parent
        scrapView: scrapViewId
        target: noteArea.targetItem
    }

    NoteItemSelectionInteraction {
        id: noteSelectionInteraction
        target: noteArea.targetItem
        scrapView: scrapViewId
        imageItem: noteArea
    }

    NoteNorthInteraction {
        id: noteNorthUpInteraction
        anchors.fill: parent
        target: noteArea.targetItem
        zoom: noteArea.targetItem.scale
        scrapView: scrapViewId
        z:1
    }

    NoteScaleInteraction {
        id: noteScaleInteraction
        target: noteArea.targetItem
        imageItem: noteArea
        zoom: noteArea.targetItem.scale
        note: noteArea.note
        z: 1
        scrapView: scrapViewId
    }

    NoteDPIInteraction {
        id: noteDPIInteraction
        target: noteArea.targetItem
        anchors.fill: parent
        imageItem: noteArea
        zoom: noteArea.targetItem.scale
        note: noteArea.note
        imageResolution: noteArea.note != null ? noteArea.note.imageResolution : null
        z:1
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            panZoomInteraction,
            addScrapInteraction,
            addStationInteraction,
            addLeadInteraction,
            noteSelectionInteraction,
            noteNorthUpInteraction,
            noteScaleInteraction,
            noteDPIInteraction
        ]
        defaultInteraction: panZoomInteraction

        onActiveInteractionChanged: {
            if(activeInteraction == defaultInteraction) {
                switch(noteArea.state) {
                case NoteToolMode.addScrap:
                    addScrapInteraction.activate();
                    break;
                case NoteToolMode.addStation:
                    addStationInteraction.activate();
                    break;
                case NoteToolMode.addLead:
                    addLeadInteraction.activate();
                    break;
                case NoteToolMode.select:
                    noteSelectionInteraction.activate();
                    break;
                }
            }
        }
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.topMargin: 5
        spacing: 5
        z: 2

        NoteResolution {
            id: noteResolutionId
            note: noteArea.note
            collapsed: noteArea.isNarrow
            visible: noteArea.editingOverlaysVisible

            onActivateDPIInteraction: interactionManagerId.active(noteDPIInteraction)
        }

        NoteTransformEditor {
            id: noteTransformEditorId
            collapsed: noteArea.isNarrow
            interactionManager: interactionManagerId
            northInteraction: noteNorthUpInteraction
            scaleInteraction: noteScaleInteraction
            scrap: {
                if(scrapViewId.selectedScrapItem !== null) {
                    return scrapViewId.selectedScrapItem.scrap;
                }
                return null;
            }
        }

        LeadInfoEditor {
            id: leadInfoEditor
            leadView: scrapViewId.selectedScrapItem !== null ? scrapViewId.selectedScrapItem.leadView : null
        }
    }

    //For rendering scraps onto the view
    ScrapView {
        id: scrapViewId
        objectName: "scrapViewId"
        parent: noteArea.targetItem
        note: noteArea.note

        zoom: noteArea.targetItem.scale //Zoom is needed for keep the line width constant
        visible: noteArea.editingOverlaysVisible
        targetItem: scrapPointsContainer
    }

    states: [
        QQ.State {
            name: NoteToolMode.addStation
        },

        QQ.State {
            name: NoteToolMode.addScrap
        },

        QQ.State {
            name: NoteToolMode.addLead
        },

        QQ.State {
            name: NoteToolMode.select
        }
    ]

    transitions: [
         QQ.Transition {
            to: NoteToolMode.addScrap
            QQ.ScriptAction {
                script: {
                    interactionManagerId.active(addScrapInteraction)
                    addScrapInteraction.startNewScrap()
                }
            }
        },

         QQ.Transition {
            to: NoteToolMode.addStation
            QQ.ScriptAction {
                script: interactionManagerId.active(addStationInteraction)
            }
        },

         QQ.Transition {
            to: NoteToolMode.addLead
            QQ.ScriptAction {
                script: interactionManagerId.active(addLeadInteraction)
            }
        },

         QQ.Transition {
            to: NoteToolMode.none
            QQ.ScriptAction {
                script: {
                    scrapViewId.clearSelection();
                    interactionManagerId.active(panZoomInteraction)
                }
            }
        },

         QQ.Transition {
            to: NoteToolMode.select
            QQ.ScriptAction {
                script: {
                    interactionManagerId.active(noteSelectionInteraction)
                }
            }
        },

        //Runs only for modes no transition above names (CARPET, NO_NOTES, the
        //sketch tools): Qt picks the most specific matching transition, so this
        //one loses to every `to:` above it regardless of declaration order.
        //Tearing down here keeps an unhandled mode tool-free instead of leaving
        //whichever tool was active still running.
        QQ.Transition {
            QQ.ScriptAction {
                script: {
                    scrapViewId.clearSelection();
                    interactionManagerId.active(panZoomInteraction)
                }
            }
        }
    ]

}
