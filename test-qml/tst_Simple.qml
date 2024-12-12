import QtQuick
import QtTest
import QmlTestRecorder
import CaveWhere.Simple

Item {
    id: rootId
    objectName: "rootId"

    width: simpleId.width
    height: simpleId.height

    TestcaseRecorder {
        id: recorderId
        rootItem: simpleId
        rootItemId: "rootId"
    }

    Simple {
        id: simpleId
    }

    TestCase {
        name: "SimpleButtons"
        when: windowShown

        function test_nextButtonPressedShowError() {
            let label_obj1 = ObjectFinder.findObjectByChain(rootId, "rootId->hello->label")
            mousePress(label_obj1, 40.1133, 9.22656)
            mouseRelease(label_obj1, 39.6211, 9.22656)
            verify(simpleId.concatenatedString === "Hello ")

            let label_obj2 = ObjectFinder.findObjectByChain(rootId, "rootId->world->label")
            mousePress(label_obj2, 38.5859, 9.98047)
            mouseRelease(label_obj2, 38.3398, 9.98047)
            verify(simpleId.concatenatedString === "Hello World ")

            let label_obj3 = ObjectFinder.findObjectByChain(rootId, "rootId->qt->label")
            mousePress(label_obj3, 33.5781, 8.84375)
            mouseRelease(label_obj3, 33.5781, 8.84375)
            verify(simpleId.concatenatedString === "Hello World Qt ")

            let label_obj4 = ObjectFinder.findObjectByChain(rootId, "rootId->reset->label")
            mouseClick(label_obj4, 12.9258, 9.15625)
            verify(simpleId.concatenatedString === "")
        }
    }
}
