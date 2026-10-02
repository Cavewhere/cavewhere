import QtQuick as QQ
import QtTest
import cavewherelib
import QmlTestRecorder

// The generic "go to the View page and turn tool X on" seam. Every test here
// registers its own throw-away tool names, so it exercises the seam without
// depending on which real tools happen to exist — except the last one, which
// checks that the Select Area tool really did claim the name Add Layer arms.
MainWindowTest {
    id: rootId

    // A registration that counts its own arming. Counting inside the object is
    // what lets a test see an arming that happens during creation, which a spy
    // attached afterward would already have missed.
    QQ.Component {
        id: registrationComponentId

        ViewToolRegistration {
            property int armedCount: 0

            onArmed: armedCount++
        }
    }

    CWTestCase {
        name: "ViewTools"
        when: windowShown

        property list<ViewToolRegistration> createdRegistrations: []

        // A registration owned by the test rather than by a tool, so its
        // lifetime is something the test can end on purpose.
        function createRegistration(toolName: string) : ViewToolRegistration {
            let registration = registrationComponentId.createObject(rootId, {toolName: toolName})
            verify(registration !== null, "the registration " + toolName + " was not created")
            createdRegistrations.push(registration)
            return registration
        }

        function destroyRegistration(registration: ViewToolRegistration) {
            let index = createdRegistrations.indexOf(registration)
            if (index >= 0) {
                createdRegistrations.splice(index, 1)
            }

            let toolName = registration.toolName
            registration.destroy()
            tryVerify(() => ViewTools.findRegistration(toolName) === null, 5000,
                      "the destroyed registration " + toolName + " stayed in ViewTools")
        }

        function gotoMapPage() {
            RootData.pageSelectionModel.gotoPageByName(null, "Map")
            tryVerify(() => RootData.pageSelectionModel.currentPage.name === "Map", 5000,
                      "the Map page never became current")
        }

        function init() {
            RootData.pageSelectionModel.gotoPageByName(null, "View")
        }

        function cleanup() {
            // A pending arm outlives the test that made it, so drop it here
            // rather than at the end of each test where a failed assertion
            // would skip it.
            ViewTools.cancelArm()

            while (createdRegistrations.length > 0) {
                destroyRegistration(createdRegistrations[0])
            }
        }

        function test_armActivatesARegisteredToolAndGoesToTheView() {
            let registration = createRegistration("TEST-REGISTERED")

            gotoMapPage()

            ViewTools.arm("TEST-REGISTERED")

            compare(registration.armedCount, 1, "the registered tool was not armed")
            compare(ViewTools.pendingTool, "", "the pending tool was not cleared")
            verify(ViewTools.viewPageCurrent, "arm() did not go to the View page")
        }

        function test_armWaitsForARegistrationThatComesLater() {
            ViewTools.arm("TEST-LATE")

            compare(ViewTools.pendingTool, "TEST-LATE", "the unregistered tool was not left pending")
            verify(ViewTools.viewPageCurrent, "arm() did not go to the View page")

            // The registration arrives with the View page already current, so
            // registering is what arms it.
            let registration = createRegistration("TEST-LATE")

            compare(registration.armedCount, 1, "registering did not answer the pending arm")
            compare(ViewTools.pendingTool, "", "the pending tool was not cleared")
        }

        function test_aSecondArmReplacesTheFirst() {
            ViewTools.arm("TEST-FIRST")
            ViewTools.arm("TEST-SECOND")

            compare(ViewTools.pendingTool, "TEST-SECOND", "the second arm() did not replace the first")

            let first = createRegistration("TEST-FIRST")
            let second = createRegistration("TEST-SECOND")

            compare(first.armedCount, 0, "the replaced tool was armed anyway")
            compare(second.armedCount, 1, "the second tool was not armed by registering")
            compare(ViewTools.pendingTool, "", "the pending tool was not cleared")
        }

        function test_anArmSurvivesLeavingTheViewPage() {
            ViewTools.arm("TEST-AWAY")
            gotoMapPage()

            compare(ViewTools.pendingTool, "TEST-AWAY", "leaving the View page dropped the pending arm")

            // Registering off the View page is not enough on its own.
            let registration = createRegistration("TEST-AWAY")

            compare(registration.armedCount, 0, "the tool was armed while another page was current")
            compare(ViewTools.pendingTool, "TEST-AWAY", "the pending tool was cleared off the View page")

            RootData.pageSelectionModel.gotoPageByName(null, "View")

            tryCompare(registration, "armedCount", 1, 5000)
            compare(ViewTools.pendingTool, "", "returning to the View page did not clear the pending arm")
        }

        function test_cancelArmDropsAPendingTool() {
            ViewTools.arm("TEST-CANCELED")
            ViewTools.cancelArm()

            compare(ViewTools.pendingTool, "", "cancelArm() left the tool pending")

            let registration = createRegistration("TEST-CANCELED")

            compare(registration.armedCount, 0, "a canceled arm still activated the tool")
        }

        function test_aDestroyedRegistrationLeavesTheSeamAlone() {
            let registration = createRegistration("TEST-GONE")
            verify(ViewTools.findRegistration("TEST-GONE") !== null,
                   "the registration never reached ViewTools")

            destroyRegistration(registration)

            ViewTools.arm("TEST-GONE")

            compare(ViewTools.pendingTool, "TEST-GONE", "arming a destroyed tool did not stay pending")
        }

        // The one name in this file that a real tool owns: the Map page's Add
        // Layer button arms it, so nothing may quietly rename either side.
        function test_theSelectExportAreaToolRegistersItsName() {
            gotoMapPage()

            tryVerify(() => ViewTools.findRegistration(ViewTools.selectExportArea) !== null, 5000,
                      "the select export area tool never registered its name")
        }
    }
}
