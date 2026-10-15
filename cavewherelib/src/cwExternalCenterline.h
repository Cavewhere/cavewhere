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
#include "cwGeoPoint.h"

//Qt includes
#include <QHashFunctions>
#include <QMetaType>
#include <QString>
#include <QUuid>
#include <QtQml/qqmlregistration.h>

//Std includes
#include <optional>

/**
 * Value type identifying the entry-point centerline file (Survex,
 * Compass, or Walls) attached to a cwCave or cwTrip. The path is
 * project-relative, rooted at the owner's external-centerline/
 * subdirectory (see cwSaveLoad::externalCenterlineDir).
 *
 * An empty entry file means "no attachment" - the owner is Native
 * (per the data-model spec in
 * plans/EXTERNAL_FILE_INTEGRATION_PLAN.html section 3). Equality
 * compares every field; the hash derives from the entry file path, the
 * canonical identity of an attachment.
 */
class CAVEWHERE_LIB_EXPORT cwExternalCenterline
{
    Q_GADGET
    QML_VALUE_TYPE(cwExternalCenterline)
    Q_PROPERTY(QString entryFile READ entryFile WRITE setEntryFile FINAL)
    Q_PROPERTY(bool isEmpty READ isEmpty FINAL)
    //! The coordinate system the solve reads the attachment's fixes in when
    //! the file names none for them; empty when the user has chosen none.
    Q_PROPERTY(QString fixCoordinateSystem READ fixCoordinateSystem WRITE setFixCoordinateSystem FINAL)

public:
    cwExternalCenterline() = default;
    explicit cwExternalCenterline(QString entryFile);

    QString entryFile() const { return m_entryFile; }
    void setEntryFile(const QString& entryFile) { m_entryFile = entryFile; }

    QString fixCoordinateSystem() const { return m_fixCoordinateSystem; }
    void setFixCoordinateSystem(const QString& system) { m_fixCoordinateSystem = system; }

    bool isEmpty() const { return m_entryFile.isEmpty(); }

    bool operator==(const cwExternalCenterline& other) const
    {
        return m_entryFile == other.m_entryFile
               && m_fixCoordinateSystem == other.m_fixCoordinateSystem;
    }
    bool operator!=(const cwExternalCenterline& other) const
    {
        return !(*this == other);
    }

private:
    QString m_entryFile;
    QString m_fixCoordinateSystem;
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
    //! The coordinate text: the file's numbers; a Compass .mak fix in meters,
    //! as cavern reads it.
    Q_PROPERTY(QString coordinate MEMBER coordinate FINAL)
    //! The input coordinate system in force for the fix, empty when none.
    Q_PROPERTY(QString coordinateSystem MEMBER coordinateSystem FINAL)
    //! The attached entry file's name.
    Q_PROPERTY(QString fileName MEMBER fileName FINAL)
    //! The file whose line fixes the station, relative to the attachment's
    //! directory ("sanqiao/fixedpts/ents.svx").
    Q_PROPERTY(QString filePath MEMBER filePath FINAL)
    //! That line, counting from 1; 0 when the format's reader keeps no line.
    Q_PROPERTY(int line MEMBER line FINAL)
    //! For a fix with no coordinate system of its own, the system the solve
    //! reads it in: the one chosen for the attachment
    //! (cwExternalCenterline::fixCoordinateSystem), or, when none is chosen
    //! and the project has no frame, the *cs out the file names itself. Empty
    //! when the fix names its own system, or when nothing names one for it.
    Q_PROPERTY(QString readInSystem MEMBER readInSystem FINAL)
    //! Whether readInSystem is the *cs out the file names itself rather than
    //! the system chosen for the attachment.
    Q_PROPERTY(bool readInFileOutput MEMBER readInFileOutput FINAL)
    //! The node or trip whose attachment holds the file.
    Q_PROPERTY(QUuid ownerId MEMBER ownerId FINAL)
    //! Whether the file marks the fix as a reference point (Survex's
    //! "*fix <station> reference …"), which cavern keeps even when no leg uses it.
    Q_PROPERTY(bool reference MEMBER reference FINAL)

public:
    QString station;
    QString coordinate;
    QString coordinateSystem;
    QString fileName;
    QString filePath;
    int line = 0;
    QString readInSystem;
    bool reference = false;
    bool readInFileOutput = false;
    QUuid ownerId;

    //! The note the Fix Stations page shows on the row: where the fix is read
    //! in and which line left it there. Empty when readInSystem is.
    QString readInWarning() const;

    //! The system the driver solves the fix in: its own coordinateSystem, else
    //! readInSystem. Empty when nothing names one.
    QString solveSystem() const;

    //! Where the fix sits once read in a system: the PROJ text of that system
    //! and the point in its own axis order.
    struct Placement
    {
        QString cs;
        cwGeoPoint point;
    };

    //! The fix's coordinate read in \a system, spelled the way the driver
    //! writes it after *cs (a Survex name, or else PROJ text). Empty when the
    //! system names no valid CRS or the coordinate does not parse.
    std::optional<Placement> placementIn(const QString& system) const;

    bool operator==(const cwAttachedFix& other) const = default;
};

Q_DECLARE_METATYPE(cwAttachedFix)

#endif // CWEXTERNALCENTERLINE_H
