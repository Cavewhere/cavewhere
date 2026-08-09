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
            // inherited, no elevations to describe, and nothing centering it.
            verify(!label("projectionDatumValue").visible,
                   "the datum row stays hidden until a frame exists")
            verify(!label("projectionVerticalDatumValue").visible,
                   "the elevations row stays hidden until a frame exists")
            verify(!label("projectionAnchorValue").visible,
                   "nothing anchors an ungeoreferenced project")
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
            // never chosen — the projection rows stay labels when the Project
            // box is put into edit mode, however editable Units becomes.
            const editButton = findChild(dataPage(), "regionSettingsEditButton")
            verify(editButton !== null, "edit toggle must exist")
            editButton.editMode = true

            verify(label("projectionDatumValue").visible,
                   "the datum stays a label in edit mode")
            verify(label("projectionOriginValue").visible,
                   "the location stays a label in edit mode")

            editButton.editMode = false
        }

        function test_theElevationsRowSaysSoWhenNothingDeclaredAVerticalDatum() {
            addUtm16NFix()
            tryVerify(() => label("projectionDatumValue").visible, 5000)

            // The row stays put and says what it knows. Vanishing would leave
            // the reader unable to tell "your data never said" from "CaveWhere
            // forgot to show it".
            const vertical = label("projectionVerticalDatumValue")
            verify(vertical.visible,
                   "the elevations row shows once there is a frame")
            compare(vertical.text, "Not declared by your data")

            RootData.region.geoReference.verticalDatum = "NAVD88"

            tryCompare(vertical, "text", "NAVD88", 3000)
        }

        function test_theProjectSaysWhichStationItIsCenteredOn() {
            addUtm16NFix()

            const anchor = label("projectionAnchorValue")
            tryVerify(() => anchor.visible, 5000,
                      "an anchored frame names the input it came from")
            compare(anchor.text, "entrance — " + RootData.region.cave(0).name)
        }

        function test_theProjectionFactsShareOneGroup() {
            // Every projection fact reads as one block. GIS Sources is a link to
            // another page, not a setting, so it stays outside.
            const group = label("coordinateSystemGroup")
            const inGroup = name => {
                verify(findChild(group, name) !== null,
                       name + " must sit inside the Coordinate System group")
            }
            inGroup("unitSystemValue")
            inGroup("projectionOriginValue")
            inGroup("projectionAnchorValue")
            inGroup("projectionDatumValue")
            inGroup("projectionVerticalDatumValue")

            // Edit unlocks Units and nothing else, so it belongs to the group
            // rather than to the Project box around it.
            inGroup("regionSettingsEditButton")

            verify(findChild(group, "geospatialLayersLink") === null,
                   "GIS Sources is a link to another page, not a projection fact")
        }

        // A custom label replaces the one the style positions and measures, so
        // both jobs are done by hand and both can go wrong: the title starts at
        // the control's edge instead of the frame's, and the group reserves too
        // little room for it and prints it over the first row.
        function test_theGroupTitleLinesUpWithItsRows() {
            const group = label("coordinateSystemGroup")
            const title = label("coordinateSystemTitle")
            const edit = label("regionSettingsEditButton")

            compare(group.mapFromItem(title, 0, 0).x, group.contentItem.x,
                    "the title starts where the rows inside the frame do")

            const clears = item => {
                const bottom = group.mapFromItem(item, 0, item.height).y
                verify(bottom <= group.contentItem.y,
                       "the title line clears the rows: " + item.objectName
                       + " ends at " + bottom
                       + ", the rows start at " + group.contentItem.y)
            }
            clears(title)
            clears(edit)
        }

        // Deleting the anchor of a project that has a second georeferenced
        // station hands the frame off to Frozen: it keeps the projection but has
        // no input answerable for where it sits, so there is no one station the
        // attribution could name.
        function test_aFrozenFrameNamesNothing() {
            addUtm16NFix()
            const cave = RootData.region.cave(0)
            cave.fixStations.addFixStation()
            const second = cave.fixStations.index(1)
            cave.fixStations.setData(second, "back door", FixStationModel.StationNameRole)
            cave.fixStations.setData(second, utm16N, FixStationModel.InputCSRole)
            cave.fixStations.setData(second, easting + 1000.0, FixStationModel.EastingRole)
            cave.fixStations.setData(second, northing + 1000.0, FixStationModel.NorthingRole)
            cave.fixStations.setData(second, 320.0, FixStationModel.ElevationRole)

            tryVerify(() => label("projectionAnchorValue").visible, 5000)

            cave.fixStations.removeFixStation("entrance")

            tryCompare(RootData.region.geoReference, "state", GeoReference.Frozen, 5000)
            verify(!label("projectionAnchorValue").visible,
                   "a frozen frame has no anchor to name")
            verify(label("projectionDatumValue").visible,
                   "the frame is still there, so the datum row stays")
        }

        function test_theCenteredOnRowFollowsACaveRename() {
            addUtm16NFix()
            tryVerify(() => label("projectionAnchorValue").visible, 5000)

            RootData.region.cave(0).name = "Roppel Cave"

            tryCompare(label("projectionAnchorValue"), "text",
                       "entrance — Roppel Cave", 3000)
        }
    }
}
