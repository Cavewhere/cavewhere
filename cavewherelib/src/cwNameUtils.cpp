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

bool cwNameUtils::naturalLess(const QString& left, const QString& right)
{
    int i = 0;
    int j = 0;
    const int n = left.size();
    const int m = right.size();

    while (i < n && j < m) {
        const QChar ca = left.at(i);
        const QChar cb = right.at(j);

        if (ca.isDigit() && cb.isDigit()) {
            //Span each digit run, then compare by value (skip leading zeros so
            //"007" and "7" compare equal in magnitude, longer run breaking ties).
            int ai = i;
            int bj = j;
            while (ai < n && left.at(ai).isDigit()) { ++ai; }
            while (bj < m && right.at(bj).isDigit()) { ++bj; }

            int as = i;
            int bs = j;
            while (as < ai - 1 && left.at(as) == QLatin1Char('0')) { ++as; }
            while (bs < bj - 1 && right.at(bs) == QLatin1Char('0')) { ++bs; }

            const int aLen = ai - as;
            const int bLen = bj - bs;
            if (aLen != bLen) {
                return aLen < bLen;
            }
            for (int k = 0; k < aLen; ++k) {
                const QChar da = left.at(as + k);
                const QChar db = right.at(bs + k);
                if (da != db) {
                    return da < db;
                }
            }
            //Equal magnitude: shorter original run (fewer leading zeros) first.
            if ((ai - i) != (bj - j)) {
                return (ai - i) < (bj - j);
            }
            i = ai;
            j = bj;
        } else {
            const QChar la = ca.toCaseFolded();
            const QChar lb = cb.toCaseFolded();
            if (la != lb) {
                return la < lb;
            }
            ++i;
            ++j;
        }
    }

    return (n - i) < (m - j);
}
