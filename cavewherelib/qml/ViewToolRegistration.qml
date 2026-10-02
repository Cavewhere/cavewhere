/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// A view tool's entry in ViewTools, declared as a child of the tool itself. It
// carries the name other pages arm the tool by, and emits armed() when one of
// them does. The tool turns itself on from that handler, so ViewTools never
// calls into the tool and holds nothing but this object — which drops out of
// the registry with the tool that owns it.
QQ.QtObject {
    id: registrationId

    required property string toolName

    signal armed()

    QQ.Component.onCompleted: ViewTools.register(registrationId)
    QQ.Component.onDestruction: ViewTools.unregister(registrationId)
}
