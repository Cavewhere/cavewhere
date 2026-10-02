/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

// The survey table's own remove challenge. A splay cluster is named by the row
// it hangs from rather than by a plain index, and the shared RemoveAskBox is
// asked for by six other pages that have no idea what a survey row is — so the
// row index rides here instead of on the box everyone uses.
RemoveAskBox {
    id: splayRemoveChallenge

    property cwSurveyEditorRowIndex rowIndexToRemove
}
