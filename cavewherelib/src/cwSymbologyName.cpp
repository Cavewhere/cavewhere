/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyName.h"

namespace cwSymbology {

bool isValidName(QStringView name)
{
    if (name.isEmpty()) {
        return false;
    }
    for (const QChar c : name) {
        const bool kebabChar = (c >= u'a' && c <= u'z')
                            || (c >= u'0' && c <= u'9')
                            || c == u'-';
        if (!kebabChar) {
            return false;
        }
    }
    // No leading/trailing hyphen — keeps the name canonical kebab-case (a bare
    // "-" or "--" is already excluded by the char check being the only allowed
    // punctuation).
    return name.front() != u'-' && name.back() != u'-';
}

QString slugify(const QString &name)
{
    QString slug;
    slug.reserve(name.size());
    bool lastWasDash = false;
    for (const QChar c : name) {
        if (c.isLetterOrNumber()) {
            slug.append(c.toLower());
            lastWasDash = false;
        } else if (!slug.isEmpty() && !lastWasDash) {
            slug.append(QLatin1Char('-'));
            lastWasDash = true;
        }
    }
    while (slug.endsWith(QLatin1Char('-'))) {
        slug.chop(1);
    }
    if (slug.isEmpty()) {
        slug = QStringLiteral("palette");
    }
    return slug;
}

QString uniqueName(const QSet<QString> &taken, const QString &base)
{
    QString candidate = base;
    int suffix = 2;
    while (taken.contains(candidate)) {
        candidate = QStringLiteral("%1-%2").arg(base).arg(suffix++);
    }
    return candidate;
}

}
