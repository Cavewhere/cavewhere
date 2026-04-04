#include "cwNameUtils.h"
#include "cwSanitizedNameSet.h"

QString cwNameUtils::sanitizeFileName(QString input)
{
    static const QString forbidden = R"(\/:*?"<>|)";
    for (QChar& ch : input) {
        if (forbidden.contains(ch)) {
            ch = u'_';
        }
    }

    input = input.trimmed();
    while (input.startsWith('.')) input = input.mid(1);
    while (input.endsWith('.'))  input.chop(1);

    if (input.isEmpty()) {
        input = "untitled";
    }

    return input;
}

QString cwNameUtils::validateEntityName(const QString& currentName,
                                        const QString& proposedName,
                                        const cwSanitizedNameSet* nameSet,
                                        const QString& entityLabel)
{
    if (proposedName.isEmpty()) {
        return QStringLiteral("Name cannot be empty.");
    }

    const QString sanitized = sanitizeFileName(proposedName);
    if (sanitized != proposedName) {
        return QStringLiteral("Name contains characters that would be removed: \"%1\"")
            .arg(sanitized);
    }

    // Use pre-computed keys to avoid re-sanitizing in wouldCollide.
    if (nameSet) {
        const QString currentKey = cwSanitizedNameSet::toKey(currentName);
        const QString newKey = sanitized.toLower(); // sanitized == proposedName here
        if (nameSet->wouldCollideKey(currentKey, newKey)) {
            return QStringLiteral("A %1 with that name already exists.").arg(entityLabel);
        }
    }

    return {};
}
