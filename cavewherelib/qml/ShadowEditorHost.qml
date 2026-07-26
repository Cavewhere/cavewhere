/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

/**
  The shadow-editor host. Fields (CoreClickTextInput) hand editing off to it:
  it loads the editor that field asked for, positions it over the field, and
  commits when the user presses somewhere else.

  It knows how to show *an* editor, never what a particular editor is for.
  Fields that need more than plain text name their own with
  CoreClickTextInput.editorComponent, and that editor carries its own extras
  (see StationNameEditor) instead of this host growing a slot per feature.

  One per window, held by AppOverlay. Fields find the one for their own window
  through the WindowOverlay attached property rather than naming it, so nothing
  here assumes there is only one of it (issue #494).

  Editing goes through this host's own signals and functions. Nothing outside
  may reach the loaded editor or its text input: both are destroyed and rebuilt
  whenever a field asks for a different editor, so anything holding one is left
  pointing at a corpse — and a QQ.PropertyChanges that captured it resolves its
  target once and never re-resolves, which fails silently.
 */
QQ.MouseArea {
    id: shadowEditorHost

    property CoreClickTextInput coreClickInput

    //The loaded editor, for the length of one statement only. Editors declare
    //their own extras, so a caller that needs those (a test asserting which
    //editor opened, say) has to look; storing it is the mistake, not looking.
    readonly property ShadowTextEditor currentEditor: editorLoader.item as ShadowTextEditor

    //The live editor's text input. Internal on purpose: every caller outside
    //this file goes through the functions below, which is what keeps the
    //null-guarding and the "don't hold it" rule in one place.
    readonly property GlobalTextInputHelper _input: shadowEditorHost.currentEditor !== null
                                                    ? shadowEditorHost.currentEditor.textInput
                                                    : null

    //Re-emitted from whichever editor is live, so consumers bind to something
    //that outlives the swap.
    signal enterPressed()
    signal escapePressed()
    signal pressKeyPressed(pressKeyEvent: var)
    signal editorFocusChanged(bool editorFocus)

    anchors.fill: parent
    enabled: false
    visible: enabled

    //Returns false when the field's editorComponent could not be loaded, so the
    //caller can put itself back rather than sitting blank with no editor.
    function openEditor(field: CoreClickTextInput) : bool {
        editorLoader.sourceComponent = field.editorComponent !== null
                ? field.editorComponent
                : defaultEditorComponent

        let editorItem = shadowEditorHost.currentEditor
        if(editorItem === null) {
            console.warn("editorComponent must be a ShadowTextEditor, refusing to edit",
                         field)
            return false
        }

        editorItem.field = field

        editorItem.textInput.text = field.text
        editorItem.textInput.font = field.font
        editorLoader.visible = true
        editorItem.textInput.forceActiveFocus()
        editorItem.textInput.selectAll();

        //Assigned unconditionally: the editor outlives any one field, so a
        //validator left by the previous one would filter this field's typing.
        //After the text, so the incoming value is never rejected on the way in.
        editorItem.textInput.validator = field.validator
        shadowEditorHost.clearError()

        shadowEditorHost.enabled = true

        //Set the editor's position. This host fills its window's overlay, so a
        //field anywhere in the same window maps straight into the coordinates
        //the editor is laid out in.
        let hostPosition = field.mapToItem(shadowEditorHost, 0, 0)
        editorLoader.x = hostPosition.x - editorItem.contentMargin
        editorLoader.y = hostPosition.y - editorItem.contentMargin

        editorItem.minWidth = field.width + editorItem.contentMargin * 2
        editorItem.minHeight = field.height + editorItem.contentMargin * 2

        //Connect to commitChanges()
        shadowEditorHost.coreClickInput = field
        return true
    }

    function closeEditor() {
        editorLoader.visible = false

        if(shadowEditorHost.currentEditor !== null) {
            shadowEditorHost._input.focus = false
            shadowEditorHost._input.validator = null
            shadowEditorHost.clearError()
            shadowEditorHost.currentEditor.field = null
        }

        shadowEditorHost.enabled = false
        shadowEditorHost.coreClickInput = null
    }

    //What's being edited right now. Empty when nothing is.
    function editorText() : string {
        return shadowEditorHost._input?.text ?? ""
    }

    function setEditorText(text: string) {
        if(shadowEditorHost._input !== null) {
            shadowEditorHost._input.text = text
        }
    }

    function clearSelection() {
        if(shadowEditorHost._input !== null) {
            shadowEditorHost._input.select(shadowEditorHost._input.cursorPosition,
                                           shadowEditorHost._input.cursorPosition)
        }
    }

    //forceActiveFocus, not focus: the editor sits inside a QQ.Loader, which is
    //a focus scope, so `focus` is still true after a blur and setting it again
    //would change nothing.
    function focusEditor() {
        shadowEditorHost._input?.forceActiveFocus()
    }

    function editorHasFocus() : bool {
        return shadowEditorHost._input !== null && shadowEditorHost._input.activeFocus
    }

    //Why the value the user typed can't be taken, shown under the editor until
    //they fix it or give up.
    function showError(message: string) {
        if(shadowEditorHost.currentEditor !== null) {
            shadowEditorHost.currentEditor.errorHelpBox.text = message
            shadowEditorHost.currentEditor.errorHelpBox.visible = true
        }
    }

    function clearError() {
        if(shadowEditorHost.currentEditor !== null) {
            shadowEditorHost.currentEditor.errorHelpBox.visible = false
        }
    }

    function hasError() : bool {
        return shadowEditorHost.currentEditor?.errorHelpBox.visible ?? false
    }

    //Enter commits, Escape closes. Consumers handling pressKeyPressed run
    //their own keys first, then call this for the rest.
    function defaultKeyHandling() {
        shadowEditorHost._input?.defaultKeyHandling()
    }

    onPressed: (mouse) => {
        if(coreClickInput !== null) {
            var commited = coreClickInput.commitChanges()
            if(!commited) {
                coreClickInput.closeEditor()
            }
        }

        mouse.accepted = false
    }

    QQ.Connections {
        target: shadowEditorHost._input

        function onEnterPressed() { shadowEditorHost.enterPressed() }
        function onEscapePressed() { shadowEditorHost.escapePressed() }
        //The event rides along, so a consumer never has to go find it on an
        //object it isn't allowed to keep.
        function onPressKeyPressed() {
            shadowEditorHost.pressKeyPressed(shadowEditorHost._input.pressKeyEvent)
        }
        //activeFocus for the same reason GlobalTextInputHelper uses it: the
        //Loader is a focus scope, so `focus` never goes back false on its own.
        function onActiveFocusChanged() {
            shadowEditorHost.editorFocusChanged(shadowEditorHost.editorHasFocus())
        }
    }

    //One editor alive at a time, however many fields there are. It is rebuilt
    //whenever a field asks for a different component — and since an inline
    //editorComponent is wrapped per declaring instance, two fields of the same
    //kind still count as different. Nothing outside may hold onto what this
    //loads; watch the host's signals instead.
    QQ.Loader {
        id: editorLoader

        visible: false
        sourceComponent: defaultEditorComponent
    }

    QQ.Component {
        id: defaultEditorComponent

        ShadowTextEditor {}
    }
}
