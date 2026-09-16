/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma Singleton

import cavewherelib

// The one survey tree of the project: every cave, the nodes under it and each
// node's trips. Every SurveyTreeView filters and roots this model instead of
// building one of its own, so a trip's length and used-station tasks run once
// for the project however many trees are open — the Data page's whole-region
// tree and a cave page's tree share these rows.
SurveyTreeModel {
    region: RootData.region
}
