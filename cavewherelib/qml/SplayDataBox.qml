/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

// One reading of one splay, as an editable cell of the survey grid. It is a
// DataBox with the parts a splay has no use for left out: nothing checks a
// splay yet, so there are no errors to show, and a splay row previews its own
// removal across the whole row rather than a reading at a time.
//
// The cell it writes back to is one of the editor's own — SplayDistanceCell,
// SplayCompassCell or SplayClinoCell — so the caller names it. The reading
// itself carries no chunk role that could name it, since a splay lives under a
// station at an index of its own.
DataBox {
    id: splayDataBox

    //DataBox reads its cell off the reading it shows, which a splay's reading
    //can't name. Required here so a call site that forgets it fails at load
    //rather than quietly writing to a shot cell
    required cellRole
}
