#pragma once

#include <QDir>

namespace cw::git {

void ensureGitIgnoreHasCacheEntry(const QDir& repoDir);

} // namespace cw::git
