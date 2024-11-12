/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef STREAMOPERATOR
#define STREAMOPERATOR

//Qt includes
#include <QVector3D>
#include <QString>
#include <QVariant>
#include <QObject>
#include <QMetaProperty>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QMetaEnum>
#include <QTextStream>
#include <QVector2D>
#include <QMatrix4x4>

//Our includes
#include "cwStationPositionLookup.h"
#include "cwProject.h"
#include "cwError.h"
#include "cwImageData.h"

//Std includes
#include <iostream>

inline std::ostream& operator << ( std::ostream& os, QVector3D const& value ) {
    os << "(" << value.x() << ", " << value.y() << ", " << value.z() << ")";
    return os;
}

inline std::ostream& operator << ( std::ostream& os, QString const& value ) {
    os << value.toStdString();
    return os;
}

 std::ostream& operator << ( std::ostream& os, QStringList const& value );

inline std::ostream& operator << ( std::ostream& os, QSet<QString> const& value ) {
    return operator <<(os, value.values());
}

inline std::ostream& operator << ( std::ostream& os, QVariant const& value ) {
    os << value.toString().toStdString();
    return os;
}

inline std::ostream& operator << ( std::ostream& os, QMetaProperty const& value ) {
    os << value.name() << " type:" << value.typeName();
    return os;
}

inline std::ostream& operator << ( std::ostream& os, QSize const& value ) {
    os << "(" << value.width() << "x" << value.height() << ")";
    return os;
}

inline std::ostream& operator << ( std::ostream& os, QSizeF const& value ) {
    os << "(" << value.width() << "x" << value.height() << ")";
    return os;
}

inline std::ostream& operator << ( std::ostream& os, cwImageData const& value ) {
    return os << "[size=" << value.size() << "]";
}

inline std::ostream& operator << ( std::ostream& os, QVector2D const& value ) {
    return os << "(" << value.x() << ", " << value.y() << ")";
}

inline std::ostream& operator << ( std::ostream& os, QList<cwImageData> const& value ) {
    for(auto image : value) {
        os << image << ",";
    }
    return os;
}

std::ostream& operator << ( std::ostream& os, QList<int> const& value);

inline std::ostream& operator << ( std::ostream& os, const QMatrix4x4& matrix) {
    os << "[\n";
    for(int r = 0; r < 4; r++) {
        for(int c = 0; c < 4; c++) {
            os << matrix.data()[r * 4 + c];

            if(c != 3) {
                os << ",";
            }
        }
        os << "\n";
    }
    os << "]";
    return os;
}


std::ostream &operator << ( std::ostream& os, cwError const& error);

void propertyCompare(QObject* tc1, QObject* tc2);

double roundToDecimal(double value, int decimals);

QVector3D roundToDecimal(QVector3D v, int decimals);

void checkQVector3D(QVector3D v1, QVector3D v2, int decimals = 2);

void fuzzyCompareVector(QVector3D v1, QVector3D v2, double delta = 0.000001);

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
void checkStationLookup(cwStationPositionLookup lookup1, cwStationPositionLookup lookup2);

/**
 * Copyies filename to the temp folder
 */
QString copyToTempFolder(QString filename);

QString prependTempFolder(QString filename);

/**
 * @brief fileToProject
 * @param filename
 * @return A new project generate from filename
 */
std::shared_ptr<cwProject> fileToProject(QString filename);
void fileToProject(cwProject* project, const QString& filename);

#endif // STREAMOPERATOR

