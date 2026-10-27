/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

/**
  The floating editor a CoreClickTextInput hands off to: a shadow box holding
  the one live text input, shown over the field being edited.

  Editing is never in place — all fields share one editor at a time — but
  *which* editor opens is per field: a field names one with
  CoreClickTextInput.editorComponent, defaulting to this plain one. Anything
  that decorates editing (station-name completion, say) subclasses this, so it
  exists only for the fields that asked for it instead of riding along inert on
  every other field.

  The host (ShadowEditorHost) fills in field, minWidth and minHeight when
  it opens an editor. An editor never reaches back into its host, so hosting it
  somewhere else later is a change to one file.
 */
ShadowRectangle {
    id: editorRoot

    //The field being edited. Subclasses needing more than the text reach it
    //through here, or through their own declaration site.
    property CoreClickTextInput field

    //Floors, so the editor never opens smaller than the field it covers.
    property int minWidth: 0
    property int minHeight: 0

    readonly property alias textInput: input
    readonly property alias errorHelpBox: errorHelpBoxItem

    //Gap between the field's edge and the editor's, which the host also uses to
    //offset the editor so the text lands where the field's text was.
    readonly property int contentMargin: 3

    //Drop below the editor, clear of its shadow.
    readonly property int errorHelpBoxSpacing: 10

    color: Theme.surface

    implicitWidth: Math.max(editorRoot.minWidth, input.width + editorRoot.contentMargin * 2)
    implicitHeight: Math.max(editorRoot.minHeight, input.height + editorRoot.contentMargin * 2)

    QQ.MouseArea {
        id: borderArea

        anchors.fill: parent

        onPressed: (mouse) => {
            input.forceActiveFocus()
            mouse.accepted = true
        }
    }

    GlobalTextInputHelper {
        id: input

        field: editorRoot.field
    }

    ErrorHelpBox {
        id: errorHelpBoxItem

        //Above anything a subclass adds: those children are appended after this
        //one and would otherwise paint over it, and they share this band below
        //the field (the station completer's dropdown does).
        z: 1
        y: parent.height + editorRoot.errorHelpBoxSpacing
        visible: false
        anchors.bottom: undefined
        anchors.bottomMargin: 0
    }
}
