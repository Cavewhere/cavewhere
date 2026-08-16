/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma Singleton

import QtQuick as QQ
import cavewherelib

// "Take me to the 3D view and turn tool X on" — the one route every page uses
// to start an interaction that lives on the View page.
//
// The awkward part of that sentence is the *and*: the button that asks is on
// another page, and the tool it wants may not even be built yet. So the ask is
// recorded as a name here, and the tool answers it whenever it can — on the
// spot if it is already registered and the view is current, otherwise the
// moment it registers. Naming the tool decouples the two: the Map page's Add
// Layer button holds no reference to the tool it arms.
//
// Tool *arguments* stay out of this deliberately (a typed seam can't carry an
// arbitrary payload, and QML has no untyped properties here). A tool that needs
// a target keeps its own typed singleton for it and arms itself through this
// one — see FixStationPick.
QQ.QtObject {
    id: viewTools

    // The tool vocabulary, spelled once. These are matched against
    // ViewToolRegistration.toolName, so nothing but this file can drift.
    readonly property string selectExportArea: "SELECT-EXPORT-AREA"

    // The page every armable tool overlays.
    readonly property string viewPageName: "View"

    // The tool waiting for its chance, empty when nothing is waiting. Read-only
    // to everyone outside this file: arm(), cancelArm() and activation own it.
    property string pendingTool: ""

    property list<ViewToolRegistration> registrations: []

    readonly property bool viewPageCurrent: {
        let page = RootData.pageSelectionModel.currentPage
        return page !== null && page.name === viewTools.viewPageName
    }

    // Records the ask and heads for the view. A second arm() replaces the first
    // — the last button pressed is the one the user is waiting on.
    function arm(toolName: string) {
        if (toolName === "") {
            console.warn("ViewTools.arm() was given an empty tool name")
            return
        }

        viewTools.pendingTool = toolName
        RootData.pageSelectionModel.gotoPageByName(null, viewTools.viewPageName)
        viewTools.activatePending()
    }

    // Drops an ask that was never answered — for a tool that gives up on its
    // own target, or a surface that changes its mind before the view appears.
    function cancelArm() {
        viewTools.pendingTool = ""
    }

    function register(registration: ViewToolRegistration) {
        if (registration === null) {
            return
        }

        viewTools.registrations.push(registration)
        viewTools.activatePending()
    }

    function unregister(registration: ViewToolRegistration) {
        let index = viewTools.registrations.indexOf(registration)
        if (index >= 0) {
            viewTools.registrations.splice(index, 1)
        }
    }

    // register() is the only way in, so every element here is a live
    // registration and this needs no null handling of its own.
    function findRegistration(toolName: string) : ViewToolRegistration {
        return viewTools.registrations.find(
            (registration) => registration.toolName === toolName) ?? null
    }

    function activatePending() {
        if (viewTools.pendingTool === "" || !viewTools.viewPageCurrent) {
            return
        }

        let registration = viewTools.findRegistration(viewTools.pendingTool)
        if (registration === null) {
            return
        }

        // Cleared first, so a tool that arms something else from its own
        // handler keeps the ask it just made.
        viewTools.pendingTool = ""
        registration.armed()
    }

    // A pending ask survives a trip to another page: it is answered by the tool
    // registering, not by the navigation, and clearing it here would silently
    // drop an arm() that raced the view's construction. cancelArm() is how an
    // ask ends early.
    onViewPageCurrentChanged: viewTools.activatePending()
}
