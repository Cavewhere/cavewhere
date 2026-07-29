// Shared reads over a page's list<ToolItem>. Three surfaces render the same
// model — the sidebar rail, the tool-property flyout, and the compact Tools
// drawer — and each used to work it out again.

// The model grouped by stable groupId, in first-seen order: one entry per group
// carrying its (translated) heading and the ToolItems under it.
function groupTools(toolModel) {
    let ordered = []
    // Null-prototype map so a groupId that collides with an Object.prototype
    // member name (e.g. "toString") can't poison the lookup and blank the list.
    let byId = Object.create(null)
    for (let i = 0; i < toolModel.length; i++) {
        let tool = toolModel[i]
        if (byId[tool.groupId] === undefined) {
            byId[tool.groupId] = { "group": tool.group, "tools": [] }
            ordered.push(byId[tool.groupId])
        }
        byId[tool.groupId].tools.push(tool)
    }
    return ordered
}

// The armed tool: the ToolItem whose interaction is the active one. Nothing
// armed, or an armed interaction that isn't in this page's model, yields null.
function activeTool(interactionManager, toolModel) {
    if (interactionManager === null) {
        return null
    }
    let active = interactionManager.activeInteraction
    if (!active) {
        return null
    }
    for (let i = 0; i < toolModel.length; i++) {
        if (toolModel[i].interaction === active) {
            return toolModel[i]
        }
    }
    return null
}
