/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The Add verbs for one place in the survey tree. The place picks them: where
// caves go (the region's top level, or a Folder outside every cave) the menu
// offers Add Cave and Add Folder; inside a cave it offers Add Trip and Add
// Section, a Section being a Folder named for where it sits. A cave is
// therefore never offered inside a cave.
//
// Every place also offers Add Survey File…, set apart below the native verbs,
// which attaches the file's subtree under the menu's node.
//
// The menu adds the node or trip itself and hands it to added(), so each host
// decides only where the user lands afterwards. Attach needs a dialog first, so
// the menu hands that verb to attachSurveyFileRequested() and the host opens
// its dialog. A host appends its own entries below the verbs.
QC.Menu {
    id: addVerbsMenuId

    enum Verb {
        AddCave,
        AddFolder,
        AddTrip,
        AddSection,
        AttachSurveyFile
    }

    //The node the verbs add under, null for the region's top level.
    property SurveyNode node: null

    readonly property bool takesCaves: addVerbsMenuId.node === null
                                       || addVerbsMenuId.node.takesCaves

    //Verbs the host's own button already runs, which the menu leaves out.
    property list<int> hiddenVerbs: []

    //The verbs that make native nodes and trips; Add Survey File… follows them.
    readonly property list<int> verbs: (addVerbsMenuId.takesCaves
                                        ? [AddVerbsMenu.AddCave, AddVerbsMenu.AddFolder]
                                        : [AddVerbsMenu.AddTrip, AddVerbsMenu.AddSection])
                                       .filter(verb => !addVerbsMenuId.hiddenVerbs.includes(verb))

    //\a verb ran: \a object is the new cwSurveyNode or cwTrip.
    signal added(object: QQ.QtObject, verb: int)

    //Add Survey File… ran: the host opens its dialog with \a node as the
    //target, the region's root for the top level.
    signal attachSurveyFileRequested(node: SurveyNode)

    function verbText(verb: int) : string {
        switch(verb) {
        case AddVerbsMenu.AddCave:
            return qsTr("Add Cave");
        case AddVerbsMenu.AddFolder:
            return qsTr("Add Folder");
        case AddVerbsMenu.AddTrip:
            return qsTr("Add Trip");
        case AddVerbsMenu.AddSection:
            return qsTr("Add Section");
        case AddVerbsMenu.AttachSurveyFile:
            return qsTr("Add Survey File…");
        default:
            return "";
        }
    }

    function verbObjectName(verb: int) : string {
        switch(verb) {
        case AddVerbsMenu.AddCave:
            return "addCaveMenuItem";
        case AddVerbsMenu.AddFolder:
            return "addFolderMenuItem";
        case AddVerbsMenu.AddTrip:
            return "addTripMenuItem";
        case AddVerbsMenu.AddSection:
            return "addSectionMenuItem";
        case AddVerbsMenu.AttachSurveyFile:
            return "attachSurveyFileMenuItem";
        default:
            return "";
        }
    }

    //The glyph of what \a verb makes.
    function verbIcon(verb: int) : url {
        switch(verb) {
        case AddVerbsMenu.AddCave:
            return Theme.kindIconCave;
        case AddVerbsMenu.AddFolder:
            return Theme.kindIconFolder;
        case AddVerbsMenu.AddSection:
            return Theme.kindIconSection;
        case AddVerbsMenu.AddTrip:
            return Theme.kindIconTrip;
        default:
            return "";
        }
    }

    function run(verb: int) {
        const region = RootData.region;
        const node = addVerbsMenuId.node;
        let object = null;

        switch(verb) {
        case AddVerbsMenu.AddCave:
            object = region.addNode(node, SurveyNodeKind.Cave);
            break;
        case AddVerbsMenu.AddFolder:
            object = region.addNode(node, SurveyNodeKind.Folder);
            break;
        case AddVerbsMenu.AddSection:
            object = region.addNode(node, SurveyNodeKind.Folder, qsTr("New Section"));
            break;
        case AddVerbsMenu.AddTrip:
            node.addTrip();
            object = node.trip(node.tripCount - 1);
            break;
        case AddVerbsMenu.AttachSurveyFile:
            addVerbsMenuId.attachSurveyFileRequested(node !== null ? node : region.rootNode);
            break;
        default:
            break;
        }

        if(object !== null) {
            addVerbsMenuId.added(object, verb);
        }
    }

    //The position of \a item among the menu's entries, -1 when it holds none.
    function indexOfItem(item: QQ.Item) : int {
        for(let i = 0; i < addVerbsMenuId.count; i++) {
            if(addVerbsMenuId.itemAt(i) === item) {
                return i;
            }
        }
        return -1;
    }

    //The native verbs lead the menu, ahead of the separator, Add Survey File…,
    //and whatever the host appends.
    QQ.Instantiator {
        model: addVerbsMenuId.verbs

        delegate: QC.MenuItem {
            required property int modelData

            objectName: addVerbsMenuId.verbObjectName(modelData)
            text: addVerbsMenuId.verbText(modelData)
            icon.source: addVerbsMenuId.verbIcon(modelData)

            onTriggered: addVerbsMenuId.run(modelData)
        }

        onObjectAdded: (index, object) => addVerbsMenuId.insertItem(index, object as QC.MenuItem)

        //The Instantiator destroys what it made, so the menu gives the entry
        //up rather than destroying it too.
        onObjectRemoved: (index, object) => {
            const position = addVerbsMenuId.indexOfItem(object as QQ.Item);
            if(position >= 0) {
                addVerbsMenuId.takeItem(position);
            }
        }
    }

    QC.MenuSeparator {
        objectName: "addSurveyFileSeparator"
    }

    QC.MenuItem {
        objectName: addVerbsMenuId.verbObjectName(AddVerbsMenu.AttachSurveyFile)
        text: addVerbsMenuId.verbText(AddVerbsMenu.AttachSurveyFile)

        onTriggered: addVerbsMenuId.run(AddVerbsMenu.AttachSurveyFile)
    }
}
