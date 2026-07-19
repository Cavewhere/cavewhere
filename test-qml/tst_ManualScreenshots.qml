import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Controls // unaliased: lets a test set the SplitView.* attached properties by name
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder
import QQuickGit

// User-manual screenshot harness (Deliverable B). Doubles as a regression test:
// under the headless `offscreen` QPA it skips (no live QRhi, so grabWindow()
// cannot capture the 3D view); on a GPU-backed platform it drives the real full
// UI, navigates to each documented page, and writes a PNG via WindowGrabber.
//
// Run it as the generator with scripts/gen-manual-screenshots.sh, which points
// CW_MANUAL_IMAGE_DIR at docs/manual/images/. Without that variable the grabs
// land in a temp dir, so a plain test run never writes into the source tree.
//
// Shots here back the 3D View chapter (docs/manual/view-3d/), the Scraps /
// Carpeting chapter (docs/manual/scraps/), and the Notes chapter
// (docs/manual/notes/) — the sharpest test of grabWindow(), since each grab must
// contain the QML UI chrome and the live RHI scene together, which the offscreen
// renderer cannot produce.
MainWindowTest {
    id: rootId

    HighlightOverlay {
        id: highlightOverlayId
        anchors.fill: parent
        z: 1000
        target: null
    }

    // The first-run setup page, which this harness cannot reach the way a user
    // does. The real app swaps MainContent out for WelcomePage while
    // RootData.account is invalid (CavewhereMainWindow.qml), but MainWindowTest
    // hosts MainContent directly, so that gate isn't here. Stage the page over the
    // window instead and grab the loader itself.
    //
    // The account is a private, empty Person rather than RootData.account: the
    // page is only shown when the account is invalid, and clearing the shared one
    // to stage that would change what every later shot is looking at (a commit
    // needs an identity — see test_gitHistory).
    //
    // Sized to CavewhereMainWindow.qml's own default window rather than filling
    // this larger one. The page centers a fixed-width column in whatever space it
    // is given, so at 1200x700 it is a third of a screen of content in a field of
    // white — scaled into the manual's ~512px column, the text stops being
    // readable. At the app's default size the shot is the window a user opens on.
    //
    // The Rectangle is what keeps the grab clean. WelcomePage draws no background
    // of its own — in the app it sits on the window's — and grabItemToFile crops
    // out of a whole-window grab, so without an opaque one beneath it MainContent
    // shows through inside the crop. It also carries the palette that
    // CavewhereMainWindow.qml sets and MainWindowTest.qml does not, which the name
    // and email placeholders are drawn with; Fusion's default placeholder color is
    // not the one users see, and those two fields are the point of the shot.
    QQ.Loader {
        id: welcomePageLoaderId
        anchors.centerIn: parent
        width: 1024
        height: 576
        active: false
        z: 999
        sourceComponent: QQ.Rectangle {
            color: Theme.background
            palette.placeholderText: Theme.textSubtle

            WelcomePage {
                objectName: "welcomePage"
                anchors.fill: parent
                account: Person {}
            }
        }
    }

    TestCase {
        name: "ManualScreenshots"
        when: windowShown

        // Breathing room left around a floating panel (Scrap Info, Image Info)
        // when cropping a control shot out of the full-window grab.
        readonly property int panelCropMargin: 12

        // Wider margin for the LiDAR transform panel, so the crop keeps enough of
        // the scan behind it to read as a 3D view rather than a floating form.
        readonly property int lidarPanelCropMargin: 60

        // How many times parkSurveyEditorAtTop re-parks the survey editor before
        // giving up, and how far the view may sit from its header and still count
        // as parked. Each attempt costs a settle(), and in practice the header has
        // stopped growing by the second.
        readonly property int maxParkAttempts: 5
        readonly property real parkTolerance: 0.5

        // Margin around an open error message, whose crop is anchored on the
        // message's text. Like the caret menu below, the message hangs off the
        // cell it belongs to, so padding the text reaches back over the cell and
        // frames both without dragging in the whole editor.
        readonly property int errorQuoteCropMargin: 100

        // Margin around the open caret menu, whose crop is anchored on the menu
        // rather than on the Distance cell it belongs to. The menu opens down and
        // to the right and is twice the cell's width, so a cell-anchored crop
        // needs a ~180px margin to clear the menu's far corner — and that same
        // margin applied leftward and upward drags in the sidebar and the note
        // gallery. Anchored on the menu, this reaches back over the cell and keeps
        // the shot to the table. Sized to land just past the column titles: wider
        // starts slicing the Data heading above them in half.
        readonly property int excludeMenuCropMargin: 80

        // Margin around the open Import / Export menu, anchored on the menu's
        // content. These are simple dropdowns near the top of the page, not hung
        // off a table cell, so a small even margin frames them without dragging in
        // the page behind.
        readonly property int importExportMenuCropMargin: 40

        // The identity the Git History shot commits under. The demo trip's surveyor,
        // so the author column agrees with survey-team.png.
        readonly property string gitHistoryShotAuthor: "Philip Schuchardt"
        readonly property string gitHistoryShotEmail: "philip@cavewhere.com"

        // The Save As dialog is grabbed on its own background, which already spans
        // the whole dialog (title, content and buttons), so this only needs to keep
        // the frame off the edge of the crop.
        readonly property int saveAsCropMargin: 12

        // Stands in for the random placeholder name a new project gets, so the Save
        // As shot reads the same every run. The demo cave's name, matching the rest
        // of the manual — and it must be a name that does NOT already exist in the
        // dialog's default Location (the Desktop), or the shot captures the red
        // "a folder already exists" error instead. test_saveAsDialog asserts that.
        readonly property string saveAsShotProjectName: "Phake Cave 3000"

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

        // Highlight one control row inside a floating panel (Scrap Info, Image
        // Info) and grab the panel cropped tight around it. The panel is a small
        // floating box in a 2400x1400 window, so a full-window grab renders it too
        // small to read in the manual; grabItemToFile grabs the whole window and
        // *then* crops, so the highlight ring and its dim scrim still land in the
        // crop.
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
        function grabPanelRowShot(panel, anchorName, shotName) {
            let anchor = findVisibleByName(panel, anchorName);
            verify(anchor, "found " + anchorName + " in the panel");
            let row = anchor.parent;
            verify(row, anchorName + " has a parent row to highlight");
            grabPanelItemShot(panel, row, shotName);
        }

        // Ring `item` and grab `panel` cropped around it. Use directly to point at
        // a single control; grabPanelRowShot wraps it to point at a whole row.
        function grabPanelItemShot(panel, item, shotName) {
            highlightOverlayId.target = item;
            settle();

            let path = WindowGrabber.grabItemToFile(panel, shotName,
                                                    panelCropMargin);
            verify(path.length > 0, "grabItemToFile wrote " + shotName);
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   shotName + " is not blank");
        }

        // Drain any jobs the navigation/tab-switch queued, then give the scene
        // graph a couple of frames to present the page (and any highlight
        // overlay) before grabbing. Re-aims the highlight ring last: its rect is
        // computed from the target's position, which only means anything once the
        // page has finished laying out.
        function settle() {
            RootData.futureManagerModel.waitForFinished();
            wait(150);
            highlightOverlayId.refresh();
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

        // The note is a large scanned photo loaded through the image provider, so
        // it decodes a beat after the page appears and a grab taken too early
        // captures a blank note area.
        //
        // Wait on the Image's own status rather than probing pixels. An earlier
        // version cropped the note area and waited for it to be "non-uniform",
        // which silently reported success as soon as ANY content was in the crop —
        // the floating toolbar sitting over the note area is enough — so the grab
        // could still race a decoding scan. That made whichever note shot happened
        // to lose the race come out blank, varying from run to run.
        // ImageBackground binds `visible: status === Image.Ready`, so Ready is
        // exactly the point the note becomes paintable; callers settle() afterwards
        // to give it a frame.
        function waitForNoteRendered() {
            tryVerify(() => {
                let page = RootData.pageView.currentPageItem;
                if (!page) { return false; }
                let noteArea = findVisibleByName(page, "noteArea");
                if (!noteArea || !noteArea.image) { return false; }
                return noteArea.image.status === QQ.Image.Ready;
            }, 20000, "the note image finished loading");
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

        // The View tab switched to Perspective, so the Projection group's
        // Field of View row (hidden in Orthogonal) is showing. Backs the "Set the
        // Field of View" section of docs/manual/view-3d/perspective-and-field-of-view.md.
        function test_view3dFieldOfView() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            // projectionTransition lives on the GLTerrainRenderer (objectName
            // "renderer"), not the RegionViewer that loadRhiViewer returns.
            let glTerrain = findByName(rootId.mainWindow, "renderer");
            verify(glTerrain && glTerrain.projectionTransition,
                   "found the terrain renderer and its projection transition");

            let panel = findByName(rootId.mainWindow, "renderingSidePanel");
            verify(panel, "found the rendering side panel");

            let tabBar = sidePanelTabBar();
            if (tabBar) { tabBar.currentIndex = 0; } // View tab

            // Settle the projection onto perspective (progress 1). The Field of
            // View row is bound visible to perspectiveProjection.enabled.
            glTerrain.projectionTransition.progress = 1;
            tryVerify(function() { return regionViewer.perspectiveProjection.enabled; },
                      5000, "perspective projection is active");

            // Move the toggle's handle to the Perspective end too, so the shot is
            // self-consistent (a handle stuck at Orthognal while the Field of View
            // row shows would read as a bug). ToggleSlider.sliderPos is read-only —
            // computed from the internal button's x — so nudge the button image,
            // which also drives progress back through the normal binding.
            let slider = findByName(panel, "projectionSlider");
            verify(slider, "found the projection slider");
            let toggle = findByName(slider, "slider");
            verify(toggle, "found the toggle inside the projection slider");
            let kids = toggle.children;
            for (let i = 0; kids && i < kids.length; ++i) {
                if (kids[i].source !== undefined
                        && String(kids[i].source).indexOf("buttonSlider") !== -1) {
                    kids[i].x = toggle.sliderRange;
                    break;
                }
            }
            tryVerify(function() { return toggle.sliderPos >= 1.0; }, 2000,
                      "the projection toggle reached the Perspective end");

            // The View tab scrolls; bring the Projection group at its foot — where
            // the Field of View row now sits — into the panel's viewport.
            let flick = slider;
            while (flick && flick.contentY === undefined) { flick = flick.parent; }
            if (flick) { flick.contentY = Math.max(0, flick.contentHeight - flick.height); }

            highlightOverlayId.target = slider.parent; // slider + Field of View + help
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-field-of-view");
            verify(path.length > 0, "grabToFile wrote the field-of-view screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "field-of-view screenshot is not blank");

            // Hygiene: return to orthogonal so a later 3D shot in a full run starts
            // from the default projection (and gets its scale bar back).
            glTerrain.projectionTransition.progress = 0;
        }

        // The Cavern Output page: the solve summary on the Cavern log tab (with its
        // loop count), and the Loop closure tab where Survex's per-traverse error
        // report appears for a cave that has loops. The demo cave is a single line
        // with no loops, so that tab is correctly disabled here — which is itself
        // what the manual describes. Backs docs/manual/loop-closure/.
        function test_loopClosureReport() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            // The solve runs during load; its log feeds cavernLog. Navigate to the
            // page the way the Data page's region menu reaches it.
            RootData.pageSelectionModel.gotoPageByName(null, "Cavern");
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "cavernOutputPage",
                      5000, "the Cavern Output page is current");
            let page = RootData.pageView.currentPageItem;

            tryVerify(() => RootData.linePlotManager.cavernLog.length > 0, 10000,
                      "cavern produced a solve log");

            // Cavern log tab (index 0): shows "There are N loops" and the length
            // summary. The Loop closure tab (index 1) stays disabled because the
            // demo cave has no loops — the manual explains that state.
            let tabBar = findByName(page, "outputTabBar");
            verify(tabBar, "found the output tab bar");
            tabBar.currentIndex = 0;

            highlightOverlayId.target = tabBar;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "loop-closure-summary");
            verify(path.length > 0, "grabToFile wrote the loop-closure screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "loop-closure screenshot is not blank");
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

        // Tick only `value` in a KeywordGroupByKeyModel column, unticking the rest —
        // the model-level equivalent of clicking one value's checkbox in KeywordTab.
        function checkOnlyValue(group, value) {
            for (let r = 0; r < group.rowCount(); ++r) {
                let idx = group.index(r, 0);
                let v = group.data(idx, KeywordGroupByKeyModel.ValueRole);
                group.setData(idx, v === value, KeywordGroupByKeyModel.AcceptedRole);
            }
        }

        // First value in a column that isn't the "Others" catch-all, or "".
        function firstRealValue(group) {
            for (let r = 0; r < group.rowCount(); ++r) {
                let v = group.data(group.index(r, 0), KeywordGroupByKeyModel.ValueRole);
                if (v !== "Others") { return v; }
            }
            return "";
        }

        // A built-up filter: two AND columns drilling down (Type -> Plan, then a
        // Caver column showing only the cavers among those plan scraps, one ticked).
        // Backs the "Drill down: add an AND column" section of
        // docs/manual/view-3d/layers-and-keywords.md, whose sibling shot
        // (view-3d-layers) shows the single-column starting state.
        function test_view3dLayersDrilldown() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let panel = findByName(rootId.mainWindow, "renderingSidePanel");
            verify(panel, "found the rendering side panel");

            let tabBar = sidePanelTabBar();
            verify(tabBar, "found the side-panel tab bar");
            tabBar.currentIndex = 1; // Layers

            let pipeline = RootData.keywordFilterPipelineModel;
            verify(pipeline, "found the keyword filter pipeline model");
            verify(pipeline.rowCount() >= 1, "pipeline has its default first column");

            // Column 1: group by Type, show only Plan scraps.
            let typeGroup = pipeline.data(pipeline.index(0, 0),
                KeywordFilterPipelineModel.FilterModelObjectRole);
            verify(typeGroup, "found the first column's group-by model");
            typeGroup.key = "Type";
            tryVerify(function() { return typeGroup.rowCount() > 0; }, 5000,
                      "the Type column populated its values");
            checkOnlyValue(typeGroup, "Plan");

            // Column 2: drill into those plan scraps by Caver.
            pipeline.insertRow(1);
            verify(pipeline.rowCount() >= 2, "the second column was inserted");
            let caverGroup = pipeline.data(pipeline.index(1, 0),
                KeywordFilterPipelineModel.FilterModelObjectRole);
            verify(caverGroup, "found the second column's group-by model");
            caverGroup.key = "Caver";
            tryVerify(function() { return caverGroup.rowCount() > 0; }, 5000,
                      "the Caver column populated from the plan scraps");
            let caver = firstRealValue(caverGroup);
            if (caver.length > 0) { checkOnlyValue(caverGroup, caver); }

            // Widen the side panel so both AND columns are fully visible — the same
            // thing a user does by dragging the SplitView handle. The default 320px
            // panel fits only one 220px column, clipping the second mid-word.
            let renderer = findByName(rootId.mainWindow, "renderer");
            let viewPageItem = findByName(rootId.mainWindow, "viewPage");
            if (renderer && viewPageItem) {
                let rendererWrapper = renderer.parent;
                let splitView = rendererWrapper.parent;
                let panelHost = null;
                for (let i = 0; i < splitView.children.length; ++i) {
                    let c = splitView.children[i];
                    if (c !== rendererWrapper && findByName(c, "renderingSidePanel")) {
                        panelHost = c;
                        break;
                    }
                }
                if (panelHost) {
                    // Make the renderer the fill item and give the panel a wider
                    // preferred width, so SplitView lays both AND columns out fully.
                    rendererWrapper.SplitView.fillWidth = true;
                    panelHost.SplitView.preferredWidth = 480;
                }
            }

            highlightOverlayId.target = panel;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-layers-drilldown");
            verify(path.length > 0, "grabToFile wrote the drilldown screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "drilldown screenshot is not blank");

            // Hygiene: reset the filter and hand the next shot the demo on a neutral
            // page (this shot leaves a two-column filter that would otherwise persist).
            pipeline.clear();
            restoreDemoProject();
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

        // Park the trip page's survey editor at its header, so a grab of that
        // page shows the trip's name and calibration rather than a mid-scroll
        // position. Nothing in the app sets this: the editor opens wherever the
        // previous test left it, which made every trip-page shot depend on run
        // order — and come out scrolled to Front Sights when run alone.
        //
        // Call this LAST, right before the grab, and note that one call does not
        // settle it. positionViewAtBeginning() parks the view by setting contentY
        // to originY, but the editor's header keeps growing afterwards: the note
        // thumbnail decodes, and the trip's warning banner appears once the error
        // model has run. Anything that grows ABOVE the viewport moves originY
        // without moving contentY, so the view silently ends up ~190px below the
        // top it was just sent to — with the trip's name and banner scrolled off.
        // Re-park until it stays put; once the header stops growing, one park
        // sticks and the loop exits on the check rather than the retry.
        // Resolve something inside the *live* trip page. A page the tests have
        // navigated away from is not destroyed at once, so the window can briefly
        // hold two items called "tripPage" — the outgoing one and the current one.
        // findObjectByChain gathers its matches in a QSet, so with both in the tree
        // it returns an arbitrary one; take the stale one and everything inside it
        // is invisible, and every findVisibleByName below it returns null. Searching
        // from currentPageItem — by definition the live page — puts the stale one out
        // of scope. The chain stays absolute: findObjectByChain matches it against
        // each candidate's path from the root, so it must start at rootId no matter
        // where the search begins.
        function findInTripPage(chain) {
            let tripPage = RootData.pageView.currentPageItem;
            verify(tripPage && tripPage.objectName === "tripPage",
                   "the trip page is the current page");
            return ObjectFinder.findObjectByChain(tripPage, chain);
        }

        function parkSurveyEditorAtTop() {
            let view = findInTripPage("rootId->tripPage->surveyEditor->view");
            verify(view, "found the survey editor's list view");

            let parked = false;
            for (let i = 0; i < maxParkAttempts && !parked; ++i) {
                view.positionViewAtBeginning();
                settle();
                parked = Math.abs(view.contentY - view.originY) < parkTolerance;
            }
            verify(parked, "the survey editor stayed parked at its header");
        }

        // Park `view` with data row `index` at the top of the viewport. Same
        // re-assert loop, and the same reason for it, as parkSurveyEditorAtTop:
        // positionViewAtIndex parks the row by setting contentY to the row's y,
        // and a header that grows afterwards moves the row without moving
        // contentY. Use this instead of relying on where a cell happens to sit —
        // the editor scrolls a focused cell into view with ListView.Contain, which
        // does nothing at all when the cell is already visible, so an unparked
        // cell lands wherever the previous shot left the editor scrolled.
        function parkSurveyEditorAtRow(view, index) {
            let parked = false;
            for (let i = 0; i < maxParkAttempts && !parked; ++i) {
                view.positionViewAtIndex(index, QQ.ListView.Beginning);
                settle();
                let row = view.itemAtIndex(index);
                parked = row !== null
                    && Math.abs(row.y - view.contentY) < parkTolerance;
            }
            verify(parked, "the survey editor parked row " + index + " at the top");
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
            parkSurveyEditorAtTop();

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
            parkSurveyEditorAtTop();

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

        // Every note-page shot from a SINGLE note-page visit. Grabbing them in
        // one function is deliberate: the note page renders its image only on the
        // first visit within a process — a second visit (a separate test that
        // re-opens a note page) grabs a blank note area — so all grabs must share
        // one visit. Uses Phake Cave 3000, whose clean geometric scraps read more
        // clearly than a real field sketch.
        //   - scraps-digitize.png: scrap 1 (the lower passage) selected, Scrap Info
        //     collapsed, no highlight — the outline points + stations on the note.
        //     Backs docs/manual/scraps/digitize-a-scrap.md.
        //   - scraps-type-editor.png: scrap 0 (the upper passage) selected, Scrap
        //     Info expanded + highlighted. Backs docs/manual/scraps/scrap-types.md.
        //   - scraps-north-up / scraps-scale / scraps-azimuth: one Scrap Info row
        //     each. Back docs/manual/scraps/scrap-types.md.
        //   - notes-image-info.png: the Image Info (DPI) row. Backs
        //     docs/manual/notes/note-resolution.md — and has to live here rather
        //     than in its own Notes test for the first-visit reason above.
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

            grabPanelRowShot(scrapInfo, "setNorthButton", "scraps-north-up");
            grabPanelRowShot(scrapInfo, "setLengthButton", "scraps-scale");

            // The azimuth row is a projected-profile-only control
            // (`visible: upInputId.scrapType == Scrap.ProjectedProfile`), so the
            // scrap has to change type before it can be shown at all.
            scrap.type = Scrap.ProjectedProfile;
            tryVerify(() => findVisibleByName(scrapInfo, "directionComboBox") !== null,
                      5000, "the azimuth row is visible on a projected profile");
            grabPanelRowShot(scrapInfo, "directionComboBox", "scraps-azimuth");

            // --- Image Info / DPI shot (same visit) ---
            // NoteResolution only renders while the note is in carpet mode
            // (`visible: noteArea.scrapsVisible`), which loadCarpetNote has already
            // entered. The note page is a narrow layout, and the panel binds
            // `collapsed: noteArea.isNarrow`, so it arrives collapsed to its title
            // bar and has to be opened before the row exists to highlight.
            let noteResolution = findVisibleByName(RootData.pageView.currentPageItem,
                                                   "noteResolution");
            verify(noteResolution, "found the Image Info panel");
            noteResolution.collapsed = false;
            tryVerify(() => findVisibleByName(noteResolution, "setResolution") !== null,
                      5000, "the Image Resolution row is visible once expanded");
            grabPanelRowShot(noteResolution, "setResolution", "notes-image-info");

            // The resolution tool button on its own, for the "Measure the
            // resolution off the page" section of note-resolution.md — the row
            // shot above rings the whole row, which doesn't say which control
            // starts the tool.
            let measureButton = findVisibleByName(noteResolution, "setResolution");
            verify(measureButton, "found the resolution tool button");
            grabPanelItemShot(noteResolution, measureButton, "notes-measure-resolution");

            highlightOverlayId.target = null;
        }

        // The Add (+) button on the trip's Notes section, highlighted. Backs the
        // "Add a note" section of docs/manual/notes/add-a-note.md.
        //
        // The button is ringed *closed* rather than with its menu open: the menu is
        // a QC.Menu, whose popup type is chosen by the style and defaults to a
        // separate window (Popup.Window) when the style doesn't set one — which
        // grabWindow(mainWindow) cannot capture — and popup() positions it at the
        // mouse cursor, which a test doesn't control. The two menu items are
        // spelled out in the page's prose instead, per AUTHORING.md's rule that
        // the text carries the content.
        function test_addNoteButton() {
            let gallery = openTripNoteGallery();
            if (!gallery) { return; }

            // The button lives in the SurveyEditor's Notes SectionHeader, not in
            // the gallery, so search the whole trip page.
            let addButton = null;
            tryVerify(() => {
                addButton = findVisibleByName(RootData.pageView.currentPageItem,
                                              "addNoteMenuButton");
                return addButton !== null;
            }, 3000, "the Add note button is visible");

            highlightOverlayId.target = addButton;
            waitForNoteRendered();
            settle();
            parkSurveyEditorAtTop();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "notes-add-button");
            verify(path.length > 0, "grabToFile wrote the add-note screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "add-note screenshot is not blank");
            highlightOverlayId.target = null;
        }

        // The Rotate button, ringed, with the note showing one 90° turn. Backs the
        // "Rotate a note" section of docs/manual/notes/add-a-note.md.
        //
        // Rotates for real (a click, not a poked property) so the shot shows what
        // the button actually does — and waits out the 90° PropertyAnimation, since
        // a grab taken on the click captures the note mid-spin at a meaningless
        // angle. The wide toolbar's Rotate button shares the objectName with the
        // narrow toolbar's, hence findVisibleByName.
        function test_rotateNote() {
            let gallery = openTripNoteGallery();
            if (!gallery) { return; }

            waitForNoteRendered();

            let rotateButton = null;
            tryVerify(() => {
                rotateButton = findVisibleByName(gallery, "rotateButton");
                return rotateButton !== null;
            }, 3000, "the Rotate button is visible");

            let note = gallery.currentNote;
            verify(note, "the gallery has a current note");
            compare(note.rotate, 0, "the demo note starts unrotated");

            mouseClick(rotateButton);
            tryVerify(() => Math.abs(note.rotate - 90) < 0.01, 5000,
                      "the rotation animation finished on 90 degrees");

            highlightOverlayId.target = rotateButton;
            settle();
            parkSurveyEditorAtTop();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "notes-rotate");
            verify(path.length > 0, "grabToFile wrote the rotate screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "rotate screenshot is not blank");
            highlightOverlayId.target = null;
        }

        // Put the demo project and a neutral page back, for the shots that run after.
        // Hygiene, not a fix for anything: a shot that swaps the project out
        // (test_saveAsDialog, test_gitHistory) or parks on a page outside the
        // View/Data flow (test_recentProjects) should hand the next shot the same
        // app state it inherited, rather than making it depend on run order.
        function restoreDemoProject() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.pageSelectionModel.currentPageAddress = "View";
            RootData.futureManagerModel.waitForFinished();
        }

        // Save the project and wait for the commit to land. Save is asynchronous and
        // clears `modified` only once the commit is written, so that flag is the
        // signal — waitForFinished alone returns before the queued commit is done.
        function saveAndCommit() {
            verify(RootData.project.canSaveDirectly,
                   "the project can be saved without a Save As dialog");
            verify(RootData.project.save(), "save() was accepted");
            RootData.futureManagerModel.waitForFinished();
            tryVerify(() => !RootData.project.modified, 10000,
                      "the save finished and committed");
        }

        // Add a trip to `cave` at `index` and name it. cwCave exposes no tripCount to
        // QML, so the index is asserted against the pinned demo fixture instead.
        function addTripNamed(cave, index, name) {
            cave.addTrip();
            let trip = cave.trip(index);
            verify(trip, "the new trip landed at index " + index);
            trip.name = name;
        }

        // The Git History page — the versions that Save creates. Backs "What Save
        // actually does" in docs/manual/projects-and-files/save-a-project.md.
        //
        // NOT byte-reproducible, and cannot be: the page shows real commit hashes
        // and real clock times, so every regeneration differs from the last. Don't
        // treat a diff here as a failure, and don't re-render it without a reason.
        function test_gitHistory() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            // A commit needs an identity and the harness starts from cleared
            // settings, so without this every save fails with "Git account is not
            // configured". The demo trip's own surveyor, matching survey-team.png.
            RootData.account.name = gitHistoryShotAuthor;
            RootData.account.email = gitHistoryShotEmail;

            // A one-row history illustrates nothing about versions accumulating, and
            // the freshly-converted demo has only its first save. So do a little work
            // between saves, the way an evening of data entry would. The commits all
            // share a second, which doesn't show: the list column is date-only, and
            // the graph is ordered by parentage rather than clock.
            saveAndCommit();

            let cave = RootData.region.cave(0);
            verify(cave, "the demo cave is loaded");
            compare(cave.name, "Phake Cave 3000", "the demo cave is the expected one");

            addTripNamed(cave, 1, "Release 0.09");
            saveAndCommit();
            addTripNamed(cave, 2, "Release 0.10");
            saveAndCommit();

            RootData.pageSelectionModel.gotoPageByName(null, "History");
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "gitHistoryPage",
                      5000, "the Git History page is current");
            RootData.futureManagerModel.waitForFinished();
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "project-git-history");
            verify(path.length > 0, "grabToFile wrote the git-history screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "git-history screenshot is not blank");

            restoreDemoProject();
        }

        // The before/after image comparison on the History page. Backs "Seeing what
        // a version changed" in docs/manual/collaboration/review-history.md.
        //
        // A real same-path "Modified" image diff needs the note's on-disk image file
        // to change between two commits — only a file overwrite does that (a
        // re-import writes a NEW filename, i.e. Added). So: commit a baseline holding
        // the original note image, overwrite that file with a different raster,
        // commit again, then open the History page's image diff for it. The compare
        // page renders the before from the parent commit and the after from HEAD via
        // the git image provider, so both panes must finish loading before the grab.
        function test_gitImageCompare() {
            let notePage = loadCarpetNote("test_cwProject/Phake Cave 3000.cw", false,
                "Source/Data/Cave=Phake Cave 3000/Trip=Release 0.08/Note=001");
            if (!notePage) { return; }

            // Commits need an identity; cleared settings otherwise fail the commit.
            RootData.account.name = gitHistoryShotAuthor;
            RootData.account.email = gitHistoryShotEmail;

            let gallery = findByName(notePage, "noteGallery");
            verify(gallery, "found the note gallery");
            let note = gallery.currentNote;
            verify(note, "the raster note is current");

            // Baseline commit: guarantees the ORIGINAL image is in history, so the
            // before pane has something to show.
            RootData.futureManagerModel.waitForFinished();
            RootData.project.safeCommitAll("Baseline for image diff", "");
            RootData.futureManagerModel.waitForFinished();

            let abs = RootData.project.absolutePath(note);
            let root = RootData.project.repository.directoryPath;
            verify(abs.length > 0 && abs.startsWith(root),
                   "the note image path is inside the repository");
            let rel = abs.substring(root.length + 1);

            // Overwrite the note image with a different raster, then commit it — a
            // same-path Modified diff. crashMap.png sits beside the demo dataset.
            verify(TestHelper.copyFile(
                       TestHelper.testcasesDatasetPath("test_cwProject/crashMap.png"), abs),
                   "overwrote the note image on disk");
            RootData.project.repository.checkStatusAsync();
            tryVerify(() => RootData.project.repository.modifiedFileCount > 0, 5000,
                      "the overwritten image shows as a working-tree change");
            RootData.project.safeCommitAll("Update note image", "screenshot fixture");
            RootData.futureManagerModel.waitForFinished();
            tryVerify(() => RootData.project.repository.modifiedFileCount === 0, 5000,
                      "the image change is committed");

            let headOid = TestHelper.projectHeadCommitOid(RootData.project);
            verify(headOid.length === 40, "have the head commit oid");

            RootData.pageSelectionModel.gotoPageByName(null, "History");
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "gitHistoryPage",
                      5000, "the History page is current");
            let histPage = RootData.pageView.currentPageItem;

            // Clean tree, so the newest row (HEAD, our image commit) auto-selects.
            let histView = findByName(histPage, "gitHistoryView");
            verify(histView, "found the history view");
            tryVerify(() => histView.selectedSha === headOid, 5000,
                      "the image commit is selected");

            histPage.navigateToFileDiff(rel, true, true, "Modified", false);
            tryVerify(() => RootData.pageView.currentPageItem !== histPage, 5000,
                      "navigated to the image diff page");
            let cmp = RootData.pageView.currentPageItem;
            compare(cmp.commitSha, headOid, "the diff is for the image commit");

            // Both panes must finish loading, or the shot is a placeholder.
            let primaryImage = findByName(cmp, "primaryImage");
            let beforeImage = findByName(cmp, "beforeImage");
            verify(primaryImage && beforeImage, "found both compare images");
            tryVerify(() => primaryImage.status === QQ.Image.Ready
                      && beforeImage.status === QQ.Image.Ready, 15000,
                      "before and after images both loaded");
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "git-image-compare");
            verify(path.length > 0, "grabToFile wrote the image-compare screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "image-compare screenshot is not blank");

            restoreDemoProject();
        }

        // The sign-in panel on the Online projects page (File -> Open from
        // Online...), the first-run route to GitHub. Backs "Signing in: the device
        // code" in docs/manual/collaboration/sign-in-to-github.md.
        //
        // The panel is what the Account picker's "Add Account" entry reveals, so we
        // put the page in its "addAccount" state — but set directly rather than by
        // clicking that entry, whose onClicked also fires startAddGitHubAccount()
        // and requests a device code over the network, swapping the idle "Connect to
        // GitHub" button for "Requesting a sign-in code...". The harness has no
        // GitHub, so we stage the idle state (cleared settings mean no account is
        // signed in) and assert it before the grab.
        function test_signInConnectGitHub() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            RootData.pageSelectionModel.gotoPageByName(null, "Remote");
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "remoteRepositoryPage",
                      5000, "the Online projects page is current");

            let page = RootData.pageView.currentPageItem;
            page.state = "addAccount";
            settle();

            // Idle and not installed: the panel shows the "Connect to GitHub" button
            // and its context message, with no device-code request in flight.
            let gitHub = RootData.remote.gitHubIntegration;
            compare(gitHub.authState, GitHubIntegration.Idle,
                    "the sign-in panel is idle (no device code requested)");
            verify(!gitHub.needsInstallation, "the install prompt is not showing");

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "collaboration-sign-in");
            verify(path.length > 0, "grabToFile wrote the sign-in screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "sign-in screenshot is not blank");

            restoreDemoProject();
        }

        // The Sync button ringed in the top-right of the window, in its no-remote
        // state (the upload-cloud icon). Backs "Step 1: give the project a remote"
        // in docs/manual/collaboration/share-a-project.md: the demo project has no
        // remote, so the button shows the "click to set up sync" icon a user starts
        // sharing from. Full-window grab with the highlight ring, so the shot also
        // teaches where the button lives.
        function test_setUpRemoteButton() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let syncButton = findByName(rootId.mainWindow, "syncButton");
            verify(syncButton, "found the sync button in the link bar");
            verify(!syncButton.hasRemote,
                   "the demo project has no remote, so the button shows the set-up icon");

            highlightOverlayId.target = syncButton;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow,
                                                "collaboration-set-up-remote");
            verify(path.length > 0, "grabToFile wrote the set-up-remote screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "set-up-remote screenshot is not blank");

            highlightOverlayId.target = null;
        }

        // The Source page — the recent projects list, and the breadcrumb that is the
        // only way to reach it. Backs "Reopen a recent project" in
        // docs/manual/projects-and-files/open-a-project.md.
        //
        // The list is inherited state twice over: it is restored from QSettings, and
        // every save auto-adds its project (so test_gitHistory, which runs earlier,
        // puts a temp path in it). Clear it and put back exactly what the shot should
        // show.
        function test_recentProjects() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            RootData.recentProjectModel.clear();

            // testcasesDatasetPath already copies the fixture into the shared temp
            // root, so this is a real file that exists — which is all
            // addRepositoryFromProjectFile needs. It only records the path (unlike
            // addRepository, it never writes a git repo there), so the fixture is
            // never at risk. No need to wrap it in copyToTempDirUrl and copy a copy.
            let result = RootData.recentProjectModel.addRepositoryFromProjectFile(
                TestHelper.toLocalUrl(
                    TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw")));
            verify(!result.hasError,
                   "the demo project was added to the recent list: " + result.errorMessage);

            RootData.pageSelectionModel.currentPageAddress = "Source";
            tryVerify(() => RootData.pageView.currentPageItem !== null, 5000,
                      "the Source page is current");
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "project-recent-projects");
            verify(path.length > 0, "grabToFile wrote the recent-projects screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "recent-projects screenshot is not blank");

            restoreDemoProject();
        }

        // The Save As dialog as it looks on a project's FIRST save. Backs "Save it
        // for the first time" in docs/manual/projects-and-files/save-a-project.md.
        //
        // Unlike every other shot, this one makes its own project instead of using
        // the demo cave: the dialog reads the project's name, file type and last
        // directory as it opens, and only a never-saved project produces the
        // first-save layout — the Project Name field is hidden once a project has
        // been saved, and the format only defaults to Directory while the file type
        // is still a git directory. Like test_gitHistory it must call
        // restoreDemoProject() before it returns — see that function.
        function test_saveAsDialog() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            RootData.newProject();
            RootData.futureManagerModel.waitForFinished();
            tryVerify(() => RootData.project.isTemporaryProject, 5000,
                      "the new project is temporary (the first-save layout)");

            // newProject() names the region with a random placeholder (Misty
            // Cavern, Thunder Ridge, ...). Pin it, or the shot reads differently
            // every run — the name drives the Project Name field AND the derived
            // path below it.
            RootData.region.name = saveAsShotProjectName;

            let dialog = rootId.saveAsDialog;
            verify(dialog, "found the Save As dialog");

            // Same reason as the caret menu in test_excludeDistance: draw the popup
            // into the window overlay so grabWindow() can see it. Set here, never in
            // the app's QML.
            dialog.popupType = QC.Popup.Item;
            dialog.open();
            tryVerify(() => dialog.opened, 5000, "the Save As dialog is open");

            // Location is left at the app's own default so the shot shows what a
            // user sees: RootData.lastDirectory, which is the Desktop in a fresh
            // process (the test main clears settings, and no manual shot saves).
            //
            // That makes the shot depend on the Desktop of whoever regenerates it:
            // if it already holds a folder named saveAsShotProjectName the dialog
            // shows a red "a folder already exists" error and disables Save, and if
            // the Desktop itself is missing it shows "destination folder does not
            // exist". Both are caught here rather than shipped as a screenshot of an
            // error — if this fails, move the offending folder or rename the shot's
            // project.
            verify(dialog.destinationFolder.length > 0,
                   "the Save As dialog defaulted to a Location");
            let conflict = findByName(dialog.contentItem, "saveAsConflictLabel");
            verify(conflict, "found the Save As validation label");
            verify(!conflict.visible,
                   "the Save As dialog has no conflict or error to show (see comment)");

            settle();

            let path = WindowGrabber.grabItemToFile(dialog.background, "project-save-as",
                                                    saveAsCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the save-as screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "save-as screenshot is not blank");

            // Leave no modal behind for the shots that run after this one.
            dialog.close();
            tryVerify(() => !dialog.visible, 5000, "the Save As dialog closed");

            restoreDemoProject();
        }

        // Every LiDAR shot, from ONE import. Backs docs/manual/notes/lidar-notes.md.
        // Grouped because importing and loading the .glb costs seconds; each shot
        // is a different state of the same note.
        //   - notes-lidar-note.png: the scan alone, transform panel collapsed —
        //     the chapter's opening "this is what a LiDAR note is" image.
        //   - notes-lidar-transform.png: the panel expanded, cropped tight.
        //   - notes-lidar-station.png: the carpet toolbar's Station button ringed.
        //
        // Imports the .glb at runtime (the fixture project ships without one) the
        // same way tst_LiDARNotes does: copy it to a temp dir, then emit the
        // gallery's imagesAdded. Uses the trip page — the LiDAR viewer needs the
        // room, and unlike a sketched note there's no first-visit render quirk.
        function test_lidarNoteShots() {
            TestHelper.loadProjectFromZip(RootData.project,
                TestHelper.testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
            RootData.futureManagerModel.waitForFinished();

            RootData.pageSelectionModel.currentPageAddress = "View";
            RootData.futureManagerModel.waitForFinished();
            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault";
            tryVerify(() => RootData.pageView.currentPageItem !== null
                && RootData.pageView.currentPageItem.objectName === "tripPage");

            let fileLoader = findByName(rootId.mainWindow, "fileMenuButtonLoader");
            if (fileLoader) { fileLoader.active = true; }

            let gallery = findByName(RootData.pageView.currentPageItem, "noteGallery");
            verify(gallery, "found the trip page note gallery");
            if (!requireRhi(gallery)) { return; }

            let galleryView = findByName(RootData.pageView.currentPageItem, "galleryView");
            verify(galleryView, "found the gallery view");
            let notesBefore = galleryView.count;

            let lidarPath = TestHelper.copyToTempDirUrl(
                TestHelper.testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
            gallery.imagesAdded([lidarPath]);
            tryVerify(() => galleryView.count === notesBefore + 1, 15000,
                      "the .glb was added as a note");

            // Select by clicking the new thumbnail rather than trusting the note to
            // auto-select: the gallery only refreshes currentNoteLiDAR from the
            // ListView's currentItem on an index/count change, and currentItem is
            // null until the delegate is created, so the selection can be missed
            // entirely. Clicking the delegate is what tst_LiDARNotes does.
            let index = galleryView.count - 1;

            // Scroll the new note into view before looking for its delegate.
            // galleryView is a clipped ListView, so it only creates the delegate
            // for a note that is actually in the viewport — the new note is
            // appended last, and whether it landed in view depended on how many
            // notes the trip already had and on layout timing. That made the
            // search below fail about one full generator run in five, always
            // here. Positioning first makes the delegate exist deterministically.
            galleryView.positionViewAtIndex(index, QQ.ListView.Contain);

            let thumb = null;
            tryVerify(() => {
                thumb = findByName(RootData.pageView.currentPageItem, "noteImage" + index);
                return thumb !== null;
            }, 10000, "the LiDAR note's thumbnail exists");
            mouseClick(thumb);
            tryVerify(() => gallery.currentNoteLiDAR !== null, 15000,
                      "the LiDAR note is current");

            let viewer = findByName(RootData.pageView.currentPageItem, "rhiViewerId");
            verify(viewer, "found the LiDAR viewer");
            tryVerify(() => viewer.scene.gltf.status === RenderGLTF.Ready, 20000,
                      "the glTF scan finished loading");
            RootData.futureManagerModel.waitForFinished();

            // Wait for the editor rather than grabbing as soon as the glTF is
            // Ready: on Ready the viewer captures a gallery thumbnail of itself,
            // and that CAPTURE_ICON state deliberately hides the transform editor
            // and the stations. The state is only restored by grabToImage's async
            // callback, so a grab taken on Ready alone catches the panel mid-hide.
            let editor = null;
            tryVerify(() => {
                editor = findVisibleByName(RootData.pageView.currentPageItem,
                                           "noteLiDARTransformEditor");
                return editor !== null;
            }, 10000, "the LiDAR Note Transform editor is visible again after the icon capture");

            // Nothing is ringed in either of the next two shots — the subject is the
            // scan and its panel, and a highlight would only dim what the crop is
            // meant to show.
            highlightOverlayId.target = null;

            // --- "What a LiDAR note is" shot: the scan, unobstructed ---
            // Collapse the transform panel to its title bar (a real state — the
            // panel's chevron does this, and narrow layouts start collapsed) so the
            // opening image is the scan rather than a form over a scan.
            editor.collapsed = true;
            settle();
            let notePath = WindowGrabber.grabItemToFile(viewer, "notes-lidar-note", 0);
            verify(notePath.length > 0, "grabItemToFile wrote the lidar-note screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(notePath),
                   "lidar-note screenshot is not blank");

            // --- Transform panel shot ---
            editor.collapsed = false;
            settle();

            let path = WindowGrabber.grabItemToFile(editor, "notes-lidar-transform",
                                                    lidarPanelCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the lidar-transform screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "lidar-transform screenshot is not blank");

            // --- Station tool shot ---
            // The Station button lives on the carpet toolbar, so enter that mode
            // first. On a LiDAR note the Add group holds Station alone: the Scrap
            // and Lead buttons bind `visible: currentNote !== null`, and a LiDAR
            // note sets currentNoteLiDAR instead, so they hide themselves. Grabbed
            // full-window to show the button in the toolbar over the scan.
            gallery.setMode("CARPET");
            RootData.futureManagerModel.waitForFinished();

            let stationButton = null;
            tryVerify(() => {
                stationButton = findVisibleByName(gallery, "addScrapStation");
                return stationButton !== null;
            }, 5000, "the Station button is visible on a LiDAR note");

            highlightOverlayId.target = stationButton;
            settle();

            let stationPath = WindowGrabber.grabToFile(rootId.mainWindow,
                                                       "notes-lidar-station");
            verify(stationPath.length > 0, "grabToFile wrote the lidar-station screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(stationPath) > 0.3,
                   "lidar-station screenshot is not blank");
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

        // Load the demo project, open the Settings page, and select tab `index`
        // on the vertical tab bar (0 Jobs, 1 Warping, 2 PDF/SVG, 3 Git,
        // 4 Appearance, 5 Rendering, 6 Sketch). Returns the tab bar, or null after
        // skip() when there is no live QRhi. The Settings panels don't depend on
        // project content, but reloading hands each shot a known-clean window; the
        // font/PDF/MSAA defaults come from QSettings, which the test harness clears
        // per process, so those shots are deterministic. (The Jobs tab's thread
        // counts reflect the regenerating machine's cores, so its numbers vary by
        // machine like the git-history hashes do — the prose asserts no specific
        // value.)
        function openSettingsTab(index) {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            RootData.pageSelectionModel.currentPageAddress = "Settings";

            let fileLoader = findByName(rootId.mainWindow, "fileMenuButtonLoader");
            if (fileLoader) { fileLoader.active = true; }

            let tabBar = findByName(rootId.mainWindow, "settingsTabBar");
            tryVerify(() => tabBar !== null, 3000, "found the settings tab bar");

            if (!requireRhi(tabBar)) { return null; }

            tabBar.currentIndex = index;
            verify(tabBar.currentIndex === index, "settings tab " + index + " is selected");
            return tabBar;
        }

        // Grab the whole Settings page after selecting tab `index`, with a
        // highlight ring around the tab's panel (found by `panelName`). Whole-window
        // so the tab rail stays visible for orientation; the ring directs the eye to
        // the panel against the page's whitespace, the same way test_warpingSettings
        // does. Backs docs/manual/settings/change-settings.md.
        function grabSettingsTab(index, panelName, shotName) {
            let tabBar = openSettingsTab(index);
            if (!tabBar) { return; }

            let panel = findByName(rootId.mainWindow, panelName);
            tryVerify(() => findByName(rootId.mainWindow, panelName) !== null, 3000,
                      "found " + panelName);
            highlightOverlayId.target = findByName(rootId.mainWindow, panelName);
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, shotName);
            verify(path.length > 0, "grabToFile wrote the " + shotName + " screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   shotName + " screenshot is not blank");

            highlightOverlayId.target = null;
            restoreDemoProject();
        }

        // Settings → Jobs tab (also the page-overview shot: the tab rail is visible
        // down the left). Backs docs/manual/settings/change-settings.md.
        function test_settingsJobs() {
            grabSettingsTab(0, "jobSettingsItem", "settings-jobs");
        }

        // Settings → PDF / SVG tab. Backs docs/manual/settings/change-settings.md.
        function test_settingsPdf() {
            grabSettingsTab(2, "pdfSettingsItem", "settings-pdf");
        }

        // Settings → Appearance tab (font family + size). Backs
        // docs/manual/settings/change-settings.md.
        function test_settingsAppearance() {
            grabSettingsTab(4, "appearanceSettingsItem", "settings-appearance");
        }

        // Settings → Rendering tab (MSAA anti-aliasing). Backs
        // docs/manual/settings/change-settings.md.
        function test_settingsRendering() {
            grabSettingsTab(5, "renderingSettingsItem", "settings-rendering");
        }

        // ---------------------------------------------------------------------
        // Survey Data chapter (docs/manual/survey-data/)
        // ---------------------------------------------------------------------

        // The visible button labelled `label` under `item`. AddAndSearchBar's
        // button is objectName "addButton", and so are AddButton and KeywordTab's
        // — so on a page with more than one add-bar the name alone is ambiguous.
        // The label is what the manual tells the reader to click, so match on it.
        function findAddButtonWithText(item, label) {
            let all = findAllByName(item, "addButton", []);
            for (let i = 0; i < all.length; ++i) {
                if (all[i].visible && all[i].text === label) { return all[i]; }
            }
            return null;
        }

        // Load the demo project and navigate to `address`, asserting the page that
        // comes up is `pageName`. Returns the page item, or null after skip() when
        // the platform has no QRhi (the caller must return).
        function openDataPage(address, pageName) {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            // Neutral page first so a stale page is torn down.
            RootData.pageSelectionModel.currentPageAddress = "View";
            RootData.futureManagerModel.waitForFinished();
            RootData.pageSelectionModel.currentPageAddress = address;
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === pageName,
                      5000, "the " + pageName + " is current");

            let fileLoader = findByName(rootId.mainWindow, "fileMenuButtonLoader");
            if (fileLoader) { fileLoader.active = true; }

            let page = RootData.pageView.currentPageItem;
            if (!requireRhi(page)) { return null; }
            settle();
            return page;
        }

        // The Add Cave button on the cave list, highlighted. Grabbed whole-window
        // rather than cropped: the cave list around it is the project's dashboard
        // (each cave's length and depth), which the page describes.
        // Backs the "Add a cave" section of survey-data/caves-and-trips.md.
        function test_addCaveButton() {
            let page = openDataPage("Source/Data", "dataMainPage");
            if (!page) { return; }

            let addCave = findAddButtonWithText(page, "Add Cave");
            verify(addCave, "found the Add Cave button");

            highlightOverlayId.target = addCave;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "survey-add-cave");
            verify(path.length > 0, "grabToFile wrote the add-cave screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "survey-add-cave is not blank");
        }

        // The Add Trip button on a cave page, highlighted. The trip table beside
        // it (Name, Date, Stations, Length, Decl) is the point of the shot, so
        // this is whole-window too.
        // Backs the "Add a trip" section of survey-data/caves-and-trips.md.
        function test_addTripButton() {
            let page = openDataPage("Source/Data/Cave=Phake Cave 3000", "cavePage");
            if (!page) { return; }

            // CavePage builds a wide (table) and a narrow (flow) layout, both
            // carrying objectName "addTrip"; only one is shown at a time.
            let addTripBar = findVisibleByName(page, "addTrip");
            verify(addTripBar, "found the visible Add Trip bar");

            let addTrip = findAddButtonWithText(addTripBar, "Add Trip");
            verify(addTrip, "found the Add Trip button");

            highlightOverlayId.target = addTrip;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "survey-add-trip");
            verify(path.length > 0, "grabToFile wrote the add-trip screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "survey-add-trip is not blank");
        }

        // A UTM easting/northing in the demo cave's neighbourhood, used to fix a
        // station so the georeferencing shots show a real value rather than the
        // "n/a" a fresh (un-fixed) cave reports. EPSG:32613 is UTM zone 13N; the
        // exact coordinate is arbitrary — only that it resolves to a convergence.
        readonly property string georefCS: "EPSG:32613"
        readonly property string georefStation: "a1"
        readonly property real georefEasting: 350000
        readonly property real georefNorthing: 4300000
        readonly property real georefElevation: 1200

        // Set the project coordinate system and fix `page.currentCave`'s first
        // station to real coordinates, so the cave is georeferenced. Both changes
        // are undone by the restoreDemoProject() reload each georef shot ends with.
        function georeferenceDemoCave(page) {
            RootData.region.geoReference.globalCoordinateSystem = georefCS;

            let model = page.currentCave.fixStations;
            model.addFixStation();
            model.setData(model.index(0), georefStation, FixStationModel.StationNameRole);
            model.setData(model.index(0), georefCS, FixStationModel.InputCSRole);
            model.setData(model.index(0), georefEasting, FixStationModel.EastingRole);
            model.setData(model.index(0), georefNorthing, FixStationModel.NorthingRole);
            model.setData(model.index(0), georefElevation, FixStationModel.ElevationRole);
        }

        // Drive a live two-point measurement on `interaction` for the shots that show
        // the measurement tool in action. Clears any prior measurement, activates the
        // tool, then ray-casts a grid of offsets around the view center until two
        // points land on the cave geometry (carpet meshes are the easy hit; the line
        // plot is thin) — the same approach as test_georefCoordinatePicker. Point A is
        // found scanning from the near-center out and point B from the far end in, so
        // the two sit well apart and the measured span (and its line) reads clearly.
        function placeDemoMeasurement(glTerrain, interaction) {
            interaction.deactivate();
            interaction.activate();
            tryVerify(() => interaction.enabled === true, 2000, "the measure tool is active");

            let cx = glTerrain.width / 2;
            let cy = glTerrain.height / 2;
            let offsets = [];
            for (let gx = -200; gx <= 200; gx += 50) {
                for (let gy = -140; gy <= 140; gy += 50) {
                    offsets.push([gx, gy]);
                }
            }

            let aIndex = -1;
            for (let k = 0; k < offsets.length && aIndex < 0; ++k) {
                interaction.place(Qt.point(cx + offsets[k][0], cy + offsets[k][1]));
                wait(40);
                if (interaction.hasFirst) { aIndex = k; }
            }
            verify(aIndex >= 0, "the first measurement point landed on the cave");

            for (let k = offsets.length - 1; k > aIndex && !interaction.hasMeasurement; --k) {
                interaction.place(Qt.point(cx + offsets[k][0], cy + offsets[k][1]));
                wait(40);
            }
            tryVerify(() => interaction.hasMeasurement, 2000,
                      "a two-point measurement is complete");
        }

        // The project Coordinate system control on the Data page, set to UTM so the
        // zone / hemisphere / resolved-EPSG fields are all showing.
        // Backs docs/manual/georeferencing/georeference-a-cave.md.
        //
        // Cropped to the Geospatial group box (label + control) rather than grabbed
        // whole-window: the control is one small row on an otherwise full Data page,
        // so a cropped shot reads in the manual's narrow column where a whole-window
        // one would not.
        function test_georefCoordinateSystem() {
            let page = openDataPage("Source/Data", "dataMainPage");
            if (!page) { return; }

            RootData.region.geoReference.globalCoordinateSystem = georefCS;

            let group = findByName(page, "geospatialGroupBox");
            verify(group, "found the Geospatial group box");
            let combo = findByName(page, "globalCoordinateSystemComboBox");
            verify(combo, "found the coordinate system combo box");

            highlightOverlayId.target = combo;
            settle();

            let path = WindowGrabber.grabItemToFile(group, "georef-coordinate-system",
                                                    panelCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the coordinate-system shot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "georef-coordinate-system is not blank");

            restoreDemoProject();
        }

        // Two shots from one georeferenced cave: the Fix Stations page with a fixed
        // station, and the cave page's Grid convergence readout showing a value.
        // Back docs/manual/georeferencing/georeference-a-cave.md and
        // grid-convergence.md.
        function test_georefFixAndConvergence() {
            let page = openDataPage("Source/Data/Cave=Phake Cave 3000", "cavePage");
            if (!page) { return; }

            georeferenceDemoCave(page);
            settle();

            // The cave page, with the Fix stations count and Grid convergence value
            // both now populated. Whole-window: the trip table and stats column
            // around the readout are the context. Ring the whole convergence cell
            // (label + value) via the value's parent.
            let gcValue = findByName(page, "gridConvergenceValue");
            verify(gcValue, "found the grid convergence value");
            highlightOverlayId.target = gcValue.parent;
            settle();

            let gcPath = WindowGrabber.grabToFile(rootId.mainWindow, "georef-grid-convergence");
            verify(gcPath.length > 0, "grabToFile wrote the grid-convergence shot");
            verify(OffscreenRenderTester.nonBlackFraction(gcPath) > 0.3,
                   "georef-grid-convergence is not blank");

            // Now the Fix Stations sub-page, reached the way the cave page's link
            // reaches it (gotoPageByName off the cave page's own PageView.page).
            highlightOverlayId.target = null;
            RootData.pageSelectionModel.gotoPageByName(page.PageView.page, "Fix Stations");
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "the Fix Stations page is current");
            let fixPage = RootData.pageView.currentPageItem;

            // No highlight ring here: the row is a single populated line and the
            // ring would occlude the easting. The columns are the point.
            let csCombo = findVisibleByName(fixPage, "inputCSComboBox.0");
            verify(csCombo, "found the row's Input CS picker (page rendered)");
            highlightOverlayId.target = null;
            settle();

            let fixPath = WindowGrabber.grabToFile(rootId.mainWindow, "georef-fix-station");
            verify(fixPath.length > 0, "grabToFile wrote the fix-station shot");
            verify(OffscreenRenderTester.nonBlackFraction(fixPath) > 0.3,
                   "georef-fix-station is not blank");

            restoreDemoProject();
        }

        // The 3D-view coordinate picker: georeference the cave, activate the Pick
        // tool, pick a point on the model, and grab the popup that reads the point
        // back out in the project CRS, WGS84, and elevation. Backs the "Read
        // coordinates back out" section of georeference-a-cave.md.
        //
        // The popup is a QC.Popup opened by a `visible` binding (hasPick &&
        // pickButtonId.selected), so it's a QObject child (findChild, not
        // findByName) and needs popupType = Popup.Item to draw into the window
        // overlay grabWindow can see — the same reason as the Import/Export menus.
        function test_georefCoordinatePicker() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");

            // Georeference cave 0 so the picked point resolves to real coordinates:
            // the popup's CRS / WGS84 / Elevation sections only show with a CS set.
            let cave = RootData.region.cave(0);
            verify(cave, "found the demo cave");
            RootData.region.geoReference.globalCoordinateSystem = georefCS;
            let model = cave.fixStations;
            model.addFixStation();
            model.setData(model.index(0), georefStation, FixStationModel.StationNameRole);
            model.setData(model.index(0), georefCS, FixStationModel.InputCSRole);
            model.setData(model.index(0), georefEasting, FixStationModel.EastingRole);
            model.setData(model.index(0), georefNorthing, FixStationModel.NorthingRole);
            model.setData(model.index(0), georefElevation, FixStationModel.ElevationRole);
            RootData.futureManagerModel.waitForFinished();

            let picker = glTerrain.coordinatePickerInteraction;
            verify(picker, "found the coordinate picker");
            let popup = findChild(glTerrain, "coordinatePickerPopup");
            verify(popup, "found the coordinate picker popup");
            popup.popupType = QC.Popup.Item;

            picker.activate();
            tryVerify(() => picker.enabled === true, 2000, "the picker is active");

            // Ray-cast candidate points around the view center until one lands on the
            // cave geometry (the carpet meshes are the easy hit; the line plot is thin).
            let cx = glTerrain.width / 2;
            let cy = glTerrain.height / 2;
            let offsets = [[0,0],[0,-60],[0,60],[-90,0],[90,0],[-90,-60],[90,60],
                           [0,-130],[0,130],[-180,0],[180,0],[-180,-120],[180,120]];
            let picked = false;
            for (let k = 0; k < offsets.length && !picked; ++k) {
                picker.clearPick();
                picker.pick(Qt.point(cx + offsets[k][0], cy + offsets[k][1]));
                wait(60);
                picked = picker.hasPick;
            }
            verify(picked, "a pick landed on the cave geometry");
            tryVerify(() => popup.visible, 2000, "the picked-coordinates popup is open");

            // A UTM easting/northing is wider than the value field, and the field
            // defaults to its tail (cursor at end), hiding the leading digits — the
            // meaningful part of the coordinate. Scroll each field to the start.
            let csField = findChild(popup, "CSField");
            if (csField) { csField.cursorPosition = 0; }
            let wgsField = findChild(popup, "WgsField");
            if (wgsField) { wgsField.cursorPosition = 0; }
            settle();

            let path = WindowGrabber.grabItemToFile(popup.contentItem,
                                                    "georef-coordinate-picker", 100);
            verify(path.length > 0, "grabItemToFile wrote the coordinate-picker shot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "georef-coordinate-picker is not blank");

            picker.deactivate();
            restoreDemoProject();
        }

        // The empty Geospatial Layers page: the "Add LAZ Files" bar (highlighted)
        // and the "no layers yet" help box. Whole-window so the sidebar / breadcrumb
        // place it, and because the demo has no point cloud to populate the table —
        // the page's own entry point is the point of the shot. Backs the "Where point
        // clouds live" / "Add a LAZ or LAS file" sections of
        // docs/manual/point-clouds/add-a-point-cloud.md.
        //
        // Reached the way the Data page's Layers link reaches it: gotoPageByName off
        // the Data page's own PageView.page (the page is registered lazily on
        // DataMainPage, so navigate to Data first).
        function test_pointCloudGeospatialPage() {
            let page = openDataPage("Source/Data", "dataMainPage");
            if (!page) { return; }

            RootData.pageSelectionModel.gotoPageByName(page.PageView.page, "Geospatial Layers");
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "geospatialLayerPage",
                      5000, "the Geospatial Layers page is current");
            let geoPage = RootData.pageView.currentPageItem;

            let addBar = findByName(geoPage, "addLazBar");
            verify(addBar, "found the Add LAZ Files bar");
            highlightOverlayId.target = addBar;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "point-clouds-empty");
            verify(path.length > 0, "grabToFile wrote the geospatial-page shot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "point-clouds-empty is not blank");

            highlightOverlayId.target = null;
            restoreDemoProject();
        }

        // The Clip tool in the 3D view's bottom toolbar, highlighted, so the clip
        // page can show where the tool lives. Whole-window: the demo cave in the 3D
        // view is the context, and the toolbar (Pick / Clip / Measure) sits over it.
        // The button is present with or without a loaded cloud, so no fixture is
        // needed. Backs the "Open the clip tool" section of
        // docs/manual/point-clouds/clip-a-point-cloud.md.
        function test_pointCloudClipTool() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");

            let clipButton = findByName(glTerrain, "lazClipButton");
            verify(clipButton, "found the Clip tool button");
            highlightOverlayId.target = clipButton;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "point-clouds-clip-tool");
            verify(path.length > 0, "grabToFile wrote the clip-tool shot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "point-clouds-clip-tool is not blank");

            highlightOverlayId.target = null;
        }

        // The Measure tool in action: the button highlighted in the 3D view's bottom
        // toolbar (Pick / Clip / Measure) AND a completed two-point measurement — the
        // two placed points and the line drawn between them — so the page shows both
        // where the tool lives and what using it looks like. Whole-window for context.
        // Backs the "Open the measurement tool" / "Place two points" sections of
        // docs/manual/measurement/measure-distance-and-bearing.md.
        function test_measurementTool() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");

            let interaction = findChild(glTerrain, "measurementInteraction");
            verify(interaction, "found the measurement interaction");
            placeDemoMeasurement(glTerrain, interaction);

            // Hide the readout popup for this shot so it doesn't cover the measurement
            // line — the line and its endpoints are the point here, and the readout has
            // its own dedicated shot (measurement-readout.png). Assigning visible drops
            // the popup's `measureButton.selected && hasMeasurement` binding, which is
            // fine for a one-shot grab; the interaction is torn down right after.
            let popup = findChild(glTerrain, "measurementReadoutPopup");
            if (popup) { popup.visible = false; }

            let measureButton = findByName(glTerrain, "measurementButton");
            verify(measureButton, "found the Measure tool button");
            highlightOverlayId.target = measureButton;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "measurement-tool");
            verify(path.length > 0, "grabToFile wrote the measurement-tool shot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "measurement-tool is not blank");

            highlightOverlayId.target = null;
            interaction.deactivate();
            restoreDemoProject();
        }

        // A live two-point measurement, with the readout panel showing the distance /
        // direction / by-axis groups and the grid/true/magnetic azimuth selector. The
        // cave is georeferenced first so True and Magnetic are enabled (a local cave
        // offers Grid only). Two points are ray-cast around the view center until each
        // lands on the cave geometry (the carpet meshes are the easy hit; the line
        // plot is thin) — the same approach as test_georefCoordinatePicker. Backs the
        // "Read the measurement" and "Choose which north the azimuth uses" sections of
        // docs/manual/measurement/measure-distance-and-bearing.md.
        function test_measurementReadout() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");

            // Georeference cave 0 so the azimuth selector's True and Magnetic options
            // are enabled (both need a coordinate system). Undone by restoreDemoProject.
            let cave = RootData.region.cave(0);
            verify(cave, "found the demo cave");
            RootData.region.geoReference.globalCoordinateSystem = georefCS;
            let model = cave.fixStations;
            model.addFixStation();
            model.setData(model.index(0), georefStation, FixStationModel.StationNameRole);
            model.setData(model.index(0), georefCS, FixStationModel.InputCSRole);
            model.setData(model.index(0), georefEasting, FixStationModel.EastingRole);
            model.setData(model.index(0), georefNorthing, FixStationModel.NorthingRole);
            model.setData(model.index(0), georefElevation, FixStationModel.ElevationRole);
            RootData.futureManagerModel.waitForFinished();

            let interaction = findChild(glTerrain, "measurementInteraction");
            verify(interaction, "found the measurement interaction");
            let popup = findChild(glTerrain, "measurementReadoutPopup");
            verify(popup, "found the measurement readout popup");
            popup.popupType = QC.Popup.Item;

            interaction.azimuthReference = AzimuthReference.Grid;
            placeDemoMeasurement(glTerrain, interaction);
            tryVerify(() => popup.visible, 2000, "the measurement readout is open");
            settle();

            let path = WindowGrabber.grabItemToFile(popup.contentItem,
                                                    "measurement-readout", 40);
            verify(path.length > 0, "grabItemToFile wrote the measurement-readout shot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "measurement-readout is not blank");

            interaction.deactivate();
            restoreDemoProject();
        }

        // The Leads page for a cave: every lead gathered into one table (Done, Goto,
        // Nearest, Size, Distance, Trip, Description), with the description filter,
        // the reference station the Distance is measured from, and the Export CSV
        // button in the toolbar above. Whole-window so the breadcrumb / sidebar place
        // the page. The demo cave ships with five leads, so the list populates as-is;
        // a reference station is set so the Distance column reads real values instead
        // of the 0 m a lead reports with none. Backs
        // docs/manual/leads/track-and-export-leads.md.
        function test_leadsList() {
            let page = openDataPage("Source/Data/Cave=Phake Cave 3000", "cavePage");
            if (!page) { return; }

            // The Leads sub-page is registered on the cave page; reach it the way the
            // cave page's Leads link does (gotoPageByName off its own PageView.page).
            RootData.pageSelectionModel.gotoPageByName(page.PageView.page, "Leads");
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "leadPage",
                      5000, "the Leads page is current");
            let leadPageItem = RootData.pageView.currentPageItem;

            // The LeadModel is a non-visual child (findChild, not findByName). Set its
            // reference station to a real survey station so Distance isn't all 0 m.
            let leadModel = findChild(leadPageItem, "leadModel");
            verify(leadModel, "found the lead model");
            leadModel.referanceStation = "a1";

            tryVerify(() => leadModel.rowCount() > 0, 5000,
                      "the demo cave has leads to list");
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "leads-page");
            verify(path.length > 0, "grabToFile wrote the leads-page shot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "leads-page is not blank");

            restoreDemoProject();
        }

        // A lead selected in the 3D view with its popup open in edit mode — the
        // Completed checkbox, the size editor, and the description field where a lead
        // is edited and marked done — with the cave's other question-mark markers
        // behind it. Cropped around the popup (with margin) so the form reads while
        // keeping the 3D context. Backs the "Edit a lead and mark it done" section of
        // docs/manual/leads/track-and-export-leads.md.
        function test_leadEdit() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");

            RootData.leadsVisible = true;

            // The "Walking" lead (2.5 x 2 m, described and sized — the one tst_Leads
            // pins as leadPoint2_1) makes the edit form show real values. Its marker
            // is created once carpeting has positioned the leads (loadRhiViewer drains
            // those jobs).
            // Select through the live LeadView's selection manager (a CONSTANT
            // property that is always valid), not the marker's own injected one — in
            // a full run the marker can be found while still mid-initialisation, when
            // its per-item selectionManager is briefly undefined. Clear any selection
            // an earlier 3D-view test left behind so the popup state is fresh.
            let leadView = glTerrain.leadView;
            verify(leadView && leadView.selectionManager, "found the live lead view");
            leadView.selectionManager.selectedItem = null;

            let leadPoint = null;
            tryVerify(() => {
                leadPoint = findByName(glTerrain, "leadPoint2_1");
                return leadPoint !== null && leadPoint.scrap !== null;
            }, 8000, "the Walking lead marker is live in the 3D view");

            // Center the camera on the lead so its popup opens with room in every
            // direction rather than hanging off a window edge (which clips the form).
            // This is what the Leads page's Goto does; the popup points at the marker.
            let viewpage = RootData.pageView.currentPageItem;
            if (viewpage && viewpage.turnTableInteraction) {
                viewpage.turnTableInteraction.centerOn(leadPoint.position3D, false);
            }
            settle();

            // Selecting the marker opens its popup (loaded asynchronously). Re-find
            // and re-apply the selection each tick: in a full run the marker can be
            // rebuilt (project reload, billboard reposition) after the first lookup,
            // so a single set of selectedItem can miss and the popup never opens.
            let quoteBox = null;
            tryVerify(() => {
                let lp = findByName(glTerrain, "leadPoint2_1");
                if (lp && lp.scrap !== null) {
                    leadPoint = lp;
                    leadView.selectionManager.selectedItem = lp;
                }
                quoteBox = findByName(glTerrain, "leadQuoteBox");
                return quoteBox !== null;
            }, 10000, "the lead popup opened");
            quoteBox.editMode = true;

            // The QuoteBox root is a zero-size point at the pointer tip; its balloon
            // is the content Item nested inside it (Shape -> childrenContainer). Wait
            // for that to be laid out, then crop to it so the whole form is captured
            // rather than a margin around the tip.
            let content = null;
            tryVerify(() => {
                content = (quoteBox.children[0] && quoteBox.children[0].children[0])
                          ? quoteBox.children[0].children[0] : null;
                return content !== null && content.width > 0;
            }, 5000, "the popup content box is laid out");
            settle();

            let path = WindowGrabber.grabItemToFile(content, "lead-edit", 40);
            verify(path.length > 0, "grabItemToFile wrote the lead-edit shot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "lead-edit is not blank");

            leadView.selectionManager.selectedItem = null;
            restoreDemoProject();
        }

        // The Import menu on the Data page, open, showing the survey formats.
        // Backs docs/manual/import-export/import-surveys.md.
        //
        // The menu is grabbed with Popup.Item for the reason test_excludeDistance
        // is: Fusion gives a QC.Menu no popupType, so it defaults to Popup.Window
        // — a separate top-level window grabWindow(mainWindow) cannot see. Set from
        // the test, never in the app's QML. importMenu is a flat list (no
        // submenus), so a single grab shows every format.
        function test_importMenu() {
            let page = openDataPage("Source/Data", "dataMainPage");
            if (!page) { return; }

            // findChild, not findByName: a QC.Menu is a QObject child of its
            // button, not a visual child, so the visual-tree walk can't see it.
            // Scoped to the page so a stale Data/Cave page's copy can't be picked.
            let menu = findChild(page, "importMenu");
            verify(menu, "found the import menu");
            menu.popupType = QC.Popup.Item;
            menu.popup(menu.parent, 0, menu.parent.height);
            tryVerify(() => menu.opened, 5000, "the import menu is open");
            settle();

            let path = WindowGrabber.grabItemToFile(menu.contentItem, "import-menu",
                                                    importExportMenuCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the import-menu shot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "import-menu is not blank");

            menu.close();
        }

        // The Export menu on a cave page, open, showing the three formats.
        // Backs docs/manual/import-export/export-surveys.md.
        //
        // On the cave page rather than the Data page: there currentCave is always
        // set (cavePageArea.currentCave), so the Compass and Chipdata items — which
        // disable on an empty cave — read enabled. The top level (Survex / Compass /
        // Chipdata) is what's grabbed; its submenus expand on hover, one at a time,
        // and the format list is the point.
        function test_exportMenu() {
            let page = openDataPage("Source/Data/Cave=Phake Cave 3000", "cavePage");
            if (!page) { return; }

            let menu = findChild(page, "exportMenu");
            verify(menu, "found the export menu");
            menu.popupType = QC.Popup.Item;
            menu.popup(menu.parent, 0, menu.parent.height);
            tryVerify(() => menu.opened, 5000, "the export menu is open");
            settle();

            let path = WindowGrabber.grabItemToFile(menu.contentItem, "export-menu",
                                                    importExportMenuCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the export-menu shot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "export-menu is not blank");

            menu.close();
        }

        // The CSV importer page with a file loaded and mapped, at Success.
        // Backs docs/manual/import-export/import-csv.md.
        //
        // defaultColumns.txt's columns are exactly the importer's default Used
        // Columns (From, To, Length, Compass, Clino), so it parses cleanly with no
        // remapping — the shot shows a working mapping and a green Status rather
        // than an error the reader would have to discount.
        function test_importCsvPage() {
            let page = openDataPage("Source/Data", "dataMainPage");
            if (!page) { return; }

            let menu = findChild(page, "importMenu");
            verify(menu, "found the import menu");
            menu.popupType = QC.Popup.Item;
            menu.popup(menu.parent, 0, menu.parent.height);
            tryVerify(() => menu.opened, 5000, "the import menu is open");

            let csvItem = findChild(menu, "csvMenuItem");
            verify(csvItem, "found the CSV menu item");
            csvItem.triggered();

            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "csvImporterPage",
                      5000, "the CSV importer page is current");
            let csvPage = RootData.pageView.currentPageItem;

            let csvManager = findChild(csvPage, "csvManager");
            verify(csvManager, "found the CSV manager");
            csvManager.filename = TestHelper.testcasesDatasetPath(
                "test_cwCSVImporterManager/defaultColumns.txt");
            tryVerify(() => csvManager.errorModel.fatalCount === 0
                      && csvManager.filename.length > 0,
                      5000, "the CSV file loaded and parsed without fatal errors");
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "import-csv");
            verify(path.length > 0, "grabToFile wrote the import-csv shot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "import-csv is not blank");

            restoreDemoProject();
        }

        // The Map page — the paper-layout tool — with its options panel.
        // Backs docs/manual/import-export/export-a-map.md.
        //
        // Grabbed with no layers added: adding one means drawing a rectangle on the
        // live 3D view, which this shot doesn't need — the paper preview and the
        // paper-size / resolution / file-type / Export options are what the page
        // documents. Needs a live QRhi for the QuickSceneView that draws the sheet.
        function test_mapPage() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            RootData.pageSelectionModel.gotoPageByName(null, "Map");
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "mapPage",
                      5000, "the Map page is current");
            let page = RootData.pageView.currentPageItem;
            if (!requireRhi(page)) { return; }

            highlightOverlayId.target = null;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "export-map");
            verify(path.length > 0, "grabToFile wrote the export-map shot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "export-map is not blank");

            restoreDemoProject();
        }

        // The Add Layer interaction: after Add Layer the app is on the View page
        // with the area-select tool over the 3D cave. Backs the "Add a layer"
        // section of docs/manual/import-export/export-a-map.md.
        //
        // Driven through the real button, which both activates the tool and
        // navigates to View — the tool is parented to the View page's renderer
        // (MapPage.view = mainContentId.renderer), so it overlays the live scene.
        // MUST deactivate the tool before returning: this runs alphabetically
        // before the test_view3d* shots, and a left-active tool would leave its
        // "Select Area" button in every one of them.
        function test_mapAddLayer() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            RootData.pageSelectionModel.gotoPageByName(null, "Map");
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "mapPage",
                      5000, "the Map page is current");
            let mapPage = RootData.pageView.currentPageItem;
            settle();

            let addLayer = findByName(mapPage, "addLayerButton");
            verify(addLayer, "found the Add Layer button");
            addLayer.clicked();

            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "Add Layer returned to the View page");

            let tool = findVisibleByName(rootId.mainWindow, "selectionToolButton");
            verify(tool, "the Select Area tool is visible over the 3D view");
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "map-add-layer");

            // Reset the tool BEFORE asserting the grab, so a failed verify can't
            // skip the cleanup and poison the View shots that run after this.
            tool.state = "DEACTIVE";
            highlightOverlayId.target = null;
            restoreDemoProject();

            verify(path.length > 0, "grabToFile wrote the map-add-layer shot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "map-add-layer is not blank");
        }

        // The cave page's Import Survex button, highlighted — the path that brings
        // a .svx (e.g. a TopoDroid centerline export) in as a new trip.
        // Backs the "Import a Survex file as a new trip" section of
        // docs/manual/import-export/import-surveys.md.
        //
        // Its own shot rather than reusing survey-add-trip.png, which rings Add
        // Trip; the section is about Import Survex, so the ring has to be on it.
        // CavePage builds a wide (table) and a narrow (flow) layout, both carrying
        // objectName "importSurvexButton" — findVisibleByName picks the shown one.
        function test_importSurvexTrip() {
            let page = openDataPage("Source/Data/Cave=Phake Cave 3000", "cavePage");
            if (!page) { return; }

            let importSurvex = findVisibleByName(page, "importSurvexButton");
            verify(importSurvex, "found the visible Import Survex button");

            highlightOverlayId.target = importSurvex;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "import-survex-trip");
            verify(path.length > 0, "grabToFile wrote the import-survex-trip shot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "import-survex-trip is not blank");

            highlightOverlayId.target = null;
        }

        // Open the demo trip and return its SurveyEditor's ListView — the whole
        // editor (trip header, calibration, team, then the data rows) is one
        // scrolling list, so `view` is both the viewport and the thing to scroll.
        // The chain mirrors tst_TripSync/tst_NoteNorthInteraction, which proves
        // there are no named items between the editor and its controls.
        function openSurveyEditorView() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.futureManagerModel.waitForFinished();

            // Neutral page first so a stale trip page is torn down.
            RootData.pageSelectionModel.currentPageAddress = "View";
            RootData.futureManagerModel.waitForFinished();
            RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Cave=Phake Cave 3000/Trip=Release 0.08";
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "tripPage",
                      5000, "the trip page is current");

            let fileLoader = findByName(rootId.mainWindow, "fileMenuButtonLoader");
            if (fileLoader) { fileLoader.active = true; }

            let view = findInTripPage("rootId->tripPage->surveyEditor->view");
            verify(view, "found the survey editor's list view");
            if (!requireRhi(view)) { return null; }

            // The editor fills in asynchronously after the trip page becomes
            // current, so settle()'s fixed 150ms is a guess about when the model
            // has its rows. Wait for the rows themselves instead.
            tryVerify(() => view.count > 0, 10000,
                      "the survey editor's rows appeared");

            // The editor's header (trip, calibration, team) lays out over several
            // frames; grabbing before it settles crops a half-built rectangle.
            settle();
            return view;
        }

        // Scroll `item` to the top of the survey editor's viewport. Nothing in
        // this editor is grabbable until it has been scrolled to: grabItemToFile
        // crops the item's sceneRect out of a full-window grab, so an item that
        // is scrolled off is cropped to an empty rect and the grab fails.
        //
        // The trip/calibration/team controls live in the ListView's *header*, and
        // a header sits ABOVE content y 0 — here originY is -664 for a 664-tall
        // header. So contentY is legitimately negative over the whole header, and
        // clamping the target to 0 scrolls the header away instead of to it.
        function scrollSurveyEditorTo(view, item) {
            let pos = item.mapToItem(view.contentItem, 0, 0);
            let minY = view.originY;
            let maxY = Math.max(minY, view.originY + view.contentHeight - view.height);
            view.contentY = Math.max(minY, Math.min(pos.y - 8, maxY));
            settle();
        }

        // The whole trip page with the survey editor ringed, so the reader can
        // place the survey table on the page before the chapter dissects it.
        // Whole-window: the point is where the editor sits relative to the note
        // gallery beside it. Backs the "Why" of survey-data/enter-survey-data.md.
        function test_tripPage() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            // Park the list at its header so the ringed editor shows the trip's
            // name and calibration rather than a mid-scroll position.
            view.positionViewAtBeginning();

            let editor = findInTripPage("rootId->tripPage->surveyEditor");
            verify(editor, "found the survey editor");

            highlightOverlayId.target = editor;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "survey-trip-page");
            verify(path.length > 0, "grabToFile wrote the trip-page screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "survey-trip-page is not blank");
        }

        // The whole window with nothing ringed — the frame the UI tour names: the
        // sidebar, the breadcrumb bar, and the page between them.
        // Backs docs/manual/getting-started/find-your-way-around.md.
        //
        // On the trip page rather than the View page the app actually opens on,
        // because the trip page is the one that fills the breadcrumb in. The tour
        // teaches reading and clicking a trail like
        // "Source / Data / Cave=... / Trip=...", and on the View page that trail
        // is a single crumb reading "View".
        function test_gettingStartedTour() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            parkSurveyEditorAtTop();

            // The previous shot rings a control and the ring is baked into the
            // grab, so a tour of the plain window has to clear it — alphabetically
            // test_gitHistory runs next, but nothing guarantees that.
            highlightOverlayId.target = null;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "getting-started-tour");
            verify(path.length > 0, "grabToFile wrote the tour screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "getting-started-tour is not blank");
        }

        // The first-run setup page — "Let's set you up!" with its two empty fields.
        // Backs docs/manual/getting-started/set-up-your-identity.md.
        //
        // Staged with welcomePageLoaderId (see its comment) rather than by clearing
        // RootData.account, so this shot neither depends on nor disturbs the shared
        // account. The loader is switched off again at the end: left active it
        // covers the window, and every grab after this one is of the whole window.
        function test_gettingStartedWelcome() {
            if (!requireRhi(rootId.mainWindow)) { return; }

            welcomePageLoaderId.active = true;
            highlightOverlayId.target = null;
            settle();

            let welcome = findByName(rootId, "welcomePage");
            verify(welcome, "the setup page loaded");

            // The empty fields are the subject: they carry the placeholder text the
            // page quotes. A Person with a name and a valid email is what the real
            // app treats as set up, so an account that leaked in here would show
            // filled fields and silently document the wrong thing.
            verify(!welcome.account.isValid,
                   "the setup page's account is empty, so the fields show placeholders");

            let path = WindowGrabber.grabItemToFile(welcomePageLoaderId.item,
                                                    "getting-started-welcome", 0);
            welcomePageLoaderId.active = false;

            verify(path.length > 0, "grabToFile wrote the welcome screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.3,
                   "getting-started-welcome is not blank");
        }

        // The trip's name and date row, ringed inside the editor's Trip section.
        // Backs the "Name and date a trip" section of
        // docs/manual/survey-data/caves-and-trips.md.
        function test_tripNameAndDate() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            let nameText = findVisibleByName(view, "tripNameText");
            verify(nameText, "found the trip name input");
            verify(findVisibleByName(view, "tripDate"), "found the trip date input");

            // tripNameText -> the name/date RowLayout -> the Trip section's
            // ColumnLayout (the "Trip" label, this row, and the error banner).
            let row = nameText.parent;
            let tripSection = row.parent;
            verify(tripSection, "found the Trip section");

            scrollSurveyEditorTo(view, tripSection);
            highlightOverlayId.target = row;
            settle();

            let path = WindowGrabber.grabItemToFile(tripSection, "survey-trip-name-date",
                                                    panelCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the trip name/date screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "survey-trip-name-date is not blank");
        }

        // The interleaved station/shot table. Scrolls the trip header off so the
        // grab is the data rows and their column titles — the layout that
        // docs/manual/survey-data/enter-survey-data.md has to explain.
        function test_surveyShotTable() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            // Index 0 is the first row below the header, so this parks the data
            // rows (and the sticky column titles) at the top of the viewport.
            view.positionViewAtIndex(0, QQ.ListView.Beginning);
            highlightOverlayId.target = null;
            settle();

            let path = WindowGrabber.grabItemToFile(view, "survey-shot-table", 0);
            verify(path.length > 0, "grabItemToFile wrote the shot-table screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "survey-shot-table is not blank");
        }

        // The "Press Space to add another data block" bar, ringed in the editor's
        // footer. Backs the "Start a new data block" section of
        // docs/manual/survey-data/enter-survey-data.md.
        function test_addDataBlock() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            // The bar is in the ListView's footer, below every data row.
            // positionViewAtEnd() aligns the last *item*, not the footer, and
            // leaves the footer hanging ~18px past the window's bottom edge —
            // grabItemToFile then clips the crop there and cuts the button in
            // half. Scroll to the true maximum instead.
            // Applied twice: the first scroll can change the header's layout,
            // which moves originY/contentHeight, leaving the target stale.
            view.contentY = view.originY + view.contentHeight - view.height;
            settle();
            view.contentY = view.originY + view.contentHeight - view.height;
            settle();

            let spaceBar = findVisibleByName(view, "spaceAddBar");
            verify(spaceBar, "found the add-data-block bar");

            // spaceAddBar -> the footer ColumnLayout, which also carries the
            // trip's Total Length. Cropping the footer keeps the shot readable;
            // cropping the whole viewport would be mostly survey rows.
            let footer = spaceBar.parent;
            verify(footer, "found the editor footer");

            highlightOverlayId.target = spaceBar;
            settle();

            let path = WindowGrabber.grabItemToFile(footer, "survey-add-data-block",
                                                    panelCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the add-data-block screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "survey-add-data-block is not blank");
        }

        // The Distance cell's caret menu, open on "Exclude Distance". Backs the
        // "Exclude a distance from the total" section of
        // docs/manual/survey-data/enter-survey-data.md.
        //
        // The menu is forced to popupType Item for the grab. A QC.Menu's type is
        // the style's choice, and Fusion sets none, so it defaults to
        // Popup.Window — a separate top-level window that grabWindow(mainWindow)
        // cannot see. Popup.Item draws the menu into this window's overlay from
        // the same QML delegates Popup.Window would use, so the menu a reader
        // sees here is pixel-for-pixel the one the app shows; only the windowing
        // differs. Overridden here rather than in ShotDistanceDataBox.qml: it is
        // the screenshot that needs this, not the app.
        function test_excludeDistance() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            let model = view.model;
            verify(model, "found the survey editor model");

            let chunk = model.chunkForRow(0);
            verify(chunk, "found the trip's first data block");

            let row = model.modelRowForChunkRole(chunk, 0,
                                                 SurveyChunk.ShotDistanceRole);
            verify(row >= 0, "found the model row of the first shot's Distance");

            // Focus the cell rather than clicking it: the caret button's Loader is
            // gated on the box holding focus.
            model.setFocusedCell(model.cellIndex(row, SurveyChunk.ShotDistanceRole));
            settle();

            // Park the data rows at the top of the viewport, which fixes where the
            // cell — and so the crop taken around its menu — lands. Focusing alone
            // does not: the editor only scrolls a focused cell into view when it is
            // off-screen, so on its own this shot came out framed differently in a
            // full run than it did on its own.
            parkSurveyEditorAtRow(view, 0);

            let box = null;
            tryVerify(() => {
                box = findVisibleByName(view, "dataBox." + row + "."
                                        + SurveyChunk.ShotDistanceRole);
                return box !== null && box.focus;
            }, 5000, "the first shot's Distance cell has focus");

            let caret = null;
            tryVerify(() => {
                caret = findVisibleByName(box, "excludeMenuButton");
                return caret !== null;
            }, 5000, "the Distance cell's caret button is visible");
            settle();

            let menuLoader = findByName(caret, "menuLoader");
            verify(menuLoader, "found the caret button's menu loader");
            menuLoader.active = true;
            let menu = menuLoader.item;
            verify(menu, "the caret menu loaded");
            menu.popupType = QC.Popup.Item;

            // Anchor the menu under the caret button. The button's own handler
            // calls popup() with no coordinates, which lands the menu at the mouse
            // cursor — a position a test doesn't control.
            menu.popup(caret, 0, caret.height);
            tryVerify(() => menu.opened, 5000, "the caret menu is open");

            highlightOverlayId.target = box;
            settle();

            let path = WindowGrabber.grabItemToFile(menu.contentItem,
                                                    "survey-exclude-distance",
                                                    excludeMenuCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the exclude-distance shot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "survey-exclude-distance is not blank");

            menu.close();
            highlightOverlayId.target = null;
        }

        // A cell carrying a fatal error: red border, stop-sign badge, and the
        // badge's message open beside it. Backs the "Why" of
        // docs/manual/survey-data/survey-errors.md.
        //
        // The error is made here rather than borrowed from the fixture. Phake's
        // own errors are four warnings, and a shot documenting what an *error*
        // looks like should not depend on which incidental warning the demo data
        // happens to carry. Blanking a shot's Distance is the app's own fatal
        // case — cwSurveyChunk rates an empty distance on a normal shot Fatal,
        // and only warns on an LRUD-only one — and it is what a skipped reading
        // actually looks like. Safe to mutate: every shot reloads the project
        // from the dataset into a fresh temp dir.
        function test_surveyError() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            let model = view.model;
            verify(model, "found the survey editor model");

            let chunk = model.chunkForRow(0);
            verify(chunk, "found the trip's first data block");
            let row = model.modelRowForChunkRole(chunk, 0,
                                                 SurveyChunk.ShotDistanceRole);
            verify(row >= 0, "found the model row of the first shot's Distance");

            chunk.setData(SurveyChunk.ShotDistanceRole, 0, "");
            settle();

            parkSurveyEditorAtRow(view, 0);

            let box = findVisibleByName(view, "dataBox." + row + "."
                                        + SurveyChunk.ShotDistanceRole);
            verify(box, "found the first shot's Distance cell");

            // Open the message. The badge is a checkable button, and the box it
            // shows is a plain in-scene item (QuoteBox reparented to
            // RootPopupItem), so grabWindow captures it — no popupType override
            // like test_excludeDistance needs.
            let badge = null;
            tryVerify(() => {
                badge = findVisibleByName(box, "errorIcon");
                return badge !== null;
            }, 5000, "the Distance cell shows an error badge");
            badge.checked = true;

            // Crop around the message's own text rather than the QuoteBox that
            // frames it. QuoteBox is a ZERO-SIZED Item parked at the triangle's
            // tip that draws its body around itself, partly at negative
            // coordinates — so its rect is empty, and grabItemToFile (which crops
            // an item's sceneRect) would fail on it outright. errorText is the
            // innermost named item, and padding it reaches back over the cell.
            let quote = null;
            tryVerify(() => {
                let root = findVisibleByName(rootId.mainWindow,
                                             "errorBox" + box.objectName);
                quote = root ? findVisibleByName(root, "errorText") : null;
                return quote !== null && quote.width > 0;
            }, 5000, "the error message is open");
            settle();

            let path = WindowGrabber.grabItemToFile(quote, "survey-error",
                                                    errorQuoteCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the survey-error shot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "survey-error is not blank");

            badge.checked = false;
        }

        // The Calibration section whole: declination, tape, and the front/back
        // sight boxes. Reached through frontSightCalibrationEditor's parents
        // rather than by naming CalibrationEditor's root — an objectName there
        // would insert a link into "…->view->declinationEdit" and break the
        // chains tst_TripSync and tst_NoteNorthInteraction already record.
        function test_surveyCalibration() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            let frontSights = findVisibleByName(view, "frontSightCalibrationEditor");
            verify(frontSights, "found the front-sight calibration editor");

            // frontSightCalibrationEditor -> sights Flow -> CalibrationEditor's
            // ColumnLayout, which also holds the "Calibration" label and the
            // declination/tape row.
            let section = frontSights.parent.parent;
            verify(section, "found the calibration section");

            scrollSurveyEditorTo(view, section);
            highlightOverlayId.target = null;
            settle();

            let path = WindowGrabber.grabItemToFile(section, "survey-calibration",
                                                    panelCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the calibration screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "survey-calibration is not blank");
        }

        // The Declination row, highlighted inside the calibration section.
        //
        // The demo cave has no fix station, so autoDeclinationAvailable is false
        // and the Auto/Manual picker is hidden by design — the value stands
        // alone. That is the state declination.md describes for a cave that
        // hasn't been georeferenced yet, so the shot is left that way rather
        // than fabricating a fix station to force the picker into view.
        function test_surveyDeclination() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            let declinationEdit = findVisibleByName(view, "declinationEdit");
            verify(declinationEdit, "found the declination edit");

            let frontSights = findVisibleByName(view, "frontSightCalibrationEditor");
            verify(frontSights, "found the front-sight calibration editor");
            let section = frontSights.parent.parent;

            scrollSurveyEditorTo(view, section);
            // The row (label + optional picker + value), not the bare value: a
            // ring around "0" alone reads as a highlight of nothing.
            highlightOverlayId.target = declinationEdit.parent;
            settle();

            let path = WindowGrabber.grabItemToFile(section, "survey-declination",
                                                    panelCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the declination screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "survey-declination is not blank");
        }

        // The Team section. TeamTable collapses to a "NoTeam" state when the
        // trip has no members, so this asserts the demo trip actually has one
        // rather than silently shooting an empty box.
        function test_surveyTeam() {
            let view = openSurveyEditorView();
            if (!view) { return; }

            let teamTable = findVisibleByName(view, "teamTable");
            verify(teamTable, "found the team table");
            verify(teamTable.state !== "NoTeam",
                   "the demo trip has a team to shoot (state is " + teamTable.state + ")");

            scrollSurveyEditorTo(view, teamTable);

            // Select the first member. The row's remove button, its outline and
            // the green "+ Role" button all bind `visible: rowDelegate.selected`,
            // so an unselected row shows none of them — and caves-and-trips.md
            // describes the "+ Role" button by name. TeamTable resets
            // currentIndex to -1 on every model change, so which state the grab
            // caught used to depend on what ran before it.
            let teamRow = findVisibleByName(teamTable, "teamRow.0");
            verify(teamRow, "found the first team row");
            mouseClick(teamRow);
            tryVerify(() => teamRow.selected, 5000, "the first team member is selected");

            highlightOverlayId.target = null;
            settle();

            let path = WindowGrabber.grabItemToFile(teamTable, "survey-team",
                                                    panelCropMargin);
            verify(path.length > 0, "grabItemToFile wrote the team screenshot");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "survey-team is not blank");
        }
    }
}
