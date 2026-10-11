/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalCenterline.h"
#include "cwExternalCenterlineScanner.h"

cwExternalCenterline::cwExternalCenterline(QString entryFile)
    : m_entryFile(std::move(entryFile))
{
}

QString cwExternalCenterline::format() const
{
    return cwExternalCenterlineScanner::formatName(
        cwExternalCenterlineScanner::formatFor(m_entryFile));
}

size_t qHash(const cwExternalCenterline& value, size_t seed) noexcept
{
    return qHash(value.entryFile(), seed);
}

QString cwAttachedFix::readInWarning() const
{
    if (readInSystem.isEmpty()) {
        return QString();
    }
    const QString where = line > 0
        ? QStringLiteral("%1 line %2").arg(filePath).arg(line)
        : filePath;
    const QString source = readInFileOutput
        ? QStringLiteral("the system the file's *cs out names")
        : QStringLiteral("the system chosen for %1").arg(fileName);
    return QStringLiteral("Read in %1, %2: %3 names no coordinate system for this fix.")
        .arg(readInSystem, source, where);
}
