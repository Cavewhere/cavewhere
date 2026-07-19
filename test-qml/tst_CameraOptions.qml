import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    // Reset now frames the whole scene and animates there (issue #549), so
    // tests wait on this signal instead of assuming an instant snap.
    SignalSpy {
        id: resetSpy
        signalName: "animationFinished"
    }

    TestCase {
        name: "CameraOptions"
        when: windowShown

        // Two animated resets to the same framing target interpolate from
        // different start states, so their end values can differ by a few ULP.
        // Compare position, zoom, and orientation with a tolerance the way the
        // C++ animated-view tests do, rather than exact equality — the pitch
        // lerp is only bit-exact while pitch stays in [45, 90] (Sterbenz), and
        // the drags below can leave it just outside.
        readonly property real framingEpsilon: 0.01

        function vectorsClose(a, b) {
            return Math.abs(a.x - b.x) < framingEpsilon
                && Math.abs(a.y - b.y) < framingEpsilon
                && Math.abs(a.z - b.z) < framingEpsilon;
        }

        // Ortho scale comes from zoomScale, not the eye distance, so the camera
        // depth (z) is a visually irrelevant free parameter under ortho. Compare
        // only the in-plane position there.
        function pointsCloseXY(a, b) {
            return Math.abs(a.x - b.x) < framingEpsilon
                && Math.abs(a.y - b.y) < framingEpsilon;
        }

        function clickResetAndWait(resetViewButton) {
            resetSpy.clear();
            mouseClick(resetViewButton);
            resetSpy.wait();
        }

        // The scene geometry (line plot, scraps) registers into the intersecter
        // asynchronously after load, growing the scene bounding box and thus the
        // framed view. Reset until the framed zoom stops changing, so a captured
        // reference reset is stable. Leaves the view at a settled framed reset.
        function settleFraming(turnTableInteraction, resetViewButton) {
            let previous = Number.NaN;
            for (let i = 0; i < 15; i++) {
                clickResetAndWait(resetViewButton);
                let current = turnTableInteraction.camera.zoomScale;
                if (Math.abs(current - previous) < framingEpsilon) {
                    return;
                }
                previous = current;
            }
            fail("scene framing did not stabilize");
        }

        function test_resetView() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            //Zoom into the data, in the 3d view
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer->turnTableInteraction")
            resetSpy.target = turnTableInteraction;

            let resetViewButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->cameraOptions->resetViewButton")

            //Wait for the line-plot solve to finish, then settle the framing so
            //the scene bounding box (and thus the framed view) is stable before
            //capturing a reference reset.
            tryVerify(() => RootData.linePlotManager.cavernLog.length > 0)

            //Establish the canonical framed view (ortho). Reset frames the scene
            //bounding box, so the reference is the post-reset state, not the
            //fixed default pose the view loads with.
            settleFraming(turnTableInteraction, resetViewButton);

            let originalAzimuth = turnTableInteraction.azimuth;
            let originalPitch = turnTableInteraction.pitch
            let originalZoom = turnTableInteraction.camera.zoomScale
            let originalPosition = Qt.vector3d(turnTableInteraction.camera.position.x,
                                               turnTableInteraction.camera.position.y,
                                               turnTableInteraction.camera.position.z)

            //Do some view interaction
            mouseWheel(renderer, 100, 100, 0, -20)
            mouseDrag(renderer, renderer.width * 0.5,  renderer.height * 0.5, 5, -25, Qt.RightButton)
            mouseDrag(renderer, renderer.width * 0.5,  renderer.height * 0.5, 100, 100)

            //Make sure the view has changed
            tryVerify(() => { return originalAzimuth !== turnTableInteraction.azimuth })
            tryVerify(() => { return originalPitch !== turnTableInteraction.pitch })
            tryVerify(() => { return originalZoom !== turnTableInteraction.camera.zoomScale })
            tryVerify(() => { return originalPosition.x !== turnTableInteraction.camera.position.x
                      && originalPosition.y !== turnTableInteraction.camera.position.y
                      && originalPosition.z !== turnTableInteraction.camera.position.z
                      })

            //Reset the view
            clickResetAndWait(resetViewButton);

            //Make sure the text updates correctly
            let azimuthText = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->cameraOptions->GroupBox->cameraAzimuthText")
            tryVerify(() => { return azimuthText.text === "0.0" });

            let pitchText = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->cameraOptions->GroupBox->pitchText")
            tryVerify(() => { return pitchText.text === "90.0" });

            //Make sure the view is back to the canonical framed view
            verify(Math.abs(turnTableInteraction.azimuth - originalAzimuth) < framingEpsilon)
            verify(Math.abs(turnTableInteraction.pitch - originalPitch) < framingEpsilon)
            verify(Math.abs(turnTableInteraction.camera.zoomScale - originalZoom) < framingEpsilon)
            verify(pointsCloseXY(turnTableInteraction.camera.position, originalPosition))

            //Change the projection to perspectiveProjection
            let projectionSlider = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->cameraOptions->GroupBox->projectionSlider")
            let slider = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->cameraOptions->GroupBox->projectionSlider->slider")
            verify(projectionSlider.projectionTransition.orthoProjection.enabled === true)
            verify(projectionSlider.projectionTransition.perspectiveProjection.enabled === false)
            mouseClick(slider, slider.width-1, 0)
            tryVerify(() => { return projectionSlider.projectionTransition.orthoProjection.enabled === false})
            tryVerify(() => { return projectionSlider.projectionTransition.perspectiveProjection.enabled === true})

            //Perspective frames to an eye distance rather than an ortho zoom, so
            //the framed view differs from the ortho one. Re-establish the
            //reference after a perspective reset.
            clickResetAndWait(resetViewButton);

            let perspectiveAzimuth = turnTableInteraction.azimuth;
            let perspectivePitch = turnTableInteraction.pitch
            let perspectiveZoom = turnTableInteraction.camera.zoomScale
            let perspectivePosition = Qt.vector3d(turnTableInteraction.camera.position.x,
                                                  turnTableInteraction.camera.position.y,
                                                  turnTableInteraction.camera.position.z)

            //Do some view interaction
            mouseWheel(renderer, 100, 100, 0, -20)
            mouseDrag(renderer, renderer.width * 0.5,  renderer.height * 0.5, 50, 50)
            mouseDrag(renderer, renderer.width * 0.5,  renderer.height * 0.5, 5, -25, Qt.RightButton)

            //Make sure the view has changed
            tryVerify(() => { return perspectiveAzimuth !== turnTableInteraction.azimuth })
            tryVerify(() => { return perspectivePitch !== turnTableInteraction.pitch })
            tryVerify(() => { return perspectiveZoom === turnTableInteraction.camera.zoomScale }) //Zoom scale doesn't change with perspective projection
            tryVerify(() => { return perspectivePosition.x !== turnTableInteraction.camera.position.x
                      && perspectivePosition.y !== turnTableInteraction.camera.position.y
                      && perspectivePosition.z !== turnTableInteraction.camera.position.z
                      })

            //Reset the view
            clickResetAndWait(resetViewButton);

            //Make sure the view is back to the canonical framed view
            verify(Math.abs(turnTableInteraction.azimuth - perspectiveAzimuth) < framingEpsilon)
            verify(Math.abs(turnTableInteraction.pitch - perspectivePitch) < framingEpsilon)
            verify(Math.abs(turnTableInteraction.camera.zoomScale - perspectiveZoom) < framingEpsilon)
            verify(vectorsClose(turnTableInteraction.camera.position, perspectivePosition))
        }
    }
}
