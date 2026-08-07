import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// The read-only frame readout on the Data page: where the project's local
// projection is centered, the datum it inherited, and the vertical datum its
// elevations are reported against.
MainWindowTest {
    id: rootId

    TestCase {
        name: "DataMainPageGeoreference"
        when: windowShown

        // A cave area in eastern Kentucky, typed in NAD83 UTM zone 16N — the
        // same place test_cwLocalProjection anchors on, near 37.18N 84.09W.
        readonly property string utm16N: "EPSG:26916"
        readonly property real easting: 757000.0
        readonly property real northing: 4117000.0

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000)
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function dataPage() {
            let page = RootData.pageView.currentPageItem
            verify(page !== null, "data page must exist")
            return page
        }

        function label(name) {
            const found = findChild(dataPage(), name)
            verify(found !== null, name + " must exist")
            return found
        }

        function addUtm16NFix() {
            RootData.region.addCave()
            const cave = RootData.region.cave(0)
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(0)
            cave.fixStations.setData(idx, "entrance", FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, utm16N, FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, easting, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, northing, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, 300.0, FixStationModel.ElevationRole)
        }

        function test_emptyProjectSaysItIsNotGeoreferenced() {
            compare(label("projectionOriginValue").text, "Not georeferenced")

            // Nothing placed the project, so there is no datum it could have
            // inherited and nothing to show a row for.
            verify(!label("projectionDatumValue").visible,
                   "the datum row stays hidden until a frame exists")
        }

        function test_aFixStationGivesTheProjectALocationAndADatum() {
            addUtm16NFix()

            // The frame is derived off the fix landing, so the readout arrives a
            // beat later than the fix does.
            tryVerify(() => label("projectionOriginValue").text !== "Not georeferenced",
                      5000, "a fix station must give the project a location")

            // Six decimals of two signed degrees, comma separated.
            const origin = label("projectionOriginValue").text
            const parts = /^(-?\d+\.\d{6}), (-?\d+\.\d{6})$/.exec(origin)
            verify(parts !== null,
                   "location reads as a latitude, longitude pair: " + origin)

            // Latitude first, and on the fix station the frame was derived from.
            // A transposed pair puts the latitude at -84, which is not one.
            const latitude = parseFloat(parts[1])
            const longitude = parseFloat(parts[2])
            verify(latitude > 36.0 && latitude < 38.0,
                   "latitude comes first and lands on the fix: " + latitude)
            verify(longitude > -85.0 && longitude < -83.0,
                   "longitude comes second and lands on the fix: " + longitude)

            const datum = label("projectionDatumValue")
            verify(datum.visible, "the datum row shows once there is a frame")
            compare(datum.text, "North American Datum 1983")
        }

        function test_theDatumIsReadOnly() {
            addUtm16NFix()
            tryVerify(() => label("projectionDatumValue").visible, 5000)

            // The datum is inherited from the input that anchored the project,
            // never chosen — nothing here may become an editor when the Project
            // box is put into edit mode.
            const editButton = findChild(dataPage(), "regionSettingsEditButton")
            verify(editButton !== null, "edit toggle must exist")
            editButton.editMode = true

            verify(label("projectionDatumValue").visible,
                   "the datum stays a label in edit mode")
            verify(label("projectionOriginValue").visible,
                   "the location stays a label in edit mode")

            editButton.editMode = false
        }

        function test_verticalDatumShowsOnlyWhenSomethingDeclaredOne() {
            addUtm16NFix()
            tryVerify(() => label("projectionDatumValue").visible, 5000)

            verify(!label("projectionVerticalDatumValue").visible,
                   "a typed fix station declares no vertical datum")

            RootData.region.geoReference.verticalDatum = "NAVD88"

            const vertical = label("projectionVerticalDatumValue")
            tryVerify(() => vertical.visible, 3000,
                      "a declared vertical datum earns a row")
            compare(vertical.text, "NAVD88")
        }
    }
}
