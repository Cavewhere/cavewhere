/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef TEST_HELPER
#define TEST_HELPER

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
#include <QModelIndex>
#include <QMetaEnum>
#include <QTextStream>
#include <QVector2D>
#include <QMatrix4x4>
#include <QQmlEngine>
#include <QModelIndex>

//Our includes
#include "cwStationPositionLookup.h"
#include "cwProject.h"
#include "cwKeyword.h"
#include "cwError.h"
#include "cwImageData.h"
#include "CaveWhereTestLibExport.h"
#include "LoadProjectHelper.h"
#include "ProjectFilenameTestHelper.h"

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

inline std::ostream& operator << ( std::ostream& os, const QModelIndex& value) {

    auto printIndex = [&]() {
        os << "(" << value.row() << "," << value.column() << ")";
    };

    if(value.parent() != QModelIndex()) {
        os << value.parent() << "->"; //Recursive printing
        printIndex();
    } else {
        os << "Model:" << value.model() << " ";
        printIndex();
    }

    return os;
}

inline std::ostream& operator <<( std::ostream& os, QPersistentModelIndex value) {
    return os << static_cast<QModelIndex>(value);
}

inline std::ostream& operator << ( std::ostream& os, cwKeyword const& value) {
    os << "(" << value.key() << "=" << value.value() << ")`";
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
    for(const auto& image : value) {
        os << image << ",";
    }
    return os;
}

// inline std::ostream& operator << ( std::ostream& os, const QModelIndex& index) {
//     os << "model:" << index.model() << "[" << index.row() << "," << index.column() << "," << index.internalId() << "]";
//     return os;
// }

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

inline std::ostream &operator << (std::ostream& os, QPointF point) {
    os << "(" << point.x() << ", " << point.y() << ")";
    return os;
}

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
 *`
 * This needs to be inline for catch to work on windows (because this is a library)
 */
void checkStationLookup(cwStationPositionLookup lookup1, cwStationPositionLookup lookup2);



#endif // STREAMOPERATOR
