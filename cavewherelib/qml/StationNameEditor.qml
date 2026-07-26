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
  same component) and the editor reads the scope off whichever field it is
  given. So the completer exists only while a station is being edited — no
  other field carries autocomplete machinery it has to stay inert for.
 */
ShadowTextEditor {
    id: stationEditor

    //Null for any field that isn't a station one, which leaves the completer
    //inert — this editor is only opened by station fields, but nothing in the
    //host enforces that.
    readonly property StationDoubleClickTextInput stationField:
        stationEditor.field as StationDoubleClickTextInput

    ScopeStationCompleter {
        anchors.fill: parent

        textInput: stationEditor.textInput
        scopeModel: stationEditor.stationField !== null
                    ? stationEditor.stationField.scopeModel
                    : null

        onPickCommitted: {
            if(stationEditor.field !== null) {
                stationEditor.field.commitChanges()
            }
        }
    }
}
