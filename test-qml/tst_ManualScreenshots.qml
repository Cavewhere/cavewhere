import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// User-manual screenshot harness (Deliverable B). Doubles as a regression test:
// under the headless `offscreen` QPA it skips (no live QRhi, so grabWindow()
// cannot capture the 3D view); on a GPU-backed platform it drives the real full
// UI, navigates to each documented page, and writes a PNG via WindowGrabber.
//
// Run it as the generator with scripts/gen-manual-screenshots.sh, which points
// CW_MANUAL_IMAGE_DIR at docs/manual/images/. Without that variable the grabs
// land in a temp dir, so a plain test run never writes into the source tree.
//
// Shots here back the 3D View chapter (docs/manual/view-3d/) and the Scraps /
// Carpeting chapter (docs/manual/scraps/) — the sharpest test of grabWindow(),
// since each grab must contain the QML UI chrome and the live RHI scene together,
// which the offscreen renderer cannot produce.
MainWindowTest {
    id: rootId

    HighlightOverlay {
        id: highlightOverlayId
        anchors.fill: parent
        z: 1000
        target: null
    }

    TestCase {
        name: "ManualScreenshots"
        when: windowShown

        // Breathing room left around the Scrap Info panel when cropping a
        // control shot out of the full-window grab.
        readonly property int scrapInfoCropMargin: 12

        // Load the demo cave, navigate to the 3D view, and wait for a live QRhi.
        // Returns the region viewer, or null after calling skip() when the
        // platform has no QRhi (headless `offscreen`) — callers must return on null.
        function loadRhiViewer() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            RootData.pageSelectionModel.currentPageAddress = "View";

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");
            let regionViewer = glTerrain.renderer;
            verify(regionViewer, "found the RegionViewer");

            for (let i = 0; i < 40 && !OffscreenRenderTester.windowHasRhi(regionViewer); ++i) {
                wait(50);
            }
            if (!OffscreenRenderTester.windowHasRhi(regionViewer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return null;
            }

            // Show the sidebar File button. Its loader is gated off on macOS (native
            // menu bar there); force it on so the docs match the cross-platform UI
            // users see. Harmless if already active.
            let fileLoader = findByName(rootId.mainWindow, "fileMenuButtonLoader");
            if (fileLoader) { fileLoader.active = true; }

            // Let the line-plot / carpeting jobs drain so the "Updating Scene"
            // progress chrome is gone and the 3D scene is fully rendered before any
            // grab. (The render background is left at the app's own light gradient —
            // no override — so screenshots match the real appearance.)
            RootData.futureManagerModel.waitForFinished();
            return regionViewer;
        }

        // Recursive by-objectName search over the visual tree. The side panel is
        // reparented by LayoutItemProxy, so its named-ancestor chain no longer
        // matches ObjectFinder.findObjectByChain — a plain name search still finds it.
        function findByName(item, name) {
            if (!item) { return null; }
            if (item.objectName === name) { return item; }
            let kids = item.children;
            for (let i = 0; kids && i < kids.length; ++i) {
                let found = findByName(kids[i], name);
                if (found) { return found; }
            }
            return null;
        }

        // Like findByName, but collects EVERY match. The note gallery can hold
        // several NoteItems (and thus several "scrapViewId"/"noteTransformEditor"
        // instances), only one of which is bound to the current note; a caller
        // picks the live one rather than trusting the first match.
        function findAllByName(item, name, acc) {
            if (!item) { return acc; }
            if (item.objectName === name) { acc.push(item); }
            let kids = item.children;
            for (let i = 0; kids && i < kids.length; ++i) {
                findAllByName(kids[i], name, acc);
            }
            return acc;
        }

        // First VISIBLE item with objectName `name` under `item`. The note
        // gallery carries duplicate toolbars (a wide `carpetButtonArea` and a
        // `narrowToolbar`) whose buttons share objectNames; only one toolbar is
        // shown at a time, so a plain findByName can return the hidden copy.
        function findVisibleByName(item, name) {
            let all = findAllByName(item, name, []);
            for (let i = 0; i < all.length; ++i) {
                if (all[i].visible) { return all[i]; }
            }
            return null;
        }

        // The Scrap Info editor for the currently selected scrap: the one whose
        // `scrap` is set. Falls back to the first editor found.
        function findScrapInfoEditor(page) {
            let editors = findAllByName(page, "noteTransformEditor", []);
            for (let i = 0; i < editors.length; ++i) {
                if (editors[i].scrap) { return editors[i]; }
            }
            return editors.length ? editors[0] : null;
        }

        // The View/Layers tab bar in the 3D view's side panel.
        function sidePanelTabBar() {
            return findByName(rootId.mainWindow, "renderingTabBar");
        }

        // Highlight one control row inside the Scrap Info panel and grab the panel
        // cropped tight around it. The panel is a small floating box in a
        // 2400x1400 window, so a full-window grab renders it too small to read in
        // the manual; grabItemToFile grabs the whole window and *then* crops, so
        // the highlight ring and its dim scrim still land in the crop.
        //
        // Each row is reached through a child that is already named (`anchorName`)
        // and highlighted via that child's parent — the RowLayout that *is* the
        // visible row, and which is coincident with its NoteUpInput /
        // PaperScaleInput wrapper. Naming the rows in NoteTransformEditor.qml
        // would read better here, but ObjectFinder::toChain builds a chain out of
        // every named ancestor, so a new objectName silently breaks the recorded
        // chains other tests depend on (autoCalculate->setNorthButton and friends
        // in tst_NoteNorthInteraction, tst_NoteScaleInteraction, tst_ScrapInteraction
        // and tst_ScrapSync). A screenshot is not worth that.
        function grabScrapInfoShot(scrapInfo, anchorName, shotName) {
            let anchor = findVisibleByName(scrapInfo, anchorName);
            verify(anchor, "found " + anchorName + " in the Scrap Info panel");
            let row = anchor.parent;
            verify(row, anchorName + " has a parent row to highlight");
            highlightOverlayId.target = row;
            settle();

            let path = WindowGrabber.grabItemToFile(scrapInfo, shotName,
                                                    scrapInfoCropMargin);
            verify(path.length > 0, "grabItemToFile wrote " + shotName);
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   shotName + " is not blank");
        }

        // Drain any jobs the navigation/tab-switch queued, then give the scene
        // graph a couple of frames to present the page (and any highlight overlay)
        // before grabbing.
        function settle() {
            RootData.futureManagerModel.waitForFinished();
            wait(150);
        }

        // Wait up to ~2s for a live QRhi on the given item's window; on a headless
        // offscreen platform there is none, so skip() and return false (the caller
        // must return). Mirrors loadRhiViewer's guard for the non-3D-view shots,
        // whose grabs are equally empty without a live render loop.
        function requireRhi(item) {
            for (let i = 0; i < 40 && !OffscreenRenderTester.windowHasRhi(item); ++i) {
                wait(50);
            }
            if (!OffscreenRenderTester.windowHasRhi(item)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return false;
            }
            return true;
        }

        // Load the with-scrap demo project, open its one note full-screen, and
        // enter Carpet mode. Returns the noteArea item, or null after skip() when
        // there's no live QRhi.
        //
        // The note is opened via the dedicated note page (Note=...), not the
        // trip's data page: the note page renders the drawing large enough to show
        // the scrap outline and its stations, whereas the trip page devotes most
        // of its width to the survey table. Carpet is entered programmatically
        // (setMode), since the note page's narrow layout hides the toolbar button
        // but not the underlying state it drives.
        //
        // isZip picks loadProjectFromZip (bundled .cwproj) vs loadProjectFromFile
        // (legacy .cw). noteAddress is the full page address of the note to open.
        function loadCarpetNote(fixture, isZip, noteAddress) {
            if (isZip) {
                TestHelper.loadProjectFromZip(RootData.project,
                    TestHelper.testcasesDatasetPath(fixture));
            } else {
                TestHelper.loadProjectFromFile(RootData.project,
                    TestHelper.testcasesDatasetPath(fixture));
            }
            RootData.futureManagerModel.waitForFinished();

            // Land on a neutral page first so a note page left over from a previous
            // shot (a different project) is torn down before we open this one — a
            // reused note page can otherwise keep a scrap view bound to the old,
            // now-unloaded project, leaving this note's scrap count stuck at 0.
            RootData.pageSelectionModel.currentPageAddress = "View";
            RootData.futureManagerModel.waitForFinished();

            RootData.pageSelectionModel.currentPageAddress = noteAddress;
            tryVerify(() => RootData.pageView.currentPageItem !== null
                && RootData.pageView.currentPageItem.objectName === "notePage");

            let notePageItem = RootData.pageView.currentPageItem;
            let noteGallery = findByName(notePageItem, "noteGallery");
            verify(noteGallery, "found the note gallery");

            if (!requireRhi(noteGallery)) { return null; }

            tryVerify(() => noteGallery.currentNote !== null, 5000, "the note is current");

            noteGallery.setMode("CARPET");
            RootData.futureManagerModel.waitForFinished();
            return notePageItem;
        }

        // Select the current note's scrap at `index` so its outline points and
        // station markers draw and the Scrap Info panel appears. Everything is
        // re-found from the current page each poll: after a project switch the note
        // page rebuilds asynchronously, so a scrap view captured earlier can go
        // stale (its note reset to null, count to 0). Poll until a live scrap view
        // — bound to a note, with enough scraps — accepts the selection.
        function selectScrap(index) {
            let selected = null;
            tryVerify(() => {
                RootData.futureManagerModel.waitForFinished();
                let page = RootData.pageView.currentPageItem;
                if (!page || page.objectName !== "notePage") { return false; }
                let gallery = findByName(page, "noteGallery");
                if (gallery && gallery.mode !== "CARPET") { gallery.setMode("CARPET"); }
                // Several scrap views may exist; only the one bound to the current
                // note (and holding enough scraps) can take the selection.
                let views = findAllByName(page, "scrapViewId", []);
                for (let i = 0; i < views.length; ++i) {
                    let sv = views[i];
                    if (!sv.note || sv.count <= index) { continue; }
                    sv.clearSelection();
                    sv.selectScrapIndex = index;
                    if (sv.selectedScrapItem !== null) {
                        selected = sv;
                        return true;
                    }
                }
                return false;
            }, 20000, "scrap " + index + " is selected");
            return selected;
        }

        // The note is a large scanned photo that renders a beat after the page
        // appears; a grab taken too early captures a blank note area. Poll until a
        // crop of the note area is non-uniform (has drawn content) before grabbing.
        // The probe image is written to the output dir and immediately removed.
        function waitForNoteRendered() {
            tryVerify(() => {
                let page = RootData.pageView.currentPageItem;
                if (!page) { return false; }
                let noteArea = findByName(page, "noteArea");
                if (!noteArea) { return false; }
                let probe = WindowGrabber.grabItemToFile(noteArea, "cw-note-render-probe", 0);
                let rendered = probe.length > 0 && OffscreenRenderTester.imageIsNonUniform(probe);
                if (probe.length > 0) { WindowGrabber.removeFile(probe); }
                return rendered;
            }, 10000, "the note image has rendered");
        }

        function cleanup() {
            highlightOverlayId.target = null;
            let tabBar = sidePanelTabBar();
            if (tabBar) { tabBar.currentIndex = 0; }
        }

        // Full-window screenshot of the 3D view: proves grabWindow() captures the
        // QML UI chrome and the live 3D scene together in one image. Backs the
        // chapter's opening overview.
        function test_view3dOverview() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let tabBar = sidePanelTabBar();
            if (tabBar) { tabBar.currentIndex = 0; }
            highlightOverlayId.target = null;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-overview");
            verify(path.length > 0, "grabToFile wrote a screenshot");
            verify(WindowGrabber.fileExists(path), "screenshot file exists");

            let size = WindowGrabber.imageSize(path);
            verify(size.width > 0 && size.height > 0, "screenshot has real dimensions");
            // UI chrome + 3D scene fill the frame — nowhere near all-black.
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "screenshot is not a blank/black frame (got "
                   + OffscreenRenderTester.nonBlackFraction(path) + ")");
        }

        // Orbit the finished carpeted cave and write one numbered PNG per
        // azimuth. gen-manual-screenshots.sh assembles the frames into
        // scraps-carpet-orbit.gif and deletes them — the animated "why" hero
        // for the Scraps / Carpeting overview (docs/manual/scraps/carpeting.md):
        // it shows the flat sketches morphed and draped on the 3D survey line,
        // turning so the 3D relief reads.
        //
        // Framing is computed ONCE, then the orbit only changes azimuth: a fixed
        // center + zoom keeps the cave the same size in every frame (framing per
        // frame would make the view pulse as the projected extent changes). To
        // ensure the fixed zoom never clips the elongated cave as it turns, the
        // fit is taken from the WIDEST rotation (the azimuth needing the most
        // zoom-out), sampled over a half-turn.
        function test_carpetOrbit() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");
            let tt = glTerrain.turnTableInteraction;
            verify(tt, "found the turn-table interaction");
            let scene = glTerrain.scene;
            verify(scene, "found the scene");

            // Everything this shot changes lives on the RootData singleton or the
            // renderer, and survives into later tests (the view-3d shots reuse the
            // default camera and chrome). Snapshot it all now and put it back at
            // the end, or those screenshots silently regenerate wrong — perspective,
            // tilted, and missing their lead markers.
            const prevScrapsVisible = RootData.regionSceneManager.scraps.visible;
            const prevLeadsVisible = RootData.leadsVisible;
            const prevStationsVisible = RootData.stationsVisible;
            const prevProjection = glTerrain.projectionTransition.progress;
            const prevViewState = tt.viewState(); // an invokable, not a property
            const prevAzimuthLocked = tt.azimuthLocked;
            const prevPitchLocked = tt.pitchLocked;

            // Carpets on; survey chrome (leads, station labels) off so the orbit
            // reads as pure morphed sketch draped on the line plot.
            RootData.regionSceneManager.scraps.visible = true;
            RootData.leadsVisible = false;
            RootData.stationsVisible = false;

            let tabBar = sidePanelTabBar();
            if (tabBar) { tabBar.currentIndex = 0; }
            highlightOverlayId.target = null;
            RootData.futureManagerModel.waitForFinished();

            let box = OffscreenRenderTester.sceneBoundingBox(scene);
            verify(!box.isNull, "scene bounding box is non-empty");
            tt.azimuthLocked = false;
            tt.pitchLocked = false;

            let pitchDeg = 55.0; // oblique tilt so the carpets' 3D relief shows

            // Fix the frame to the widest half-turn orientation. In ortho a
            // larger zoomScale is a wider frustum (more world visible), so the
            // max over the sampled azimuths is the fit that clips at no angle.
            let fit = tt.framingViewState(box, 0.0, pitchDeg);
            for (let a = 15.0; a < 180.0; a += 15.0) {
                let candidate = tt.framingViewState(box, a, pitchDeg);
                if (candidate.zoomScale > fit.zoomScale) { fit = candidate; }
            }
            tt.setViewState(fit); // sets center + zoom + pitch once

            let frameCount = 48; // 7.5 deg/frame — smooth, seamless 360 loop
            let firstPath = "";
            for (let i = 0; i < frameCount; ++i) {
                tt.azimuth = (360.0 * i) / frameCount; // orbit: azimuth only
                wait(60); // let the RHI present the new pose before grabbing
                let name = "scraps-carpet-orbit-" + (i < 10 ? "0" + i : "" + i);
                let path = WindowGrabber.grabItemToFile(regionViewer, name, 0);
                verify(path.length > 0, "wrote orbit frame " + i);
                if (i === 0) { firstPath = path; }
            }
            // The scene fills the viewport — no frame is a blank/black grab.
            verify(OffscreenRenderTester.nonBlackFraction(firstPath) > 0.5,
                   "orbit frame 0 is not blank (got "
                   + OffscreenRenderTester.nonBlackFraction(firstPath) + ")");

            // Static poster (the always-visible still in carpeting.md; the GIF
            // only animates when the reader expands it). A low-angle PERSPECTIVE
            // shot reads more 3D than the ortho orbit: the ground grid recedes to
            // a vanishing point. Switch to perspective (progress 1 = perspective,
            // 0 = ortho), tilt to a shallow pitch, frame once, grab. The orbit
            // itself stays ortho — a perspective turntable would swim in scale.
            glTerrain.projectionTransition.progress = 1.0;
            RootData.futureManagerModel.waitForFinished();
            let posterPitch = 30.0;   // degrees above horizontal: grid recedes
            let posterAzimuth = 20.0; // slight turn for a three-quarter framing
            let posterState = tt.framingViewState(box, posterAzimuth, posterPitch);
            // The perspective fit frames loosely at a low pitch (the cave floats
            // small amid empty grid); pull the eye in so it fills the frame and
            // the grid convergence reads stronger.
            posterState.distance = posterState.distance * 0.62;
            tt.setViewState(posterState);
            wait(120); // perspective settle + present
            let posterPath = WindowGrabber.grabItemToFile(
                regionViewer, "scraps-carpet-orbit-poster", 0);
            verify(posterPath.length > 0, "wrote the perspective poster");
            verify(OffscreenRenderTester.nonBlackFraction(posterPath) > 0.5,
                   "poster is not blank (got "
                   + OffscreenRenderTester.nonBlackFraction(posterPath) + ")");

            // Hand the scene back exactly as it was found (see the snapshot above).
            glTerrain.projectionTransition.progress = prevProjection;
            tt.setViewState(prevViewState);
            tt.azimuthLocked = prevAzimuthLocked;
            tt.pitchLocked = prevPitchLocked;
            RootData.regionSceneManager.scraps.visible = prevScrapsVisible;
            RootData.leadsVisible = prevLeadsVisible;
            RootData.stationsVisible = prevStationsVisible;
            RootData.futureManagerModel.waitForFinished();
        }

        // Highlighted screenshot of the camera side panel: the ring overlay must
        // appear in the grab, proving the highlight path renders before capture.
        // Backs the chapter's "Aim the camera" section.
        function test_view3dCameraControls() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let panel = findByName(rootId.mainWindow, "renderingSidePanel");
            verify(panel, "found the rendering side panel");

            let tabBar = sidePanelTabBar();
            if (tabBar) { tabBar.currentIndex = 0; }
            highlightOverlayId.target = panel;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-camera-controls");
            verify(path.length > 0, "grabToFile wrote the camera-controls screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "camera-controls screenshot is not blank");
        }

        // The Layers tab active: shows the keyword layer panel. Backs the
        // chapter's "Focus on part of the cave with layers" section.
        function test_view3dLayers() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let panel = findByName(rootId.mainWindow, "renderingSidePanel");
            verify(panel, "found the rendering side panel");

            let tabBar = sidePanelTabBar();
            verify(tabBar, "found the side-panel tab bar");
            tabBar.currentIndex = 1; // Layers
            highlightOverlayId.target = panel;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-layers");
            verify(path.length > 0, "grabToFile wrote the layers screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "layers screenshot is not blank");
        }

        // Cropped, element-focused grab: grabItemToFile crops the window grab to
        // the viewport's rect, proving the crop math against the window DPR.
        function test_view3dViewportCrop() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            highlightOverlayId.target = null;
            settle();

            // Full-window grab first, so the crop can be checked against it in the
            // same physical-pixel space (no dependence on the screen's DPR here).
            let fullPath = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-fullwindow");
            let cropPath = WindowGrabber.grabItemToFile(regionViewer, "view-3d-viewport", 0);
            verify(cropPath.length > 0, "grabItemToFile wrote a cropped screenshot");

            let fullSize = WindowGrabber.imageSize(fullPath);
            let cropSize = WindowGrabber.imageSize(cropPath);
            verify(cropSize.width > 0 && cropSize.height > 0, "crop has real dimensions");
            verify(cropSize.width <= fullSize.width && cropSize.height <= fullSize.height,
                   "crop is no larger than the full window");
            verify(cropSize.width < fullSize.width,
                   "viewport crop is narrower than the window (excludes the side panel)");
            verify(OffscreenRenderTester.nonBlackFraction(cropPath) > 0.3,
                   "cropped viewport screenshot has rendered content");

            // These two are mechanism-regression grabs, not chapter assets — don't
            // leave them behind in docs/manual/images/.
            WindowGrabber.removeFile(fullPath);
            WindowGrabber.removeFile(cropPath);
        }

        // Open Phake's trip page with the first note selected, and return its
        // note gallery (or null after skip() when there's no live QRhi). The
        // trip page is the surface for the toolbar-button shots because it keeps
        // the gallery's toolbar (isNarrow defaults false); the full-screen note
        // page hides it (isNarrow: true). Returns the gallery — not the page —
        // so button lookups can be scoped to THIS gallery: lazily-instantiated
        // note sub-pages carry their own (unselected) gallery whose toolbar
        // buttons a page-wide search would find first.
        function openTripNoteGallery() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            // Neutral page first so a stale note/trip page is torn down.
            RootData.pageSelectionModel.currentPageAddress = "View";
            RootData.futureManagerModel.waitForFinished();
            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Cave=Phake Cave 3000/Trip=Release 0.08";
            tryVerify(() => RootData.pageView.currentPageItem !== null
                && RootData.pageView.currentPageItem.objectName === "tripPage");
            let tripPageItem = RootData.pageView.currentPageItem;

            let fileLoader = findByName(rootId.mainWindow, "fileMenuButtonLoader");
            if (fileLoader) { fileLoader.active = true; }

            let gallery = findByName(tripPageItem, "noteGallery");
            verify(gallery, "found the trip page note gallery");
            if (!requireRhi(gallery)) { return null; }

            // Select the first note so its toolbar and image render.
            gallery.currentNoteIndex = 0;
            tryVerify(() => gallery.currentNote !== null, 5000, "a note is current");
            return gallery;
        }

        // The Carpet button on the TRIP page — how you actually enter Carpet
        // mode. Highlighted here rather than on the note page because the note
        // page's compact layout hides the toolbar button. Backs the "Enter
        // Carpet mode" section of docs/manual/scraps/digitize-a-scrap.md.
        function test_enterCarpetMode() {
            let gallery = openTripNoteGallery();
            if (!gallery) { return; }

            let carpetButton = null;
            tryVerify(() => {
                carpetButton = findVisibleByName(gallery, "carpetButtonId");
                return carpetButton !== null;
            }, 3000, "the Carpet button is visible");

            highlightOverlayId.target = carpetButton;
            waitForNoteRendered();
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "scraps-enter-carpet");
            verify(path.length > 0, "grabToFile wrote the enter-carpet screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "enter-carpet screenshot is not blank");
        }

        // Highlight one button of the carpet toolbar's Add group on the trip page
        // and grab the window. Shared by the Scrap / Station / Lead shots, which
        // differ only in which button they ring. @a buttonName is the button's
        // objectName; @a shotName the output PNG basename.
        function grabCarpetToolbarShot(buttonName, shotName) {
            let gallery = openTripNoteGallery();
            if (!gallery) { return; }

            gallery.setMode("CARPET"); // reveal the carpet toolbar (Back/Select/Add)
            RootData.futureManagerModel.waitForFinished();

            let button = null;
            tryVerify(() => {
                button = findVisibleByName(gallery, buttonName);
                return button !== null;
            }, 3000, buttonName + " is visible");

            highlightOverlayId.target = button;
            waitForNoteRendered();
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, shotName);
            verify(path.length > 0, "grabToFile wrote " + shotName);
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   shotName + " is not blank");
        }

        // The Scrap button — clicked to start tracing an outline. Backs the
        // "Trace the outline" section of docs/manual/scraps/digitize-a-scrap.md.
        function test_traceOutline() {
            grabCarpetToolbarShot("addScrapButton", "scraps-trace-outline");
        }

        // The Station button — clicked to drop survey stations on the drawing.
        // Backs the "Place the stations" section of digitize-a-scrap.md.
        function test_placeStations() {
            grabCarpetToolbarShot("addScrapStation", "scraps-place-stations");
        }

        // The Lead button — clicked to mark an unexplored passage on the note.
        // Backs the "Mark leads" section of digitize-a-scrap.md.
        function test_addLead() {
            grabCarpetToolbarShot("addLeads", "scraps-add-lead");
        }

        // Both scrap note shots from a SINGLE note-page visit. Grabbing them in
        // one function is deliberate: the note page renders its image only on the
        // first visit within a process — a second visit (a separate test that
        // re-opens a note page) grabs a blank note area — so both grabs must share
        // one visit. Uses Phake Cave 3000, whose clean geometric scraps read more
        // clearly than a real field sketch.
        //   - scraps-digitize.png: scrap 1 (the lower passage) selected, Scrap Info
        //     collapsed, no highlight — the outline points + stations on the note.
        //     Backs docs/manual/scraps/digitize-a-scrap.md.
        //   - scraps-type-editor.png: scrap 0 (the upper passage) selected, Scrap
        //     Info expanded + highlighted. Backs docs/manual/scraps/scrap-types.md.
        function test_scrapNoteShots() {
            let page = loadCarpetNote("test_cwProject/Phake Cave 3000.cw", false,
                "Source/Data/Cave=Phake Cave 3000/Trip=Release 0.08/Note=001");
            if (!page) { return; }

            // --- Digitize shot ---
            selectScrap(1);
            let scrapInfo = findScrapInfoEditor(RootData.pageView.currentPageItem);
            if (scrapInfo) { scrapInfo.collapsed = true; }
            highlightOverlayId.target = null;
            waitForNoteRendered();
            settle();

            let digPath = WindowGrabber.grabToFile(rootId.mainWindow, "scraps-digitize");
            verify(digPath.length > 0, "grabToFile wrote the digitize screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(digPath) > 0.3,
                   "digitize screenshot is not blank");

            // --- Scrap type editor shot (same visit, different scrap) ---
            selectScrap(0);
            scrapInfo = findScrapInfoEditor(RootData.pageView.currentPageItem);
            verify(scrapInfo, "found the Scrap Info editor");
            scrapInfo.collapsed = false;
            highlightOverlayId.target = scrapInfo;
            settle();

            let typePath = WindowGrabber.grabToFile(rootId.mainWindow, "scraps-type-editor");
            verify(typePath.length > 0, "grabToFile wrote the type-editor screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(typePath) > 0.3,
                   "type-editor screenshot is not blank");

            // --- Scrap Info control shots (same visit, same scrap) ---
            // Auto Calculate must be off first: it hides the north and scale tool
            // buttons outright (`enable: !checkableBoxId.checked` aliases
            // setNorthButton.visible) and makes their fields read-only, so the
            // shots documenting manual entry would otherwise show a stripped-down
            // panel. Grabbed after the type-editor shot above, which wants the
            // scrap's stored type and Auto Calculate state untouched.
            let scrap = scrapInfo.scrap;
            verify(scrap, "the Scrap Info editor is bound to a scrap");
            scrap.calculateNoteTransform = false;
            settle();

            grabScrapInfoShot(scrapInfo, "setNorthButton", "scraps-north-up");
            grabScrapInfoShot(scrapInfo, "setLengthButton", "scraps-scale");

            // The azimuth row is a projected-profile-only control
            // (`visible: upInputId.scrapType == Scrap.ProjectedProfile`), so the
            // scrap has to change type before it can be shown at all.
            scrap.type = Scrap.ProjectedProfile;
            tryVerify(() => findVisibleByName(scrapInfo, "directionComboBox") !== null,
                      5000, "the azimuth row is visible on a projected profile");
            grabScrapInfoShot(scrapInfo, "directionComboBox", "scraps-azimuth");

            highlightOverlayId.target = null;
        }

        // The Automatic Update checkbox in the lower-left sidebar, highlighted
        // and cropped. Backs the "Carpeting is automatic" section of
        // docs/manual/scraps/carpeting.md — the switch gates the survey solve
        // (loop closure) and carpet re-morphing together.
        function test_automaticUpdate() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let container = findByName(rootId.mainWindow, "autoUpdateContainer");
            verify(container, "found the Automatic Update container");

            let tabBar = sidePanelTabBar();
            if (tabBar) { tabBar.currentIndex = 0; }
            highlightOverlayId.target = container;
            settle();

            // Crop to the checkbox corner with a margin so the highlight ring and
            // the "Automatic Update" label + box are framed without the rest of
            // the window.
            let path = WindowGrabber.grabItemToFile(container, "scraps-automatic-update", 40);
            verify(path.length > 0, "grabItemToFile wrote the automatic-update screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "automatic-update screenshot is not blank");
        }

        // The Warping and Morphing settings tab, highlighted. Global settings,
        // always present. Backs docs/manual/scraps/warping-settings.md.
        function test_warpingSettings() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            RootData.pageSelectionModel.currentPageAddress = "Settings";

            let fileLoader = findByName(rootId.mainWindow, "fileMenuButtonLoader");
            if (fileLoader) { fileLoader.active = true; }

            let tabBar = findByName(rootId.mainWindow, "settingsTabBar");
            tryVerify(() => tabBar !== null, 3000, "found the settings tab bar");

            if (!requireRhi(tabBar)) { return; }

            tabBar.currentIndex = 1; // Warping

            let warping = findByName(rootId.mainWindow, "warpingSettingsItem");
            tryVerify(() => warping !== null, 3000, "found the warping settings item");

            // Pin the shipping (Release) defaults so the shot matches the values
            // documented in warping-settings.md. Debug builds use coarser grid /
            // shot-interpolation defaults (2.5 m), which real users never see.
            let ws = RootData.scrapManager.warpingSettings;
            ws.gridResolutionMeters = 0.5;
            ws.shotInterpolationSpacingMeters = 0.5;

            highlightOverlayId.target = warping;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "scraps-warping-settings");
            verify(path.length > 0, "grabToFile wrote the warping-settings screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "warping-settings screenshot is not blank");
        }
    }
}
