/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQml
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQml.Models
import cavewherelib

StandardPage {
    id: fixStationPage
    objectName: "fixStationPage"

    property SurveyNode cave

    // The wide layout is a fixed-width table and nothing scrolls it sideways, so
    // it may only stay up while the whole row fits: every column, the two
    // trailing warning icons with their gaps, the page's margins, and room for
    // the vertical scrollbar.
    readonly property real wideMinimumWidth: columnModelId.totalWidth
                                             + 2 * (Theme.iconSizeButton + Theme.tightSpacing)
                                             + 2 * Theme.pageMargin
                                             + Theme.scrollBarAllowance

    // Measured against the page's own width, never the table's content width, so
    // the fit answer stays a one-way read. The floor keeps a genuinely narrow
    // host on the narrow layout even if the column set shrinks further someday.
    readonly property bool isNarrow: width < Math.max(Theme.breakpointPanelCollapse,
                                                      fixStationPage.wideMinimumWidth)
    readonly property FixStationModel fixStationsModel: cave ? cave.fixStations : null

    // How many tree levels the station column makes room for before the
    // indentation starts to eat into the name.
    readonly property int stationIndentLevels: 3

    // Cavern refuses a fix that names no system once the project has a frame,
    // so only then does a bare attached row with no system chosen warn.
    readonly property bool projectHasFrame: RootData.region.geoReference.hasCoordinateSystem

    // The node and its trips that carry an attachment, the node first, then
    // the trips in order. Each holds the system its file's bare fixes read in.
    readonly property list<QtObject> attachmentOwners: {
        const owners = []
        if (fixStationPage.cave === null) {
            return owners
        }
        if (!fixStationPage.cave.externalCenterline.isEmpty) {
            owners.push(fixStationPage.cave)
        }
        for (let i = 0; i < fixStationPage.cave.tripCount; ++i) {
            const trip = fixStationPage.cave.trip(i)
            if (!trip.externalCenterline.isEmpty) {
                owners.push(trip)
            }
        }
        return owners
    }

    // The row to select and scroll to, or -1 for none. A page selection property: a
    // link into the page (a node warning naming a fix) sets it through the
    // page, and a row the user picks writes it back, so the same link works
    // again after the user has moved on.
    property int currentFixRow: -1

    // The node rows the user closed, by node id. Every other node row stays
    // open, the ones that arrive later included.
    property list<string> collapsedNodeIds: []

    PageView.defaultSelectionProperties: ({ "currentFixRow": -1 })

    function addFix() {
        if (fixStationPage.fixStationsModel) {
            fixStationPage.fixStationsModel.addFixStation()
        }
    }

    // Chooses the system attachmentOwners[ownerIndex]'s file's fixes with no
    // system of their own read in; empty unsets it.
    function setBareFixCoordinateSystem(ownerIndex: int, cs: string): void {
        fixStationPage.setOwnerFixCoordinateSystem(fixStationPage.attachmentOwners[ownerIndex], cs)
    }

    // The same for \a owner, a node or a trip with an attachment.
    function setOwnerFixCoordinateSystem(owner: QtObject, cs: string): void {
        const centerline = owner.externalCenterline
        centerline.fixCoordinateSystem = cs
        owner.externalCenterline = centerline
    }

    // The node whose fix station table holds page row \a fixRow, null when
    // there is no such row.
    function nodeOfFixRow(fixRow: int): SurveyNode {
        const index = treeModelId.indexOfFixRow(fixRow)
        return index.valid ? treeModelId.data(index, FixStationTreeModel.NodeObjectRole) : null
    }

    // Page row \a fixRow's row in its node's fix station table, which is the
    // same row of the node's fixStations for one of the node's own fixes.
    function rowInNode(fixRow: int): int {
        const index = treeModelId.indexOfFixRow(fixRow)
        return index.valid ? index.row : -1
    }

    function removeFix(fixRow: int): void {
        const node = fixStationPage.nodeOfFixRow(fixRow)
        if (node !== null) {
            node.fixStations.removeAt(fixStationPage.rowInNode(fixRow))
        }
    }

    function commitEdit(fixRow: int, role: int, newText: string): void {
        const node = fixStationPage.nodeOfFixRow(fixRow)
        if (node !== null) {
            node.fixStations.setData(node.fixStations.index(fixStationPage.rowInNode(fixRow)),
                                     newText, role)
        }
    }

    // The whole coordinate in one write, so a pasted one re-solves the line plot
    // once. Text that won't parse never reaches here: the field's
    // CoordinateTextValidator holds the editor open and shows the reason, in the
    // row's own axis order.
    function commitCoordinate(fixRow: int, newText: string): void {
        const node = fixStationPage.nodeOfFixRow(fixRow)
        if (node !== null) {
            node.fixStations.setCoordinateText(fixStationPage.rowInNode(fixRow), newText,
                                               ProjectUnits.unitSystem)
        }
    }

    // Re-stores text the row already held, rather than text someone typed — so
    // it is read back in the units it was stored under, not the ones on display.
    // A stored elevation may be bare (the load path takes a hand-edited string
    // as-is), and cwFixStation reads a bare one as meters; committing it in an
    // imperial project would re-read the same digits as feet and move the fix
    // vertically, which is not something the user asked for by exchanging two
    // horizontal numbers.
    function commitStoredCoordinate(fixRow: int, newText: string): void {
        const node = fixStationPage.nodeOfFixRow(fixRow)
        if (node !== null) {
            node.fixStations.setCoordinateText(fixStationPage.rowInNode(fixRow), newText,
                                               Units.Metric)
        }
    }

    // Hands page row \a fixRow to the 3D view to be placed by clicking the
    // terrain, the way the Map page's Add Layer hands its selection to the view.
    function pickFromView(fixRow: int): void {
        FixStationPick.begin(fixStationPage.nodeOfFixRow(fixRow) as Cave,
                             fixStationPage.rowInNode(fixRow))
    }

    // Both coordinate-system cells commit through here so the transposition
    // question is asked once rather than per layout. `orderWasUnknown` and
    // `coordinateText` are the row as it stood *before* this write — the write
    // is what gives a system-less row an axis order, and afterwards nothing
    // remembers it never had one.
    function commitCS(rowIndex: int, newCS: string, orderWasUnknown: bool,
                      coordinateText: string): void {
        fixStationPage.commitEdit(rowIndex, FixStationModel.InputCSRole, newCS)
        coordinateOrderAskId.askAbout(rowIndex, coordinateText, newCS, orderWasUnknown)
    }

    // Only a row the user picks is written back to the page: the view's own
    // currentIndex changes (a row removed above the current one) would
    // race a link that is opening the page on another row.
    function pickRow(row: int): void {
        treeViewId.currentIndex = row
        if (fixStationPage.PageView.page !== null) {
            fixStationPage.PageView.page.selectionProperties = { "currentFixRow": row }
        }
    }

    // Opens the nodes above page row \a fixRow and scrolls it into view.
    function revealFixRow(fixRow: int): void {
        const index = treeModelId.indexOfFixRow(fixRow)
        if (!index.valid) {
            return
        }
        treeViewId.expandToIndex(index)
        treeViewId.forceLayout()
        treeViewId.positionViewAtIndex(index, QQ.TableView.Contain)
    }

    function isCollapsedNode(node: SurveyNode): bool {
        return node !== null && fixStationPage.collapsedNodeIds.indexOf(String(node.id)) >= 0
    }

    // Opens every node row the user has not closed. The page's own node is
    // the tree's root and always open. Walks the model rather than the view's
    // rows, which the view only recounts at its next layout.
    function applyExpansion(): void {
        const open = (index, isRoot) => {
            const row = treeViewId.rowAtIndex(index)
            if (row < 0) {
                return
            }
            const node = treeModelId.data(index, FixStationTreeModel.NodeObjectRole)
            if (!isRoot && fixStationPage.isCollapsedNode(node)) {
                return
            }
            if (!treeViewId.isExpanded(row)) {
                treeViewId.expand(row)
            }
            for (let i = 0; i < treeModelId.rowCount(index); ++i) {
                const child = treeModelId.index(i, 0, index)
                if (treeModelId.data(child, FixStationTreeModel.RowKindRole) === FixStationTreeModel.NodeRow) {
                    open(child, false)
                }
            }
        }
        if (treeModelId.rowCount() > 0) {
            open(treeModelId.index(0, 0), true)
        }
    }

    function toggleNode(row: int, node: SurveyNode): void {
        const id = String(node.id)
        const collapsed = fixStationPage.collapsedNodeIds.filter(collapsedId => collapsedId !== id)
        if (treeViewId.isExpanded(row)) {
            collapsed.push(id)
            treeViewId.collapse(row)
        } else {
            treeViewId.expand(row)
        }
        fixStationPage.collapsedNodeIds = collapsed
        fixStationPage.applyExpansion()
    }

    function expandAll(): void {
        fixStationPage.collapsedNodeIds = []
        fixStationPage.applyExpansion()
    }

    // Closes every node row under the page's own node.
    function collapseAll(): void {
        const ids = []
        const collect = (node) => {
            for (let i = 0; i < node.childNodeCount; ++i) {
                const child = node.childNode(i)
                ids.push(String(child.id))
                collect(child)
            }
        }
        if (fixStationPage.cave !== null) {
            collect(fixStationPage.cave)
        }
        fixStationPage.collapsedNodeIds = ids
        treeViewId.collapseRecursively()
        fixStationPage.applyExpansion()
    }

    onCurrentFixRowChanged: {
        treeViewId.currentIndex = fixStationPage.currentFixRow
        if (fixStationPage.currentFixRow < 0) {
            return
        }
        // The rows may not be laid out yet when the page opens on a link.
        Qt.callLater(() => fixStationPage.revealFixRow(treeViewId.currentIndex))
    }

    // currentFixRow mirrors the view, so a link naming the row the page last
    // showed still counts as a change once the view has moved on.
    QQ.Connections {
        target: treeViewId
        function onCurrentIndexChanged() { fixStationPage.currentFixRow = treeViewId.currentIndex }
    }

    // New node rows open as they arrive, so the whole subtree starts open.
    QQ.Connections {
        target: treeModelId
        function onRowsInserted() { Qt.callLater(fixStationPage.applyExpansion) }
        function onModelReset() { Qt.callLater(fixStationPage.applyExpansion) }
    }

    component FixField : DoubleClickTextInput {
        id: field
        property string value
        property int role
        property int rowIndex
        // Tints the value red when the row's coordinate falls outside the input
        // CS's valid domain (U4), or when the station name matches nothing.
        property bool error: false
        //! When set, the field carries the whole coordinate rather than one
        //! role: it commits through setCoordinateText() and is validated as a
        //! unit. See cwCoordinateText for the formats it takes.
        property bool coordinate: false
        //! Which axis the coordinate leads with, from this row's own CS. It
        //! reaches the validator because a refusal has to name the axes the row
        //! actually wants — the verdict is order-independent, the message isn't.
        property int axisOrder: CoordinateText.EastingNorthing
        //! The coordinate as it was written, when the row has one. Empty ⇒ the
        //! editor opens on what the cell displays, which is what every field
        //! other than the coordinate does.
        property string editValue: ""

        text: value
        //! A coordinate cell now shows the row's own text when that text can't
        //! be read (see the value bindings below), and such text is by
        //! definition unvalidated — it reached the project by hand. AutoText
        //! would render "<b>610016" as markup and show the user something other
        //! than what is stored.
        textFormat: QC.Label.PlainText
        //! Display and edit are deliberately different for a coordinate: the
        //! column renders every row in the project's units so it can be scanned,
        //! while an edit starts from what the user wrote.
        editText: field.editValue !== "" ? field.editValue : field.text
        color: field.error ? Theme.errorText
                           : field.readOnly ? Theme.textSubtle : Theme.text
        validator: field.coordinate ? coordinateValidatorId : null

        onFinishedEditting: (newText) => {
            if (field.coordinate) {
                fixStationPage.commitCoordinate(field.rowIndex, newText)
            } else {
                fixStationPage.commitEdit(field.rowIndex, field.role, newText)
            }
        }

        // One per field rather than one per page: the axis order it explains
        // itself in belongs to the row, and two rows can disagree.
        CoordinateTextValidator {
            id: coordinateValidatorId
            axisOrder: field.axisOrder
        }
    }

    component WideCell : QQ.Item {
        id: cell
        property int columnWidth: 0
        //! An item the cell parks at its right edge, declared as a child of the
        //! cell; the field gives up the room it takes. The coordinate column
        //! uses it so the "Coordinate" header owns the crosshair that fills it.
        property QQ.Item trailingItem: null
        //! The same at the left edge; the station column parks the read-only
        //! lock there.
        property QQ.Item leadingItem: null
        property alias value: field.value
        property alias role: field.role
        property alias rowIndex: field.rowIndex
        property alias error: field.error
        property alias coordinate: field.coordinate
        property alias axisOrder: field.axisOrder
        property alias editValue: field.editValue
        property alias readOnly: field.readOnly
        //! Names the inner editable field rather than this wrapper, so callers
        //! reach the same item the narrow layout exposes directly.
        property alias fieldObjectName: field.objectName

        implicitWidth: columnWidth
        implicitHeight: Math.max(field.implicitHeight,
                                 cell.trailingItem ? cell.trailingItem.implicitHeight : 0,
                                 cell.leadingItem ? cell.leadingItem.implicitHeight : 0)
        clip: true

        FixField {
            id: field
            anchors.left: cell.leadingItem ? cell.leadingItem.right : parent.left
            anchors.leftMargin: cell.leadingItem ? Theme.tightSpacing : 0
            anchors.right: cell.trailingItem ? cell.trailingItem.left : parent.right
            anchors.rightMargin: cell.trailingItem ? Theme.tightSpacing : 0
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // Inline per-row warning icon with a hover tooltip, driven by a message
    // string (empty ⇒ hidden and zero-width). One of these carries whatever is
    // wrong with the row's coordinate — it can't be read at all
    // (CoordinateErrorRole) or it falls outside its own CS (DomainErrorRole),
    // never both — and another carries the station reference (StationErrorRole).
    component InlineWarning : QQ.Item {
        id: inlineWarning
        property string message: ""

        visible: inlineWarning.message !== ""
        implicitWidth: visible ? Theme.iconSizeButton : 0
        implicitHeight: Theme.iconSizeButton

        QQ.Image {
            anchors.centerIn: parent
            source: "qrc:icons/svg/warning.svg"
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
        }

        QQ.HoverHandler {
            id: warningHover
        }

        QC.ToolTip {
            visible: warningHover.hovered && inlineWarning.message !== ""
            text: inlineWarning.message
            delay: 300

            // Every message this carries quotes something the user supplied — a
            // station name, the unit token the parser refused — and the default
            // content item reads its text as AutoText, so a name like A<b>B
            // becomes a tag. Same reason the coordinate cell declares PlainText.
            contentItem: QC.Label {
                text: inlineWarning.message
                textFormat: QC.Label.PlainText
            }
        }
    }

    // Marks a row an attached file fixes itself, with a tooltip naming the
    // file, which is the only place that fix can be changed.
    component ReadOnlyLock : QQ.Item {
        id: readOnlyLock
        property string sourceFile: ""

        implicitWidth: Theme.iconSizeButton
        implicitHeight: Theme.iconSizeButton

        Icon {
            anchors.centerIn: parent
            source: "qrc:/twbs-icons/icons/lock.svg"
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
            colorizationColor: Theme.textSubtle
        }

        QQ.HoverHandler {
            id: lockHover
        }

        QC.ToolTip {
            visible: lockHover.hovered
            delay: Theme.toolTipDelay

            // The file name is the user's; see InlineWarning.
            contentItem: QC.Label {
                text: "Fixed by " + readOnlyLock.sourceFile + ". Edit that file to change this fix."
                textFormat: QC.Label.PlainText
            }
        }
    }

    // Marks an attached fix the file declares a reference point.
    component ReferenceChip : QQ.Rectangle {
        id: referenceChip

        implicitWidth: referenceLabel.implicitWidth + 2 * Theme.chipPadding
        implicitHeight: referenceLabel.implicitHeight + 2 * Theme.tightSpacing
        radius: Theme.chipRadius
        color: Theme.chipBackground

        QC.ToolTip.visible: referenceHover.hovered
        QC.ToolTip.text: "The file marks this fix as a reference point; cavern keeps it even when no leg uses it."
        QC.ToolTip.delay: Theme.toolTipDelay

        QQ.HoverHandler {
            id: referenceHover
        }

        QC.Label {
            id: referenceLabel
            anchors.centerIn: parent
            text: "reference"
            color: Theme.chipText
            font.pixelSize: Theme.fontSizeCaption
        }
    }

    // A read-only row's coordinate system, as the file wrote it, or "none".
    component ReadOnlyCSLabel : QC.Label {
        id: readOnlyCSLabel
        property string value: ""

        text: readOnlyCSLabel.value !== "" ? readOnlyCSLabel.value : "none"
        textFormat: QC.Label.PlainText
        elide: QC.Label.ElideRight
        color: Theme.textSubtle
    }

    // An attached fix its file names no system for: the system the solve reads
    // it in, or "unset" while cavern refuses it.
    component BareFixBadge : QC.Label {
        id: bareFixBadge
        property string readInSystem: ""
        //! The row's read-in note, which says where the system came from.
        property string readInWarning: ""
        property string sourceFile: ""
        readonly property bool unset: bareFixBadge.readInSystem === ""

        text: "none \u2192 " + (bareFixBadge.unset ? "unset" : bareFixBadge.readInSystem)
        textFormat: QC.Label.PlainText
        elide: QC.Label.ElideRight
        color: bareFixBadge.unset ? Theme.errorText : Theme.warningText
        font.pixelSize: Theme.fontSizeSmall
        leftPadding: Theme.chipPadding
        rightPadding: Theme.chipPadding

        background: QQ.Rectangle {
            radius: Theme.chipRadius
            color: Theme.warningSurface
            border.color: Theme.warningBorder
        }

        QQ.HoverHandler {
            id: bareFixHover
        }

        QC.ToolTip {
            visible: bareFixHover.hovered
            delay: Theme.toolTipDelay

            // The file name is the user's; see InlineWarning.
            contentItem: QC.Label {
                text: bareFixBadge.unset
                      ? "Cavern rejects this fix until a coordinate system is chosen for "
                        + bareFixBadge.sourceFile + "."
                      : bareFixBadge.readInWarning
                textFormat: QC.Label.PlainText
            }
        }
    }

    // A read-only row's system: the badge when the file names none and either
    // a system reads it or cavern refuses it, else ReadOnlyCSLabel.
    component ReadOnlyCSCell : QQ.Item {
        id: readOnlyCSCell
        property string value: ""
        property int rowIndex
        property string readInSystem: ""
        property string readInWarning: ""
        property string sourceFile: ""
        readonly property bool showsBadge: readOnlyCSCell.value === ""
                                           && (readOnlyCSCell.readInSystem !== ""
                                               || fixStationPage.projectHasFrame)

        implicitWidth: readOnlyCSCell.showsBadge ? badgeLoader.implicitWidth
                                                 : readOnlyLabel.implicitWidth
        implicitHeight: readOnlyCSCell.showsBadge ? badgeLoader.implicitHeight
                                                  : readOnlyLabel.implicitHeight

        ReadOnlyCSLabel {
            id: readOnlyLabel
            objectName: "inputCSText." + readOnlyCSCell.rowIndex
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: !readOnlyCSCell.showsBadge
            value: readOnlyCSCell.value
        }

        QQ.Loader {
            id: badgeLoader
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, parent.width)
            active: readOnlyCSCell.showsBadge
            sourceComponent: BareFixBadge {
                objectName: "noCoordinateSystemBadge." + readOnlyCSCell.rowIndex
                readInSystem: readOnlyCSCell.readInSystem
                readInWarning: readOnlyCSCell.readInWarning
                sourceFile: readOnlyCSCell.sourceFile
            }
        }
    }

    component CSCell : QQ.Item {
        id: csCell
        property int columnWidth: 0
        property string value
        property int rowIndex
        //! The row's own coordinate and whether anything records which axis it
        //! leads with — see fixStationPage.commitCS().
        property string coordinateText: ""
        property bool orderUnknown: false
        //! What the row's datum combo may offer, and whether it may be used at
        //! all — see CSPicker.availableDatums / datumEnabled.
        property list<string> availableDatums: []
        property bool datumEnabled: false
        //! A row an attached file fixes: the system is shown as text, with no
        //! combo to change it.
        property bool readOnly: false
        //! See ReadOnlyCSCell.
        property string readInSystem: ""
        property string readInWarning: ""
        property string sourceFile: ""

        implicitWidth: columnWidth
        implicitHeight: csLoader.implicitHeight
        clip: true

        QQ.Loader {
            id: csLoader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            sourceComponent: csCell.readOnly ? readOnlyCSComponent : comboComponent
        }

        QQ.Component {
            id: comboComponent

            CSComboBox {
                objectName: "inputCSComboBox." + csCell.rowIndex
                value: csCell.value
                availableDatums: csCell.availableDatums
                datumEnabled: csCell.datumEnabled
                onCommitted: (newCS) => fixStationPage.commitCS(
                    csCell.rowIndex, newCS, csCell.orderUnknown, csCell.coordinateText)
            }
        }

        QQ.Component {
            id: readOnlyCSComponent

            ReadOnlyCSCell {
                value: csCell.value
                rowIndex: csCell.rowIndex
                readInSystem: csCell.readInSystem
                readInWarning: csCell.readInWarning
                sourceFile: csCell.sourceFile
            }
        }
    }

    // Chooses the system an attachment's fixes with no system of their own read
    // in, on the owner's node row. Shown while the owner's files hold any such
    // fix.
    component OwnerSystemControl : RowLayout {
        id: ownerSystemControl

        required property QtObject owner
        required property bool isNodeOwner
        property string value: ""

        signal committed(string newCS)

        spacing: Theme.tightSpacing

        QC.Label {
            text: ownerSystemControl.isNodeOwner
                  ? "Read fixes with no coordinate system as"
                  : "Read " + (ownerSystemControl.owner?.name ?? "")
                    + "'s fixes with no coordinate system as"
            textFormat: QC.Label.PlainText
            font.pixelSize: Theme.fontSizeSmall
        }

        CSComboBox {
            objectName: "bareFixSystemCombo." + (ownerSystemControl.owner?.name ?? "")
            offersUnset: true
            value: ownerSystemControl.value
            onCommitted: (newCS) => ownerSystemControl.committed(newCS)
        }
    }

    // The owner controls of a node's trips that carry attachments, each shown
    // while its file has a fix with no system of its own.
    component TripSystemControls : ColumnLayout {
        id: tripSystemControls

        required property SurveyNode node

        // Recounted from the node's attached rows, which list every trip's
        // fixes.
        property int revision: 0

        spacing: Theme.tightSpacing

        QQ.Connections {
            target: tripSystemControls.node?.attachedFixes ?? null

            function onModelReset() { tripSystemControls.revision++ }
            function onDataChanged() { tripSystemControls.revision++ }
        }

        QQ.Repeater {
            model: tripSystemControls.node?.tripCount ?? 0

            delegate: OwnerSystemControl {
                id: tripControl

                required property int index

                readonly property Trip trip: tripSystemControls.node?.trip(tripControl.index) ?? null

                owner: tripControl.trip
                isNodeOwner: false
                value: tripControl.trip?.externalCenterline.fixCoordinateSystem ?? ""
                visible: tripSystemControls.revision >= 0
                         && tripControl.trip !== null
                         && !tripControl.trip.externalCenterline.isEmpty
                         && tripSystemControls.node.attachedFixes.bareFixCount(tripControl.trip.id) > 0

                onCommitted: (newCS) => fixStationPage.setOwnerFixCoordinateSystem(tripControl.trip, newCS)
            }
        }
    }

    // A small rounded label on a node row: its kind, its fix count.
    component Chip : QC.Label {
        textFormat: QC.Label.PlainText
        color: Theme.chipText
        font.pixelSize: Theme.fontSizeCaption
        leftPadding: Theme.chipPadding
        rightPadding: Theme.chipPadding

        background: QQ.Rectangle {
            radius: Theme.chipRadius
            color: Theme.chipBackground
        }
    }

    // The rows are a tree over the node and its subtree: a row per node, and
    // under each its fix station table's rows (its own rows, then the
    // read-only rows its files fix themselves) ahead of its child nodes.
    //
    // A fix row is addressed by its page row, its place among every fix row
    // in depth-first order. The node's own rows come first, so a page row
    // below fixStationsModel.count is the same row in fixStationsModel;
    // currentFixRow, the cell objectNames, and the commit functions below
    // all take page rows.
    FixStationTreeModel {
        id: treeModelId
        rootNode: fixStationPage.cave
    }

    RemoveAskBox {
        id: removeChallengeId
        onRemove: fixStationPage.removeFix(indexToRemove)
    }

    // Page-level, not per row: a table delegate is destroyed as its row scrolls
    // out of view, which would take the open question down with it. The row it
    // is asking about therefore travels with the question.
    CoordinateOrderAskBox {
        id: coordinateOrderAskId

        onSwapRequested: (row, swappedCoordinate) => fixStationPage.commitStoredCoordinate(
            row, swappedCoordinate)
    }

    // The coordinate column carries the crosshair, so it has to know how much
    // room the crosshair takes. A tool button's footprint is its icon plus
    // whatever padding the style puts around it, so the column asks a button
    // rather than guessing. Invisible: it exists to be measured.
    PickFromViewButton {
        id: pickButtonMetricsId
        visible: false
    }

    TableStaticColumnModel {
        id: columnModelId
        columns: [
            TableStaticColumn {
                id: stationColumn
                // A name's width plus the indentation of the deepest tree
                // level the column makes room for.
                columnWidth: 130 + fixStationPage.stationIndentLevels * Theme.treeIndent
                text: "Station"
            },
            TableStaticColumn {
                id: csColumn
                // Wide enough for UTM mode's four controls — mode combo (sized
                // to its widest entry, "Custom...") + zone + hemisphere + datum
                // — so none of them is clipped by the cell.
                columnWidth: 400
                text: "Input CS"
            },
            TableStaticColumn {
                id: coordinateColumn
                // Wide enough for a UTM triple with its elevation unit, e.g.
                // "610016.792, 5615117.075, 2545.34m", plus the footprint of
                // the pick-from-view crosshair that shares the column with it.
                columnWidth: 300 + pickButtonMetricsId.implicitWidth + Theme.tightSpacing
                // Both orders are named because the order is per row, not per
                // column: a geographic CS writes latitude first, a projected one
                // easting first. The headers this column replaced ("Easting /
                // Long", "Northing / Lat") were the only place that was said.
                text: "Coordinate (East, North / Lat, Long)"
            }
        ]
    }

    AddAndSearchBar {
        id: addFixBar
        objectName: "addFixBar"
        addButtonText: "Add Fix"
        onAdd: fixStationPage.addFix()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: Theme.sectionSpacing

        LayoutItemProxy {
            target: addFixBar
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Theme.flowSpacing
            Layout.fillWidth: true

            QC.Button {
                objectName: "expandAllButton"
                text: "Expand all"
                flat: true
                onClicked: fixStationPage.expandAll()
            }

            QC.Button {
                objectName: "collapseAllButton"
                text: "Collapse all"
                flat: true
                onClicked: fixStationPage.collapseAll()
            }

            QQ.Item {
                Layout.fillWidth: true
            }
        }

        QQ.Row {
            visible: !fixStationPage.isNarrow
            Layout.fillWidth: true

            QQ.Repeater {
                model: columnModelId

                delegate: TableStaticHeaderColumn {
                    model: treeModelId
                }
            }
        }

        QQ.TreeView {
            id: treeViewId
            objectName: "fixStationTreeView"

            //! The page row of the picked fix, -1 for none.
            property int currentIndex: -1
            //! How many fix rows the tree holds, every node's included.
            readonly property int count: treeModelId.fixRowCount

            model: treeModelId
            // A pooled row keeps the names its cells last carried, which would
            // answer for a row that is gone.
            reuseItems: false
            clip: true
            boundsBehavior: QQ.Flickable.StopAtBounds
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitWidth: fixStationPage.isNarrow ? 0 : columnModelId.totalWidth

            delegate: fixStationPage.isNarrow ? narrowChooserId : wideChooserId

            columnWidthProvider: function(column) {
                return fixStationPage.isNarrow
                        ? treeViewId.width
                        : Math.max(treeViewId.width, columnModelId.totalWidth)
            }

            onWidthChanged: treeViewId.forceLayout()

            QC.ScrollBar.vertical: QC.ScrollBar {}

            QQ.Component.onCompleted: Qt.callLater(fixStationPage.applyExpansion)
        }
    }

    HelpBox {
        objectName: "noFixStationsHelpBox"
        anchors.centerIn: parent
        text: "No fix stations yet. Click <b>Add Fix</b> to anchor a station to absolute coordinates."
        visible: fixStationPage.cave !== null && treeModelId.fixRowCount === 0
    }

    DelegateChooser {
        id: wideChooserId
        role: "rowKind"

        DelegateChoice {
            roleValue: FixStationTreeModel.NodeRow
            delegate: nodeRowComponent
        }

        DelegateChoice {
            roleValue: FixStationTreeModel.FixRow
            delegate: wideDelegateComponent
        }
    }

    DelegateChooser {
        id: narrowChooserId
        role: "rowKind"

        DelegateChoice {
            roleValue: FixStationTreeModel.NodeRow
            delegate: nodeRowComponent
        }

        DelegateChoice {
            roleValue: FixStationTreeModel.FixRow
            delegate: narrowDelegateComponent
        }
    }

    // A node's row: chevron, kind, name, its fix counts, and the system its
    // files' fixes with no system of their own read in, chosen on the
    // attachment's owner and shown on the nodes under it.
    QQ.Component {
        id: nodeRowComponent

        QQ.Item {
            id: nodeRowId
            objectName: "nodeRow." + nodeRowId.nodeName

            required property QQ.TreeView treeView
            required property bool expanded
            required property bool hasChildren
            required property int depth
            required property int row
            required property string nodeName
            required property string nodeKindLabel
            required property SurveyNode node
            required property int ownFixCount
            required property int subtreeFixCount
            required property int subtreeBareFixCount
            required property int subtreeUnsetFixCount
            required property int attachmentBareFixCount
            required property bool isAttachmentOwner
            required property string fixCoordinateSystem
            required property string inheritedFixCoordinateSystem
            required property SurveyNode inheritedFromNode

            readonly property bool isPageNode: nodeRowId.node === fixStationPage.cave
            // The region's root node has no name of its own.
            readonly property string displayName: nodeRowId.nodeName !== "" ? nodeRowId.nodeName : "Project"
            // The system the node's bare fixes read in: its own choice on an
            // owner, the owner's above it otherwise.
            readonly property string bareFixSystem: nodeRowId.isAttachmentOwner
                                                    ? nodeRowId.fixCoordinateSystem
                                                    : nodeRowId.inheritedFixCoordinateSystem
            readonly property bool showsInheritedSystem: !nodeRowId.isAttachmentOwner
                                                         && nodeRowId.inheritedFromNode !== null
                                                         && nodeRowId.subtreeBareFixCount > 0

            implicitWidth: nodeColumnId.implicitWidth + nodeColumnId.anchors.leftMargin + Theme.delegatePadding
            implicitHeight: nodeColumnId.implicitHeight + 2 * Theme.delegatePadding

            QQ.Rectangle {
                anchors.fill: parent
                color: Theme.surfaceMuted
            }

            ColumnLayout {
                id: nodeColumnId
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.delegatePadding + nodeRowId.depth * Theme.treeIndent
                anchors.rightMargin: Theme.delegatePadding
                spacing: Theme.tightSpacing

                RowLayout {
                    spacing: Theme.flowSpacing
                    Layout.fillWidth: true

                    // The page's own node is the tree's root and stays open.
                    QQ.Item {
                        objectName: "nodeChevron." + nodeRowId.nodeName
                        visible: !nodeRowId.isPageNode
                        Layout.preferredWidth: Theme.treeCaretWidth
                        Layout.preferredHeight: Theme.treeCaretWidth

                        QC.Label {
                            anchors.centerIn: parent
                            visible: nodeRowId.hasChildren
                            text: nodeRowId.expanded ? "\u25BE" : "\u25B8"
                            color: Theme.textSubtle
                        }

                        QQ.TapHandler {
                            enabled: nodeRowId.hasChildren
                            onTapped: fixStationPage.toggleNode(nodeRowId.row, nodeRowId.node)
                        }
                    }

                    Chip {
                        visible: nodeRowId.node !== null && nodeRowId.node.parentNode !== null
                        text: nodeRowId.nodeKindLabel
                    }

                    QC.Label {
                        text: nodeRowId.displayName
                        textFormat: QC.Label.PlainText
                        font.bold: true
                    }

                    Chip {
                        text: (nodeRowId.ownFixCount === 1 ? "1 fix" : nodeRowId.ownFixCount + " fixes")
                              + (nodeRowId.subtreeFixCount !== nodeRowId.ownFixCount
                                 ? " \u00B7 " + nodeRowId.subtreeFixCount + " with children"
                                 : "")
                    }

                    QC.Label {
                        visible: nodeRowId.ownFixCount === 0
                        text: "No fixed stations"
                        color: Theme.textSubtle
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    // Cavern refuses these until a system is chosen for them.
                    QC.Label {
                        visible: fixStationPage.projectHasFrame
                                 && nodeRowId.subtreeUnsetFixCount > 0
                        text: "\u26A0 " + nodeRowId.subtreeUnsetFixCount + " without coordinate system"
                        color: Theme.errorText
                        font.pixelSize: Theme.fontSizeSmall
                        leftPadding: Theme.chipPadding
                        rightPadding: Theme.chipPadding

                        background: QQ.Rectangle {
                            radius: Theme.chipRadius
                            color: Theme.warningSurface
                            border.color: Theme.warningBorder
                        }
                    }

                    QQ.Item {
                        Layout.fillWidth: true
                    }

                    LinkText {
                        visible: !nodeRowId.isPageNode
                        text: "Open page \u2192"
                        onClicked: RootData.pageSelectionModel.currentPageAddress =
                                   linkGeneratorId.fixStationsLink(nodeRowId.node)
                    }
                }

                OwnerSystemControl {
                    visible: nodeRowId.isAttachmentOwner && nodeRowId.attachmentBareFixCount > 0
                    owner: nodeRowId.node
                    isNodeOwner: true
                    value: nodeRowId.fixCoordinateSystem
                    Layout.leftMargin: Theme.treeCaretWidth

                    onCommitted: (newCS) => treeModelId.setData(nodeRowId.treeView.index(nodeRowId.row, 0),
                                                                newCS,
                                                                FixStationTreeModel.FixCoordinateSystemRole)
                }

                TripSystemControls {
                    node: nodeRowId.node
                    Layout.leftMargin: Theme.treeCaretWidth
                }

                RowLayout {
                    visible: nodeRowId.showsInheritedSystem
                    spacing: Theme.tightSpacing
                    Layout.leftMargin: Theme.treeCaretWidth

                    QC.Label {
                        objectName: "inheritedSystem." + nodeRowId.nodeName
                        text: "Coordinate system for bare fixes: "
                              + (nodeRowId.bareFixSystem !== "" ? nodeRowId.bareFixSystem : "unset")
                              + " (set on " + (nodeRowId.inheritedFromNode?.name ?? "") + ")"
                        textFormat: QC.Label.PlainText
                        color: nodeRowId.bareFixSystem !== "" ? Theme.textSubtle : Theme.errorText
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    LinkText {
                        text: "\u2191"
                        onClicked: RootData.pageSelectionModel.currentPageAddress =
                                   linkGeneratorId.fixStationsLink(nodeRowId.inheritedFromNode)
                    }
                }
            }
        }
    }

    LinkGenerator {
        id: linkGeneratorId
        pageSelectionModel: RootData.pageSelectionModel
    }

    QQ.Component {
        id: wideDelegateComponent

        QQ.Item {
            id: wideDelegateId
            objectName: "fixRow." + wideDelegateId.stationName

            //! The row's page row, which names its cells and every commit.
            required property int fixRow
            required property int depth
            required property string stationName
            required property string inputCS
            required property double easting
            required property double northing
            required property double elevation
            required property string coordinateText
            required property string domainError
            required property bool eastingDomainError
            required property bool northingDomainError
            required property string coordinateError
            required property bool coordinateOrderUnknown
            required property string stationError
            required property list<string> availableDatums
            required property bool datumEnabled
            //! Set on a row an attached file fixes itself, named by sourceFile.
            required property bool readOnly
            required property string sourceFile
            //! On an attached row whose fix the file gives no system: what the
            //! solve reads it in, and the line that left it without one.
            required property string readInWarning
            //! Set on an attached row the file marks as a reference point.
            required property bool reference
            //! On an attached row whose fix the file gives no system: the system
            //! the solve reads it in, empty while none is chosen.
            required property string readInSystem

            readonly property int indent: Math.min(wideDelegateId.depth,
                                                   fixStationPage.stationIndentLevels) * Theme.treeIndent

            // The two coordinate complaints can't both speak: the domain check
            // judges a coordinate the row has, and this one says there isn't one
            // to judge. So they share the single warning beside the row rather
            // than competing for space that only ever holds one of them. The
            // read-in note sits on an attached row, which has neither.
            readonly property string coordinateWarning: wideDelegateId.coordinateError !== ""
                                                        ? wideDelegateId.coordinateError
                                                        : wideDelegateId.domainError !== ""
                                                          ? wideDelegateId.domainError
                                                          : wideDelegateId.readInWarning

            implicitHeight: rowLayoutId.implicitHeight + Theme.tightSpacing * 2
            implicitWidth: rowLayoutId.implicitWidth

            TableRowBackground {
                isSelected: treeViewId.currentIndex === wideDelegateId.fixRow
                rowIndex: wideDelegateId.fixRow
                anchors.fill: parent
            }

            // Only the node's own rows can be removed here.
            QQ.Loader {
                anchors.fill: parent
                active: !wideDelegateId.readOnly
                sourceComponent: DataRightClickMouseMenu {
                    objectName: "fixRowMenu." + wideDelegateId.fixRow
                    removeChallenge: removeChallengeId
                    row: wideDelegateId.fixRow
                    name: wideDelegateId.stationName
                }
            }

            QQ.MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: fixStationPage.pickRow(wideDelegateId.fixRow)
            }

            RowLayout {
                id: rowLayoutId
                spacing: 0
                anchors.verticalCenter: parent.verticalCenter

                // Only the station cell takes the tree's indentation, so the
                // other columns line up at every depth.
                QQ.Item {
                    Layout.preferredWidth: wideDelegateId.indent
                    Layout.preferredHeight: 1
                }

                WideCell {
                    fieldObjectName: "stationCell." + wideDelegateId.fixRow
                    columnWidth: stationColumn.columnWidth - wideDelegateId.indent
                    leadingItem: wideDelegateId.readOnly ? wideLockLoader : null
                    value: wideDelegateId.stationName
                    role: FixStationModel.StationNameRole
                    rowIndex: wideDelegateId.fixRow
                    readOnly: wideDelegateId.readOnly
                    error: wideDelegateId.stationError !== ""

                    QQ.Loader {
                        id: wideLockLoader
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        active: wideDelegateId.readOnly
                        sourceComponent: ReadOnlyLock {
                            objectName: "readOnlyLock." + wideDelegateId.fixRow
                            sourceFile: wideDelegateId.sourceFile
                        }
                    }
                }

                CSCell {
                    columnWidth: csColumn.columnWidth
                    value: wideDelegateId.inputCS
                    rowIndex: wideDelegateId.fixRow
                    coordinateText: wideDelegateId.coordinateText
                    orderUnknown: wideDelegateId.coordinateOrderUnknown
                    availableDatums: wideDelegateId.availableDatums
                    datumEnabled: wideDelegateId.datumEnabled
                    readOnly: wideDelegateId.readOnly
                    readInSystem: wideDelegateId.readInSystem
                    readInWarning: wideDelegateId.readInWarning
                    sourceFile: wideDelegateId.sourceFile
                }

                WideCell {
                    fieldObjectName: "coordinateCell." + wideDelegateId.fixRow
                    columnWidth: coordinateColumn.columnWidth
                    coordinate: true
                    readOnly: wideDelegateId.readOnly
                    trailingItem: wideDelegateId.readOnly ? null : pickButtonLoader
                    // Normally the cell renders the numbers, so it needs the
                    // same axis order they were read under. A CS flip swaps the
                    // numbers and the render order together, so what this
                    // displays stays put — it is the meaning underneath that
                    // moved.
                    //
                    // A row with a coordinate error has no numbers: it reports
                    // three zeros, and rendering those would hide the very text
                    // that is wrong with it behind a coordinate at the origin.
                    // So it shows what was written, which is all such a row has.
                    // So does a read-only row: the scan keeps the file's text,
                    // not the numbers.
                    value: wideDelegateId.coordinateError !== "" || wideDelegateId.readOnly
                           ? wideDelegateId.coordinateText
                           : CoordinateText.format(wideDelegateId.easting,
                                                   wideDelegateId.northing,
                                                   wideDelegateId.elevation,
                                                   ProjectUnits.unitSystem,
                                                   CoordinateText.axisOrderFor(wideDelegateId.inputCS))
                    // The cell above renders the numbers in the project's
                    // units; the editor re-offers the coordinate as written.
                    editValue: wideDelegateId.coordinateText
                    rowIndex: wideDelegateId.fixRow
                    axisOrder: CoordinateText.axisOrderFor(wideDelegateId.inputCS)
                    // One field now holds both horizontal components, so the
                    // two per-axis domain flags share the one tint — and so does
                    // a coordinate that couldn't be read at all.
                    error: wideDelegateId.coordinateError !== ""
                           || wideDelegateId.eastingDomainError
                           || wideDelegateId.northingDomainError

                    QQ.Loader {
                        id: pickButtonLoader
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        active: !wideDelegateId.readOnly
                        sourceComponent: PickFromViewButton {
                            objectName: "pickFromViewButton." + wideDelegateId.fixRow
                            onClicked: fixStationPage.pickFromView(wideDelegateId.fixRow)
                        }
                    }
                }

                QQ.Loader {
                    Layout.leftMargin: Theme.tightSpacing
                    Layout.alignment: Qt.AlignVCenter
                    active: wideDelegateId.reference
                    visible: wideDelegateId.reference
                    sourceComponent: ReferenceChip {
                        objectName: "referenceChip." + wideDelegateId.fixRow
                    }
                }

                InlineWarning {
                    objectName: "stationWarning." + wideDelegateId.fixRow
                    Layout.leftMargin: Theme.tightSpacing
                    Layout.alignment: Qt.AlignVCenter
                    message: wideDelegateId.stationError
                }

                InlineWarning {
                    objectName: "coordinateWarning." + wideDelegateId.fixRow
                    Layout.leftMargin: Theme.tightSpacing
                    Layout.alignment: Qt.AlignVCenter
                    message: wideDelegateId.coordinateWarning
                }
            }
        }
    }

    QQ.Component {
        id: narrowDelegateComponent

        QQ.Item {
            id: narrowDelegateId
            objectName: "fixRow." + narrowDelegateId.stationName

            //! The row's page row, which names its cells and every commit.
            required property int fixRow
            required property int depth
            required property string stationName
            required property string inputCS
            required property double easting
            required property double northing
            required property double elevation
            required property string coordinateText
            required property string domainError
            required property bool eastingDomainError
            required property bool northingDomainError
            required property string coordinateError
            required property bool coordinateOrderUnknown
            required property string stationError
            required property list<string> availableDatums
            required property bool datumEnabled
            //! Set on a row an attached file fixes itself, named by sourceFile.
            required property bool readOnly
            required property string sourceFile
            //! See the wide delegate.
            required property string readInWarning
            required property bool reference
            required property string readInSystem

            //! Mutually exclusive with the domain error — see the wide delegate.
            readonly property string coordinateWarning: narrowDelegateId.coordinateError !== ""
                                                        ? narrowDelegateId.coordinateError
                                                        : narrowDelegateId.domainError !== ""
                                                          ? narrowDelegateId.domainError
                                                          : narrowDelegateId.readInWarning

            implicitHeight: narrowFlow.implicitHeight + Theme.delegatePadding * 2

            TableRowBackground {
                isSelected: treeViewId.currentIndex === narrowDelegateId.fixRow
                rowIndex: narrowDelegateId.fixRow
                anchors.fill: parent
            }

            QQ.Loader {
                anchors.fill: parent
                active: !narrowDelegateId.readOnly
                sourceComponent: DataRightClickMouseMenu {
                    objectName: "fixRowMenu." + narrowDelegateId.fixRow
                    removeChallenge: removeChallengeId
                    row: narrowDelegateId.fixRow
                    name: narrowDelegateId.stationName
                }
            }

            QQ.MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: fixStationPage.pickRow(narrowDelegateId.fixRow)
            }

            QQ.Flow {
                id: narrowFlow
                objectName: "narrowFixRow." + narrowDelegateId.fixRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.delegatePadding + narrowDelegateId.depth * Theme.treeIndent
                anchors.rightMargin: Theme.delegatePadding
                spacing: Theme.flowSpacing

                InlineWarning {
                    objectName: "stationWarning." + narrowDelegateId.fixRow
                    message: narrowDelegateId.stationError
                }

                InlineWarning {
                    objectName: "coordinateWarning." + narrowDelegateId.fixRow
                    message: narrowDelegateId.coordinateWarning
                }

                QQ.Loader {
                    active: narrowDelegateId.readOnly
                    visible: narrowDelegateId.readOnly
                    sourceComponent: ReadOnlyLock {
                        objectName: "readOnlyLock." + narrowDelegateId.fixRow
                        sourceFile: narrowDelegateId.sourceFile
                    }
                }

                FixField {
                    objectName: "stationCell." + narrowDelegateId.fixRow
                    value: narrowDelegateId.stationName
                    role: FixStationModel.StationNameRole
                    rowIndex: narrowDelegateId.fixRow
                    readOnly: narrowDelegateId.readOnly
                    font.bold: true
                    error: narrowDelegateId.stationError !== ""
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                QQ.Loader {
                    sourceComponent: narrowDelegateId.readOnly ? narrowReadOnlyCSComponent
                                                               : narrowComboComponent
                }

                QQ.Component {
                    id: narrowComboComponent

                    CSComboBox {
                        objectName: "inputCSComboBox." + narrowDelegateId.fixRow
                        value: narrowDelegateId.inputCS
                        availableDatums: narrowDelegateId.availableDatums
                        datumEnabled: narrowDelegateId.datumEnabled
                        onCommitted: (newCS) => fixStationPage.commitCS(
                            narrowDelegateId.fixRow, newCS,
                            narrowDelegateId.coordinateOrderUnknown,
                            narrowDelegateId.coordinateText)
                    }
                }

                QQ.Component {
                    id: narrowReadOnlyCSComponent

                    ReadOnlyCSCell {
                        value: narrowDelegateId.inputCS
                        rowIndex: narrowDelegateId.fixRow
                        readInSystem: narrowDelegateId.readInSystem
                        readInWarning: narrowDelegateId.readInWarning
                        sourceFile: narrowDelegateId.sourceFile
                    }
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                FixField {
                    objectName: "coordinateCell." + narrowDelegateId.fixRow
                    //! The text itself when there are no numbers to render —
                    //! see the wide delegate.
                    value: narrowDelegateId.coordinateError !== "" || narrowDelegateId.readOnly
                           ? narrowDelegateId.coordinateText
                           : CoordinateText.format(narrowDelegateId.easting,
                                                   narrowDelegateId.northing,
                                                   narrowDelegateId.elevation,
                                                   ProjectUnits.unitSystem,
                                                   CoordinateText.axisOrderFor(narrowDelegateId.inputCS))
                    editValue: narrowDelegateId.coordinateText
                    rowIndex: narrowDelegateId.fixRow
                    coordinate: true
                    readOnly: narrowDelegateId.readOnly
                    axisOrder: CoordinateText.axisOrderFor(narrowDelegateId.inputCS)
                    error: narrowDelegateId.coordinateError !== ""
                           || narrowDelegateId.eastingDomainError
                           || narrowDelegateId.northingDomainError
                }

                QQ.Loader {
                    active: narrowDelegateId.reference
                    visible: narrowDelegateId.reference
                    sourceComponent: ReferenceChip {
                        objectName: "referenceChip." + narrowDelegateId.fixRow
                    }
                }

                QQ.Loader {
                    active: !narrowDelegateId.readOnly
                    visible: !narrowDelegateId.readOnly
                    sourceComponent: PickFromViewButton {
                        objectName: "pickFromViewButton." + narrowDelegateId.fixRow
                        onClicked: fixStationPage.pickFromView(narrowDelegateId.fixRow)
                    }
                }

                QC.Label {
                    objectName: "readOnlySource." + narrowDelegateId.fixRow
                    visible: narrowDelegateId.readOnly
                    text: "from " + narrowDelegateId.sourceFile
                    textFormat: QC.Label.PlainText
                    color: Theme.textSubtle
                }
            }
        }
    }
}
