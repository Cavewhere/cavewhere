#include "cwGitIgnore.h"

#include <QFile>

namespace cw::git {

void ensureGitIgnoreHasCacheEntry(const QDir& repoDir)
{
    const QString gitignorePath = repoDir.filePath(QStringLiteral(".gitignore"));
    QByteArray existingContents;
    if (QFile::exists(gitignorePath)) {
        QFile readFile(gitignorePath);
        if (readFile.open(QIODevice::ReadOnly)) {
            existingContents = readFile.readAll();
        }
    }

    const QString entry = QStringLiteral(".cw_cache/");
    const QStringList lines = QString::fromUtf8(existingContents).split('\n');
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed == entry || trimmed == QStringLiteral(".cw_cache")) {
            return;
        }
    }

    QFile writeFile(gitignorePath);
    if (!writeFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return;
    }

    if (!existingContents.isEmpty() && !existingContents.endsWith('\n')) {
        writeFile.write("\n");
    }
    writeFile.write(".cw_cache/\n");
}

} // namespace cw::git
