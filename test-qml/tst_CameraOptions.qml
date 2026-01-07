import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "CameraOptions"
        when: windowShown

        function test_resetView() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")

            let originalAzimuth = turnTableInteraction.azimuth;
            let originalPitch = turnTableInteraction.pitch
            let originalZoom = turnTableInteraction.camera.zoomScale
            let originalPosition = Qt.vector3d(turnTableInteraction.camera.position.x,
                                               turnTableInteraction.camera.position.y,
                                               turnTableInteraction.camera.position.z)

            //Do some view interaction
            mouseWheel(renderer, 100, 100, 0, -20)
            wait(50)
            mouseDrag(renderer, renderer.width * 0.5,  renderer.height * 0.5, 5, -25, Qt.RightButton)
            wait(50)
            mouseDrag(renderer, renderer.width * 0.5,  renderer.height * 0.5, 100, 100)
            wait(50)

            //Make sure the view has changed
            tryVerify(() => { return originalAzimuth !== turnTableInteraction.azimuth })
            tryVerify(() => { return originalPitch !== turnTableInteraction.pitch })
            tryVerify(() => { return originalZoom !== turnTableInteraction.camera.zoomScale })
            tryVerify(() => { return originalPosition.x !== turnTableInteraction.camera.position.x
                      && originalPosition.y !== turnTableInteraction.camera.position.y
                      && originalPosition.z !== turnTableInteraction.camera.position.z
                      })

            //Reset the view                    
            let resetViewButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->cameraOptions->resetViewButton")
            mouseClick(resetViewButton)

            //Make sure the text updates correctly
            let azimuthText = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->cameraOptions->GroupBox->cameraAzimuthText")
            tryVerify(() => { return azimuthText.text === "0.0" });

            let pitchText = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->cameraOptions->GroupBox->pitchText")
            tryVerify(() => { return pitchText.text === "90.0" });

            //Make sure the view is correct
            tryVerify(() => { return originalAzimuth === turnTableInteraction.azimuth })
            tryVerify(() => { return originalPitch === turnTableInteraction.pitch })
            tryVerify(() => { return originalZoom === turnTableInteraction.camera.zoomScale })
            tryVerify(() => { return originalPosition.x === turnTableInteraction.camera.position.x
                      && originalPosition.y === turnTableInteraction.camera.position.y
                      && originalPosition.z === turnTableInteraction.camera.position.z
                      })

            //Change the projection to perspectiveProjection
            let projectionSlider = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->cameraOptions->GroupBox->projectionSlider")
            let slider = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->cameraOptions->GroupBox->projectionSlider->slider")
            verify(projectionSlider.viewer.orthoProjection.enabled === true)
            verify(projectionSlider.viewer.perspectiveProjection.enabled === false)
            mouseClick(slider, slider.width-1, 0)
            tryVerify(() => { return projectionSlider.viewer.orthoProjection.enabled === false})
            tryVerify(() => { return projectionSlider.viewer.perspectiveProjection.enabled === true})

            // wait(500)

            //Do some view interaction
            mouseWheel(renderer, 100, 100, 0, -20)
            wait(50)
            mouseDrag(renderer, renderer.width * 0.5,  renderer.height * 0.5, 50, 50)
            wait(50)
            mouseDrag(renderer, renderer.width * 0.5,  renderer.height * 0.5, 5, -25, Qt.RightButton)
            wait(50)

            //Make sure the view has changed
            tryVerify(() => { return originalAzimuth !== turnTableInteraction.azimuth })
            tryVerify(() => { return originalPitch !== turnTableInteraction.pitch })
            tryVerify(() => { return originalZoom === turnTableInteraction.camera.zoomScale }) //Zoom scale doesn't change with prespective projection
            tryVerify(() => { return originalPosition.x !== turnTableInteraction.camera.position.x
                      && originalPosition.y !== turnTableInteraction.camera.position.y
                      && originalPosition.z !== turnTableInteraction.camera.position.z
                      })

            //Reset the view
            mouseClick(resetViewButton)

            //Make sure the view is correct
            tryVerify(() => { return originalAzimuth === turnTableInteraction.azimuth })
            tryVerify(() => { return originalPitch === turnTableInteraction.pitch })
            tryVerify(() => { return originalZoom === turnTableInteraction.camera.zoomScale })
            tryVerify(() => { return originalPosition.x === turnTableInteraction.camera.position.x
                      && originalPosition.y === turnTableInteraction.camera.position.y
                      && originalPosition.z === turnTableInteraction.camera.position.z
                      })
        }
    }
}
