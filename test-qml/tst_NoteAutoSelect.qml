import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    SurveyNotesConcatModel {
        id: tempNotesModel
    }

    SignalSpy {
        id: insertSpy
        target: tempNotesModel
        signalName: "rowsInserted"
    }

    TestCase {
        name: "NoteAutoSelect"
        when: windowShown

        readonly property string tripAddress: "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

        // Mid-width: the NotesGallery loader is active (isNarrow=false) and
        // the SurveyEditor note thumbnails are shown (showNotes = !isWide).
        function init() {
            rootId.width = 900

            TestHelper.loadProjectFromZip(RootData.project,
                TestHelper.testcasesDatasetPath("lidarProjects/jaws of the beast.zip"))

            RootData.pageSelectionModel.currentPageAddress = tripAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })
            waitForRendering(rootId)

            let tripPageItem = RootData.pageView.currentPageItem
            tempNotesModel.trip = tripPageItem.currentTrip
            verify(tempNotesModel.trip !== null)

            insertSpy.clear()
        }

        function cleanup() {
            tempNotesModel.trip = null
            RootData.pageSelectionModel.currentPageAddress = "View"
            waitForRendering(rootId)
        }

        function findNoteGallery() {
            let ng = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery")
            verify(ng !== null, "noteGallery must exist")
            return ng
        }

        function findSurveyEditor() {
            let se = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->surveyEditor")
            verify(se !== null, "surveyEditor must exist")
            return se
        }

        function firstInsertedRow() {
            // rowsInserted(QModelIndex parent, int first, int last)
            return insertSpy.signalArguments[0][1]
        }

        function addFilesViaGallery(url) {
            let noteGallery = findNoteGallery()
            insertSpy.clear()
            noteGallery.addFiles([url])
            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)
            tryVerify(() => { return insertSpy.count >= 1 }, 10000,
                      "rowsInserted should fire after addFiles")
        }

        function test_addPng_selectsNewNote() {
            let noteGallery = findNoteGallery()
            let surveyEditor = findSurveyEditor()

            let url = TestHelper.toLocalUrl(
                TestHelper.testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"))
            addFilesViaGallery(url)

            let newRow = firstInsertedRow()
            tryCompare(noteGallery, "currentNoteIndex", newRow,
                       5000, "NotesGallery should select the newly added PNG")
            tryCompare(surveyEditor, "currentNoteIndex", newRow,
                       5000, "SurveyEditor should reflect the new PNG selection")
            tryVerify(() => { return noteGallery.currentNote !== null },
                      5000, "currentNote should resolve")
        }

        function test_addPdfMultipage_selectsFirstPage() {
            if (!TestHelper.pdfSupported()) {
                skip("Qt Pdf not built — PDF note import is a no-op without it")
            }

            let noteGallery = findNoteGallery()
            let surveyEditor = findSurveyEditor()

            let url = TestHelper.toLocalUrl(
                TestHelper.testcasesDatasetPath("test_cwPDFConverter/2page-test.pdf"))
            addFilesViaGallery(url)

            // The import can arrive as a single batch or page-by-page; either
            // way the first inserted row IS the first page of the PDF.
            let firstPageRow = firstInsertedRow()

            // Wait until both pages have been inserted before asserting
            // selection — guarantees the handler did not drift to page 2.
            tryVerify(() => {
                // Count rows that were added during this test run.
                let added = 0
                for (let i = 0; i < insertSpy.count; i++) {
                    added += insertSpy.signalArguments[i][2]
                           - insertSpy.signalArguments[i][1] + 1
                }
                return added >= 2
            }, 15000, "Both PDF pages should have been inserted")

            tryCompare(noteGallery, "currentNoteIndex", firstPageRow,
                       5000, "Gallery should select the first page of the PDF")
            tryCompare(surveyEditor, "currentNoteIndex", firstPageRow,
                       5000, "SurveyEditor should reflect the first PDF page")
        }

        function test_addGlbLiDAR_selectsNewNote() {
            let noteGallery = findNoteGallery()
            let surveyEditor = findSurveyEditor()

            let url = TestHelper.toLocalUrl(
                TestHelper.testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"))
            addFilesViaGallery(url)

            let newRow = firstInsertedRow()
            tryCompare(noteGallery, "currentNoteIndex", newRow,
                       5000, "NotesGallery should select the newly added GLB")
            tryCompare(surveyEditor, "currentNoteIndex", newRow,
                       5000, "SurveyEditor should reflect the new GLB selection")
            tryVerify(() => { return noteGallery.currentNoteLiDAR !== null },
                      5000, "currentNoteLiDAR should resolve to the new GLB")
        }

        function test_addSketch_selectsNewSketch() {
            let noteGallery = findNoteGallery()
            let surveyEditor = findSurveyEditor()

            insertSpy.clear()
            noteGallery.addSketch()
            tryVerify(() => { return insertSpy.count >= 1 }, 5000,
                      "sketch row should be added")

            let newRow = firstInsertedRow()
            tryCompare(noteGallery, "currentNoteIndex", newRow,
                       5000, "NotesGallery should select the newly added sketch")
            tryCompare(surveyEditor, "currentNoteIndex", newRow,
                       5000, "SurveyEditor should reflect the new sketch selection")
            tryVerify(() => { return noteGallery.currentSketch !== null },
                      5000, "currentSketch should resolve")
        }
    }
}
