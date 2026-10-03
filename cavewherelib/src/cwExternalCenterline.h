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
#include <QtQml/qqmlregistration.h>

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
    Q_GADGET
    QML_VALUE_TYPE(cwExternalCenterline)
    Q_PROPERTY(QString entryFile READ entryFile WRITE setEntryFile FINAL)
    Q_PROPERTY(bool isEmpty READ isEmpty FINAL)
    Q_PROPERTY(QString format READ format FINAL)

public:
    cwExternalCenterline() = default;
    explicit cwExternalCenterline(QString entryFile);

    QString entryFile() const { return m_entryFile; }
    void setEntryFile(const QString& entryFile) { m_entryFile = entryFile; }

    // Display name of the entry file's survey format ("Survex",
    // "Compass", "Walls"; empty when unrecognized or unattached),
    // derived from the extension via the scanner's formatFor.
    QString format() const;

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

/**
 * A station an attached survey file fixes itself, as the attach scan read it
 * (cwExternalCenterlineScanner::ScannedFix), plus the attached file that
 * carries it. A node's Fix Stations page lists these read-only beside the
 * node's own fixes, since a node fix on one of them is refused.
 */
class CAVEWHERE_LIB_EXPORT cwAttachedFix
{
    Q_GADGET
    QML_VALUE_TYPE(cwAttachedFix)
    //! The station as a fix on the node names it ("doghill.d1"): qualified by
    //! the file's own blocks, and by the trip's scopePrefix() for a trip's file.
    Q_PROPERTY(QString station MEMBER station FINAL)
    //! The coordinate text as the file writes it.
    Q_PROPERTY(QString coordinate MEMBER coordinate FINAL)
    //! The input coordinate system in force for the fix, empty when none.
    Q_PROPERTY(QString coordinateSystem MEMBER coordinateSystem FINAL)
    //! The attached entry file's name.
    Q_PROPERTY(QString fileName MEMBER fileName FINAL)

public:
    QString station;
    QString coordinate;
    QString coordinateSystem;
    QString fileName;

    bool operator==(const cwAttachedFix& other) const = default;
};

Q_DECLARE_METATYPE(cwAttachedFix)

#endif // CWEXTERNALCENTERLINE_H
