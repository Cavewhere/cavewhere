import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl

// The right-click menu of every text editor in the style, adapted from Basic's
// TextEditingContextMenu: Qt's own edit actions, drawn by this style's Menu.
Menu {
    id: menu

    required property QQ.Item editor

    popupType: Qt.platform.pluginName !== "wayland" ? T.Popup.Window : T.Popup.Item

    UndoAction {
        editor: menu.editor
    }
    RedoAction {
        editor: menu.editor
    }

    MenuSeparator { }

    CutAction {
        editor: menu.editor
    }
    CopyAction {
        editor: menu.editor
    }
    PasteAction {
        editor: menu.editor
    }
    DeleteAction {
        editor: menu.editor
    }

    MenuSeparator { }

    SelectAllAction {
        editor: menu.editor
    }
}
