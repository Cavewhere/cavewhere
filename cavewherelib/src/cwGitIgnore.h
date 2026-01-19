#pragma once

#include <QDir>
#include "CaveWhereLibExport.h"

namespace cw::git {

CAVEWHERE_LIB_EXPORT void ensureGitIgnoreHasCacheEntry(const QDir& repoDir);

} // namespace cw::git
