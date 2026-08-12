/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

// One reading of one splay, as an editable cell of the survey grid. It is a
// DataBox with the parts a splay has no use for left out: a splay row previews
// its own removal across the whole row rather than a reading at a time. A
// splay's readings are checked by the rules a shot's are, so the error box and
// its list come straight from DataBox — the model hands the box the error
// model the chunk keeps for that reading.
//
// The cell it writes back to is one of the editor's own — SplayDistanceCell,
// SplayCompassCell or SplayClinoCell — and the model's box data names it, the
// same way a shot box's data names its shot cell.
DataBox {
    id: splayDataBox
}
