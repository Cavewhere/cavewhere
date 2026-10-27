/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

/**
  The editor station-name fields open: the plain shadow editor plus in-scope
  station autocomplete.

  A field declares this as its CoreClickTextInput.editorComponent (through the
  shared EditorComponents.stationName, so all station fields hand the host the
  same component) and the editor reads CoreClickTextInput.stationScope off
  whichever field it is given. So the completer exists only while a station
  is being edited — no other field carries autocomplete machinery it has to
  stay inert for.
 */
ShadowTextEditor {
    id: stationEditor

    //Null for a field that sets no scope, which leaves the completer inert.
    readonly property ScopeStationListModel stationScope: stationEditor.field?.stationScope ?? null
    readonly property alias completer: completerId

    ScopeStationCompleter {
        id: completerId

        anchors.fill: parent

        textInput: stationEditor.textInput
        scopeModel: stationEditor.stationScope

        onPickCommitted: {
            if(stationEditor.field !== null) {
                stationEditor.field.commitChanges()
            }
        }
    }
}
