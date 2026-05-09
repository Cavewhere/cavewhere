/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATION_H
#define CWFIXSTATION_H

//Qt includes
#include <QSharedDataPointer>
#include <QString>
#include <QUuid>
#include <QMetaType>

//Our includes
#include "cwGlobals.h"

class cwFixStationData;

/**
 * Value class anchoring a named station to absolute coordinates in some
 * input coordinate system. Lives inside cwFixStationModel; serialized as
 * part of cwCave.
 *
 * Copyable / equality-comparable; uses QSharedDataPointer for COW so cheap
 * to pass through QVariant and signals.
 */
class CAVEWHERE_LIB_EXPORT cwFixStation
{
public:
    cwFixStation();
    cwFixStation(const cwFixStation& other);
    cwFixStation& operator=(const cwFixStation& other);
    ~cwFixStation();

    bool operator==(const cwFixStation& other) const;
    bool operator!=(const cwFixStation& other) const { return !(*this == other); }

    QUuid id() const;
    void setId(const QUuid& id);

    QString stationName() const;
    void setStationName(const QString& name);

    QString inputCS() const;
    void setInputCS(const QString& cs);

    double easting() const;
    void setEasting(double v);

    double northing() const;
    void setNorthing(double v);

    double elevation() const;
    void setElevation(double v);

    double horizontalVariance() const;
    void setHorizontalVariance(double v);

    double verticalVariance() const;
    void setVerticalVariance(double v);

private:
    QSharedDataPointer<cwFixStationData> data;
};

Q_DECLARE_METATYPE(cwFixStation)

#endif // CWFIXSTATION_H
