/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef STREAMOPERATOR
#define STREAMOPERATOR

//Catch includes
#include "catch.hpp"

//Qt includes
#include <QVector3D>
#include <QString>
#include <QVariant>
#include <QObject>
#include <QMetaProperty>
#include <QFile>
#include <QFileInfo>
#include <QDir>

//Our includes
#include "cwStationPositionLookup.h"

inline std::ostream& operator << ( std::ostream& os, QVector3D const& value ) {
    os << "(" << value.x() << ", " << value.y() << ", " << value.z() << ")";
    return os;
}

inline std::ostream& operator << ( std::ostream& os, QString const& value ) {
    os << value.toStdString();
    return os;
}

inline std::ostream& operator << ( std::ostream& os, QVariant const& value ) {
    os << value.toString().toStdString();
    return os;
}

inline std::ostream& operator << ( std::ostream& os, QMetaProperty const& value ) {
    os << value.name() << " type:" << value.typeName();
    return os;
}

inline void propertyCompare(QObject* tc1, QObject* tc2) {

    for(int i = 0; i < tc1->metaObject()->propertyCount(); i++) {
        QMetaProperty property = tc1->metaObject()->property(i);
        INFO("Testing property " << property);
        CHECK(property.read(tc1) == property.read(tc2));
    }
}

inline double roundToDecimal(double value, int decimals) {
    double decimalPlaces = 10.0 * decimals;
    return qRound(value * decimalPlaces) / decimalPlaces;
}

inline QVector3D roundToDecimal(QVector3D v, int decimals) {
    return QVector3D(roundToDecimal(v.x(), decimals),
                     roundToDecimal(v.y(), decimals),
                     roundToDecimal(v.z(), decimals));
}

inline void checkQVector3D(QVector3D v1, QVector3D v2, int decimals = 2) {
    if(v1 != v2) {
        v1 = roundToDecimal(v1, decimals);
        v2 = roundToDecimal(v2, decimals);
        CHECK(v1 == v2);
    }
}

/**
 * @brief checkStationLookup
 * @param lookup1
 * @param lookup2
 *
 * This isn't an exact eqauls comparison. This just makes sure all the stations in lookup1 are
 * equal to the stations in lookup2. Lookup2 could have extra stations that aren't checked. We
 * need this exact functionality for compass import export test. The compass exporter creates
 * extra stations at the end of the survey that has lrud data.
 */
inline void checkStationLookup(cwStationPositionLookup lookup1, cwStationPositionLookup lookup2) {
    foreach(QString stationName, lookup1.positions().keys()) {
        INFO("Checking position for " << stationName);
        CHECK(lookup2.hasPosition(stationName) == true);
        checkQVector3D(lookup1.position(stationName), lookup2.position(stationName));
    }
}

inline QString copyToTempFolder(QString filename) {

    QFileInfo info(filename);
    QString newFileLocation = QDir::tempPath() + "/" + info.fileName();

    QFile::remove(newFileLocation);

    bool couldCopy = QFile::copy(filename, newFileLocation);
    REQUIRE(couldCopy == true);

    return newFileLocation;
}

#endif // STREAMOPERATOR

