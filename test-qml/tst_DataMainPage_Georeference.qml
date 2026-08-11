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
            // The page is cached between tests, and so is the dialog on it. A
            // modal dialog left open would eat the next test's clicks.
            const dialog = findChild(dataPage(), "projectionCenterDialog")
            if (dialog !== null) {
                dialog.close()
            }

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

        // Fills row \a row of the first cave's fix stations, offset from the
        // fixture coordinate so each station lands somewhere of its own.
        function addFix(row, name, eastingOffset, northingOffset) {
            const cave = RootData.region.cave(0)
            cave.fixStations.addFixStation()
            const idx = cave.fixStations.index(row)
            cave.fixStations.setData(idx, name, FixStationModel.StationNameRole)
            cave.fixStations.setData(idx, utm16N, FixStationModel.InputCSRole)
            cave.fixStations.setData(idx, easting + eastingOffset, FixStationModel.EastingRole)
            cave.fixStations.setData(idx, northing + northingOffset, FixStationModel.NorthingRole)
            cave.fixStations.setData(idx, 300.0, FixStationModel.ElevationRole)
        }

        function addUtm16NFix() {
            RootData.region.addCave()
            addFix(0, "entrance", 0.0, 0.0)
        }

        // Two stations a kilometer apart: one the frame anchors on, one it
        // doesn't, which is the smallest project the picker has a choice in.
        function addTwoFixes() {
            addUtm16NFix()
            addFix(1, "back door", 1000.0, 1000.0)
        }

        function editButton() {
            const found = findChild(dataPage(), "regionSettingsEditButton")
            verify(found !== null, "edit toggle must exist")
            return found
        }

        // Waiting for the frame to arrive is part of the picker being openable
        // at all: the button shows only once something has placed the project,
        // which makes it the thing to wait on.
        function openProjectionCenterDialog() {
            editButton().editMode = true

            const button = label("recenterButton")
            tryVerify(() => button.visible, 5000,
                      "the frame must be derived before it can be moved")
            mouseClick(button)

            const dialog = findChild(dataPage(), "projectionCenterDialog")
            verify(dialog !== null, "the picker must exist")
            tryVerify(() => dialog.opened, 3000, "the picker must open")
            return dialog
        }

        // The picker's rows, addressed by station name. findChild() reaches the
        // list but not the items a ListView creates inside it, so ask the list
        // for them.
        function candidateRow(dialog, stationName) {
            const list = findChild(dialog, "recenterCandidateList")
            verify(list !== null, "the candidate list must exist")
            for (let i = 0; i < list.count; ++i) {
                const row = list.itemAtIndex(i)
                if (row !== null && row.objectName === "recenterCandidate " + stationName) {
                    return row
                }
            }
            return null
        }

        // The subtle second line of a row: where it is, and why it can't be
        // picked.
        function detailOf(row) {
            const detail = findChild(row, "centerOptionDetail")
            verify(detail !== null, "every row carries a detail line")
            return detail.text
        }

        function acceptButton(dialog) {
            const button = findChild(dialog, "projectionCenterAcceptButton")
            verify(button !== null, "the Center button must exist")
            return button
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
            editButton().editMode = true

            verify(label("projectionDatumValue").visible,
                   "the datum stays a label in edit mode")
            verify(label("projectionOriginValue").visible,
                   "the location stays a label in edit mode")

            editButton().editMode = false
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
            addTwoFixes()

            tryVerify(() => label("projectionAnchorValue").visible, 5000)

            RootData.region.cave(0).fixStations.removeFixStation("entrance")

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

        // Reading where the project landed is one thing and moving it is
        // another, so the picker is reachable only from edit mode.
        function test_recenteringIsOfferedOnlyInEditMode() {
            addUtm16NFix()
            tryVerify(() => label("projectionDatumValue").visible, 5000)

            const button = label("recenterButton")
            verify(!button.visible, "the read-only box shows the frame, it doesn't move it")

            editButton().editMode = true
            verify(button.visible, "edit mode offers to recenter a georeferenced project")

            editButton().editMode = false
        }

        // Recentering re-derives a frame that already exists; it is never how
        // the first one gets made.
        function test_anUngeoreferencedProjectHasNothingToRecenter() {
            editButton().editMode = true

            verify(!label("recenterButton").visible,
                   "a project nothing has placed has no frame to move")

            editButton().editMode = false
        }

        // Centering re-projects the whole project, so a click on a row only
        // selects it. Nothing moves until Center is pressed.
        function test_aRowIsSelectedAndTheCenterButtonCommitsIt() {
            addTwoFixes()

            const caveName = RootData.region.cave(0).name
            const originBefore = label("projectionOriginValue").text
            const dialog = openProjectionCenterDialog()

            verify(!acceptButton(dialog).enabled,
                   "Center has nothing to act on until a row is chosen")

            const picked = candidateRow(dialog, "back door")
            verify(picked !== null, "the other station must be offered")
            mouseClick(picked)

            verify(picked.highlighted, "the chosen row says it is chosen")
            verify(acceptButton(dialog).enabled, "a chosen row is something Center can act on")
            compare(label("projectionOriginValue").text, originBefore,
                    "choosing a row moves nothing on its own")

            mouseClick(acceptButton(dialog))

            tryCompare(label("projectionAnchorValue"), "text",
                       "back door — " + caveName, 5000)
            verify(label("projectionOriginValue").text !== originBefore,
                   "the origin moved onto the chosen station")
            tryVerify(() => !dialog.opened, 3000, "centering closes the picker")
        }

        // The rows are drawn by the list, the border by the frame around it.
        // Filling the frame edge to edge puts a row's highlight on top of that
        // border.
        function test_theRowsStayInsideTheirFrame() {
            addUtm16NFix()

            const dialog = openProjectionCenterDialog()
            // The rows all sit in the column the list and the data-center row
            // share, so it is the column that has to clear the border.
            const rows = findChild(dialog, "recenterCandidateList").parent
            const frame = rows.parent

            verify(rows.x > 0 && rows.y > 0
                   && rows.x + rows.width < frame.width
                   && rows.y + rows.height < frame.height,
                   "the rows stay inside the frame: rows at " + rows.x + "," + rows.y
                   + " sized " + rows.width + "x" + rows.height
                   + " in a frame " + frame.width + "x" + frame.height)
        }

        // A station's coordinate is what tells two "entrance" rows apart, and
        // what catches a fix typed into the wrong zone before the whole project
        // is centered on it.
        function test_everyRowSaysWhereItIs() {
            addUtm16NFix()

            const dialog = openProjectionCenterDialog()

            // Six decimals of two signed degrees, comma separated, on the fix.
            const coordinate = /(-?\d+\.\d{6}), (-?\d+\.\d{6})/
            const station = detailOf(candidateRow(dialog, "entrance"))
            const parts = coordinate.exec(station)
            verify(parts !== null, "a station row reads as a latitude, longitude pair: " + station)
            verify(parseFloat(parts[1]) > 36.0 && parseFloat(parts[1]) < 38.0,
                   "latitude comes first and lands on the fix: " + parts[1])

            const dataCenter = findChild(dialog, "recenterDataCenterRow")
            verify(dataCenter !== null, "the middle of the data must be offered")
            verify(coordinate.test(detailOf(dataCenter)),
                   "the middle of the data says where it is too: " + detailOf(dataCenter))
        }

        // The station the frame already sits on stays listed and says so, rather
        // than going missing from the project's own list.
        function test_theCurrentCenterIsListedAndSaysSo() {
            addTwoFixes()

            const dialog = openProjectionCenterDialog()
            const anchored = candidateRow(dialog, "entrance")
            verify(anchored !== null, "the current anchor is still a row")
            verify(!anchored.enabled, "centering on the current anchor is a no-op")
            verify(detailOf(anchored).indexOf("Projection's center") >= 0,
                   "the current anchor says which one it is: " + detailOf(anchored))

            const other = candidateRow(dialog, "back door")
            verify(other.enabled, "a station beside the data is a fine place to center")
            verify(detailOf(other).indexOf("Projection's center") < 0,
                   "only one row is the projection's center: " + detailOf(other))
        }

        function test_centeringOnTheMiddleOfTheDataFreezesTheFrameThere() {
            addTwoFixes()

            const dialog = openProjectionCenterDialog()
            const dataCenter = findChild(dialog, "recenterDataCenterRow")
            verify(dataCenter !== null, "the middle of the data must be offered")
            mouseClick(dataCenter)
            mouseClick(acceptButton(dialog))

            tryCompare(RootData.region.geoReference, "state", GeoReference.Frozen, 5000)

            // The middle of the data is no station, so there is nothing for the
            // attribution to name.
            verify(!label("projectionAnchorValue").visible,
                   "a frame centered on the data has no anchor to name")
            verify(label("projectionDatumValue").visible,
                   "the frame is still there, so the datum row stays")
            tryVerify(() => !dialog.opened, 3000, "centering closes the picker")
        }

        // A station the project's data sits far from stays listed and says why
        // it can't be picked. The distance it names is the projection's own
        // reach, read from it rather than restated beside it, so tuning the
        // reach can't leave the picker quoting the old number.
        function test_anOutOfReachStationSaysHowFarIsTooFar() {
            addUtm16NFix()
            addFix(1, "far entrance", 0.0, 200000.0)

            const dialog = openProjectionCenterDialog()
            const far = candidateRow(dialog, "far entrance")
            verify(far !== null, "a station out of reach stays in the list")
            verify(!far.enabled, "centering that far off gives a worse frame")

            const reachKm = RootData.region.localProjection.anchorThresholdMeters / 1000
            verify(detailOf(far).indexOf(reachKm + " km") >= 0,
                   "the row names the projection's own reach of " + reachKm
                   + " km: " + detailOf(far))
        }

        // Centering on the middle of the data freezes the frame with no anchor
        // left behind it, so this row can't answer "you are already here" the
        // way a station row does. Offering it again would re-derive the frame
        // the project already has and reload every point cloud for nothing.
        function test_theMiddleOfTheDataSaysWhenTheFrameIsAlreadyOnIt() {
            addTwoFixes()

            let dialog = openProjectionCenterDialog()
            let middle = findChild(dialog, "recenterDataCenterRow")
            verify(middle.enabled, "the middle of the data is a move worth offering")
            verify(detailOf(middle).indexOf("Projection's center") < 0,
                   "an anchored frame is not centered on the data: " + detailOf(middle))

            mouseClick(middle)
            mouseClick(acceptButton(dialog))
            tryCompare(RootData.region.geoReference, "state", GeoReference.Frozen, 5000)
            tryVerify(() => !dialog.opened, 3000, "centering closes the picker")

            dialog = openProjectionCenterDialog()
            middle = findChild(dialog, "recenterDataCenterRow")
            verify(!middle.enabled,
                   "centering on the middle again would re-derive the frame it has")
            verify(detailOf(middle).indexOf("Projection's center") >= 0,
                   "the row says the projection is already there: " + detailOf(middle))
        }

        // The rows are the project as it stood when the picker opened. A choice
        // the project has since lost is refused, and a refusal that closed the
        // picker would read exactly like a recentering that worked.
        function test_aChoiceTheProjectHasLostIsRefusedAndSaysSo() {
            addTwoFixes()

            const dialog = openProjectionCenterDialog()
            const originBefore = label("projectionOriginValue").text
            mouseClick(candidateRow(dialog, "back door"))

            // Taking away the coordinate system takes away the only thing that
            // could place the station, which is what recentering refuses on.
            const fixStations = RootData.region.cave(0).fixStations
            fixStations.setData(fixStations.index(1), "", FixStationModel.InputCSRole)

            mouseClick(acceptButton(dialog))

            verify(dialog.opened, "a refused choice leaves the picker open")
            const warning = findChild(dialog, "projectionCenterStaleWarning")
            verify(warning !== null && warning.visible,
                   "the picker says why nothing moved")
            compare(label("projectionOriginValue").text, originBefore,
                    "the frame stayed where it was")
            verify(candidateRow(dialog, "back door") === null,
                   "the rows are drawn again without the station the project lost")
        }

        // Reopening starts over rather than resuming whatever was chosen and
        // abandoned last time.
        function test_cancelingLeavesTheFrameAloneAndClearsTheChoice() {
            addTwoFixes()

            const originBefore = label("projectionOriginValue").text
            let dialog = openProjectionCenterDialog()
            mouseClick(candidateRow(dialog, "back door"))

            const cancel = findChild(dialog, "projectionCenterCancelButton")
            verify(cancel !== null, "the Cancel button must exist")
            mouseClick(cancel)

            tryVerify(() => !dialog.opened, 3000, "Cancel closes the picker")
            compare(label("projectionOriginValue").text, originBefore,
                    "Cancel leaves the frame where it was")

            dialog = openProjectionCenterDialog()
            verify(!acceptButton(dialog).enabled,
                   "the picker opens with nothing chosen")
        }
    }
}
