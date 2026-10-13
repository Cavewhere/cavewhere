import QtQuick as QQ

// The Theme tokens as a palette. A window that takes it hands each role to its
// controls, menus, and popups, when they are created and when the scheme flips.
// A control starts from the Qt Quick Controls theme palette, which comes from
// the fallback style, and inherits only the roles an ancestor sets explicitly.
QQ.Palette {
    window: Theme.background
    base: Theme.fieldSurface
    alternateBase: Theme.rowAlternate
    button: Theme.buttonSurface
    brightText: Theme.textInverse
    highlight: Theme.highlight
    highlightedText: Theme.text
    placeholderText: Theme.fieldPlaceholder
    toolTipBase: Theme.toolTipSurface
    toolTipText: Theme.toolTipText
    link: Theme.textLink
    accent: Theme.accent
    light: Theme.buttonHover
    midlight: Theme.border
    mid: Theme.border
    dark: Theme.controlBorder
    shadow: Theme.paletteShadow

    // The three text roles are set per group: a role set on the palette itself
    // covers every group, the disabled one included.
    active.windowText: Theme.text
    active.text: Theme.text
    active.buttonText: Theme.text
    inactive.windowText: Theme.text
    inactive.text: Theme.text
    inactive.buttonText: Theme.text
    disabled.windowText: Theme.textDisabled
    disabled.text: Theme.textDisabled
    disabled.buttonText: Theme.textDisabled
}
