/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
pragma Singleton

import QtQuick as QQ
import cavewherelib

/**
  The one shadow-editor host. Fields (CoreClickTextInput) hand editing off to
  it: it loads the editor that field asked for, positions it over the field,
  and commits when the user presses somewhere else.

  It knows how to show *an* editor, never what a particular editor is for.
  Fields that need more than plain text name their own with
  CoreClickTextInput.editorComponent, and that editor carries its own extras
  (see StationNameEditor) instead of this host growing a slot per feature.

  Editing goes through this host's own signals and functions. Nothing outside
  may reach the loaded editor or its text input: both are destroyed and rebuilt
  whenever a field asks for a different editor, so anything holding one is left
  pointing at a corpse — and a QQ.PropertyChanges that captured it resolves its
  target once and never re-resolves, which fails silently.
 */
QQ.MouseArea {
    id: globalMouseArea

    property CoreClickTextInput coreClickInput

    //The loaded editor, for the length of one statement only. Editors declare
    //their own extras, so a caller that needs those (a test asserting which
    //editor opened, say) has to look; storing it is the mistake, not looking.
    readonly property ShadowTextEditor currentEditor: editorLoader.item as ShadowTextEditor

    //The live editor's text input. Internal on purpose: every caller outside
    //this file goes through the functions below, which is what keeps the
    //null-guarding and the "don't hold it" rule in one place.
    readonly property GlobalTextInputHelper _input: globalMouseArea.currentEditor !== null
                                                    ? globalMouseArea.currentEditor.textInput
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

        let editorItem = globalMouseArea.currentEditor
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
        globalMouseArea.clearError()

        globalMouseArea.enabled = true

        //Set the editor's position
        //Calling this function with just GlobalShadowTextInput cause a crash, maybe because it's a singleton?
        //Using the parent, should be the CavewhereMainWindow
        let globalPosition = field.mapToItem(globalMouseArea.parent, 0, 0)
        editorLoader.x = globalPosition.x - editorItem.contentMargin
        editorLoader.y = globalPosition.y - editorItem.contentMargin

        editorItem.minWidth = field.width + editorItem.contentMargin * 2
        editorItem.minHeight = field.height + editorItem.contentMargin * 2

        //Connect to commitChanges()
        globalMouseArea.coreClickInput = field
        return true
    }

    function closeEditor() {
        editorLoader.visible = false

        if(globalMouseArea.currentEditor !== null) {
            globalMouseArea._input.focus = false
            globalMouseArea._input.validator = null
            globalMouseArea.clearError()
            globalMouseArea.currentEditor.field = null
        }

        globalMouseArea.enabled = false
        globalMouseArea.coreClickInput = null
    }

    //What's being edited right now. Empty when nothing is.
    function editorText() : string {
        return globalMouseArea._input?.text ?? ""
    }

    function setEditorText(text: string) {
        if(globalMouseArea._input !== null) {
            globalMouseArea._input.text = text
        }
    }

    function clearSelection() {
        if(globalMouseArea._input !== null) {
            globalMouseArea._input.select(globalMouseArea._input.cursorPosition,
                                          globalMouseArea._input.cursorPosition)
        }
    }

    //forceActiveFocus, not focus: the editor sits inside a QQ.Loader, which is
    //a focus scope, so `focus` is still true after a blur and setting it again
    //would change nothing.
    function focusEditor() {
        globalMouseArea._input?.forceActiveFocus()
    }

    function editorHasFocus() : bool {
        return globalMouseArea._input !== null && globalMouseArea._input.activeFocus
    }

    //Why the value the user typed can't be taken, shown under the editor until
    //they fix it or give up.
    function showError(message: string) {
        if(globalMouseArea.currentEditor !== null) {
            globalMouseArea.currentEditor.errorHelpBox.text = message
            globalMouseArea.currentEditor.errorHelpBox.visible = true
        }
    }

    function clearError() {
        if(globalMouseArea.currentEditor !== null) {
            globalMouseArea.currentEditor.errorHelpBox.visible = false
        }
    }

    function hasError() : bool {
        return globalMouseArea.currentEditor?.errorHelpBox.visible ?? false
    }

    //Enter commits, Escape closes. Consumers handling pressKeyPressed run
    //their own keys first, then call this for the rest.
    function defaultKeyHandling() {
        globalMouseArea._input?.defaultKeyHandling()
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
        target: globalMouseArea._input

        function onEnterPressed() { globalMouseArea.enterPressed() }
        function onEscapePressed() { globalMouseArea.escapePressed() }
        //The event rides along, so a consumer never has to go find it on an
        //object it isn't allowed to keep.
        function onPressKeyPressed() {
            globalMouseArea.pressKeyPressed(globalMouseArea._input.pressKeyEvent)
        }
        //activeFocus for the same reason GlobalTextInputHelper uses it: the
        //Loader is a focus scope, so `focus` never goes back false on its own.
        function onActiveFocusChanged() {
            globalMouseArea.editorFocusChanged(globalMouseArea.editorHasFocus())
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
