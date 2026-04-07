#pragma once

#include "cwGlobals.h"

#include <QString>

class cwSanitizedNameSet;

namespace cwNameUtils {

// Replaces forbidden filesystem characters with '_', trims whitespace/dots,
// returns "untitled" for empty results.
CAVEWHERE_LIB_EXPORT QString sanitizeFileName(QString input);

// Validates a proposed entity name against emptiness, forbidden characters,
// and sibling collisions.  Returns an empty string on success or an error
// message describing the problem.  nameSet may be nullptr when the entity
// has no parent yet (collision check is skipped).  entityLabel is used in
// the collision error message (e.g. "cave", "trip", "note").
CAVEWHERE_LIB_EXPORT QString validateEntityName(const QString& currentName,
                                                 const QString& proposedName,
                                                 const cwSanitizedNameSet* nameSet,
                                                 const QString& entityLabel);

} // namespace cwNameUtils
