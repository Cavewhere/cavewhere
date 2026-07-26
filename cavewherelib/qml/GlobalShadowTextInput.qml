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
 */
QQ.MouseArea {
    id: globalMouseArea

    property CoreClickTextInput coreClickInput

    readonly property ShadowTextEditor currentEditor: editorLoader.item as ShadowTextEditor
    readonly property GlobalTextInputHelper textInput: globalMouseArea.currentEditor !== null
                                                       ? globalMouseArea.currentEditor.textInput
                                                       : null
    readonly property ErrorHelpBox errorHelpBox: globalMouseArea.currentEditor !== null
                                                 ? globalMouseArea.currentEditor.errorHelpBox
                                                 : null

    //Re-emitted from whichever editor is live. Consumers must bind to these and
    //never to textInput itself: the editor is swapped out and destroyed when a
    //field asks for a different one, and a QQ.PropertyChanges bound to the old
    //object resolves it once and never re-resolves, so it silently dies.
    signal enterPressed()
    signal escapePressed()
    signal pressKeyPressed()
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
        editorItem.errorHelpBox.visible = false

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
            globalMouseArea.textInput.focus = false
            globalMouseArea.textInput.validator = null
            globalMouseArea.errorHelpBox.visible = false
            globalMouseArea.currentEditor.field = null
        }

        globalMouseArea.enabled = false
        globalMouseArea.coreClickInput = null
    }

    function clearSelection() {
        if(globalMouseArea.textInput !== null) {
            globalMouseArea.textInput.select(globalMouseArea.textInput.cursorPosition,
                                             globalMouseArea.textInput.cursorPosition)
        }
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
        target: globalMouseArea.textInput

        function onEnterPressed() { globalMouseArea.enterPressed() }
        function onEscapePressed() { globalMouseArea.escapePressed() }
        function onPressKeyPressed() { globalMouseArea.pressKeyPressed() }
        //activeFocus for the same reason GlobalTextInputHelper uses it: the
        //Loader is a focus scope, so `focus` never goes back false on its own.
        function onActiveFocusChanged() {
            globalMouseArea.editorFocusChanged(globalMouseArea.textInput.activeFocus)
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
