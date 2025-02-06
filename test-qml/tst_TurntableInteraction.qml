import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "TurnTableInteraction"
        when: windowShown

        function test_cameraPan() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")

            for(let i = 0; i < 30; i++) {
                mouseWheel(turnTableInteraction, 521, 344, 0, 100, Qt.LeftButton, Qt.NoModifier, 30)
            }

            wait(50)

            let originalPosition = Qt.vector3d(turnTableInteraction.camera.position.x,
                                               turnTableInteraction.camera.position.y,
                                               turnTableInteraction.camera.position.z)

            mouseClick(turnTableInteraction, 627.715, 371.531)

            //Make sure the camera doesn't move
            tryVerify(() => { return originalPosition.x === turnTableInteraction.camera.position.x
                      && originalPosition.y === turnTableInteraction.camera.position.y
                      && originalPosition.z === turnTableInteraction.camera.position.z
                      })
        }
    }
}
