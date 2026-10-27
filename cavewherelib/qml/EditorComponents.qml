/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
pragma Singleton

import QtQuick as QQ
import cavewherelib

/**
  The shared editor components a CoreClickTextInput can name as its
  editorComponent.

  They live here, one per editor *kind*, rather than being declared inline at
  each field: an inline component is wrapped per declaring instance, so two
  fields of the same kind would hand the shared editor host two different
  components and make it tear the editor down and rebuild it on every move
  between them.

  An editor takes what it needs from the field it is given
  (ShadowTextEditor.field), so nothing here is per field.
 */
QQ.QtObject {
    readonly property QQ.Component stationName: QQ.Component {
        StationNameEditor {}
    }
}
