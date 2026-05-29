/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALCENTERLINE_H
#define CWEXTERNALCENTERLINE_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QHashFunctions>
#include <QMetaType>
#include <QString>

/**
 * Value type identifying the entry-point centerline file (Survex,
 * Compass, or Walls) attached to a cwCave or cwTrip. The path is
 * project-relative, rooted at the owner's external-centerline/
 * subdirectory (see cwSaveLoad::externalCenterlineDir).
 *
 * An empty entry file means "no attachment" - the owner is Native
 * (per the data-model spec in
 * plans/EXTERNAL_FILE_INTEGRATION_PLAN.html section 3). Equality and
 * hash derive solely from the entry file path, which is the
 * canonical identity of an attachment for this commit.
 */
class CAVEWHERE_LIB_EXPORT cwExternalCenterline
{
public:
    cwExternalCenterline() = default;
    explicit cwExternalCenterline(QString entryFile);

    QString entryFile() const { return m_entryFile; }
    void setEntryFile(const QString& entryFile) { m_entryFile = entryFile; }

    bool isEmpty() const { return m_entryFile.isEmpty(); }

    bool operator==(const cwExternalCenterline& other) const
    {
        return m_entryFile == other.m_entryFile;
    }
    bool operator!=(const cwExternalCenterline& other) const
    {
        return !(*this == other);
    }

private:
    QString m_entryFile;
};

CAVEWHERE_LIB_EXPORT size_t qHash(const cwExternalCenterline& value, size_t seed = 0) noexcept;

Q_DECLARE_METATYPE(cwExternalCenterline)

#endif // CWEXTERNALCENTERLINE_H
