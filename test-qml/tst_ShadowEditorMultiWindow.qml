pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Window
import QtTest
import cavewherelib

// Two windows, each with its own overlay. This is issue #494's shape: the Data
// page is wall-to-wall ClickTextInput, so tearing it into a second window means
// two windows want an editor at once.
//
// A QML singleton could not do this. There is one QQmlEngine for the whole
// application, so a singleton host is shared by every window, and an Item has
// one parent — whichever window claimed it last took it from the other, and the
// loser's fields opened an editor in the wrong window, silently.
//
// compare() is avoided on Items on purpose: when they are not identical it
// walks their properties until the JS stack overflows, which reports as
// "Maximum call stack size exceeded" instead of the difference.
QQ.Item {
    id: rootId
    width: 100
    height: 50

    Window {
        id: windowAId

        width: 300
        height: 200
        title: "A"

        AppOverlay {
            id: overlayAId
        }

        ClickTextInput {
            id: fieldAId

            x: 20
            y: 30
            text: "alpha"
        }
    }

    Window {
        id: windowBId

        width: 300
        height: 200
        title: "B"

        AppOverlay {
            id: overlayBId
        }

        ClickTextInput {
            id: fieldBId

            x: 40
            y: 60
            text: "beta"
        }
    }

    TestCase {
        name: "ShadowEditorMultiWindow"
        when: windowShown

        function initTestCase() {
            windowAId.visible = true
            windowBId.visible = true
        }

        function cleanup() {
            fieldAId.closeEditor()
            fieldBId.closeEditor()
        }

        //Two AppOverlays always carry two distinct hosts, whatever the registry
        //does, so asserting only that proves nothing. What has to hold is that
        //each *field* resolves the host of the window it is actually in.
        function test_eachFieldResolvesItsOwnWindowsHost() {
            verify(overlayAId.shadowEditor !== null, "window A's overlay carries a host")
            verify(overlayBId.shadowEditor !== null, "window B's overlay carries a host")
            verify(overlayAId.shadowEditor !== overlayBId.shadowEditor,
                   "one host per window, not one shared between them")

            verify(fieldAId._shadowEditor === overlayAId.shadowEditor,
                   "window A's field must resolve window A's host")
            verify(fieldBId._shadowEditor === overlayBId.shadowEditor,
                   "window B's field must resolve window B's host, not the "
                   + "first-registered one")
        }

        function test_aFieldEditsInTheWindowItIsIn() {
            fieldAId.openEditor()

            verify(overlayAId.shadowEditor.coreClickInput === fieldAId,
                   "window A's field must open window A's editor")
            verify(overlayBId.shadowEditor.coreClickInput === null,
                   "and must leave the other window's editor alone")
        }

        function test_bothWindowsCanEditAtOnce() {
            //Two windows are two places the user can be typing. Neither editor
            //may close the other, which is what one shared host would do.
            fieldAId.openEditor()
            fieldBId.openEditor()

            verify(overlayAId.shadowEditor.coreClickInput === fieldAId,
                   "opening an editor in window B closed window A's")
            verify(overlayBId.shadowEditor.coreClickInput === fieldBId,
                   "window B's field must hold window B's editor")
        }
    }
}
