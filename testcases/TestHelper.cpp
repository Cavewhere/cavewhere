//Std includes
#include <iostream>
#include <memory>

//Our includes
#include "cwProject.h"
#include "LoadProjectHelper.h"
#include "TestHelper.h"

//Qt includes
#include <QVector3D>

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>


double roundToDecimal(double value, int decimals) {
    double decimalPlaces = 10.0 * decimals;
    return qRound(value * decimalPlaces) / decimalPlaces;
}

QVector3D roundToDecimal(QVector3D v, int decimals) {
    return QVector3D(roundToDecimal(v.x(), decimals),
                     roundToDecimal(v.y(), decimals),
                     roundToDecimal(v.z(), decimals));
}

std::ostream &operator <<(std::ostream &os, const QStringList &value) {
    if(!value.isEmpty()) {
        os << "[";
        for(auto iter = value.begin(); iter != value.end() - 1; iter++) {
            os << "\"" + iter->toStdString() + "\",";
        }
        os << "\"" + value.last() + "\"]";
    } else {
        os << "[]";
    }
    return os;
}

std::ostream &operator <<(std::ostream &os, const cwError &error) {
    QMetaEnum metaEnum = QMetaEnum::fromType<cwError::ErrorType>();
    os << "(typeId:" << error.errorTypeId() << ", " << metaEnum.valueToKey(error.type()) << ", \'" << error.message().toStdString() << "'" << ", suppressed:" << error.suppressed() << ")";
    return os;
}


void fuzzyCompareVector(QVector3D v1, QVector3D v2, double delta)
{
    using Catch::Matchers::WithinAbs;
    INFO(v1 << "==" << v2);
    CHECK_THAT(v1.x(), WithinAbs(v2.x(), delta));
    CHECK_THAT(v1.y(), WithinAbs(v2.y(), delta));
    CHECK_THAT(v1.z(), WithinAbs(v2.z(), delta));
}

std::ostream &operator <<(std::ostream &os, const QList<int> &value) {
    os << "[";
    for(int i = 0; i < value.size(); i++) {
        if(i < value.size() - 1) {
            os << i << " ";
        } else {
            os << i;
        }
    }
    os << "]";
    return os;
}

void propertyCompare(QObject *tc1, QObject *tc2) {

    for(int i = 0; i < tc1->metaObject()->propertyCount(); i++) {
        QMetaProperty property = tc1->metaObject()->property(i);
        INFO("Testing property:" << property);
        CHECK(property.read(tc1) == property.read(tc2));
    }
}

void checkQVector3D(QVector3D v1, QVector3D v2, int decimals) {
    if(v1 != v2) {
        v1 = roundToDecimal(v1, decimals);
        v2 = roundToDecimal(v2, decimals);
        CHECK(v1 == v2);
    }
}

void checkStationLookup(cwStationPositionLookup lookup1, cwStationPositionLookup lookup2) {
    foreach(QString stationName, lookup1.positions().keys()) {
        INFO("Checking position for " << stationName);
        CHECK(lookup2.hasPosition(stationName) == true);
        checkQVector3D(lookup1.position(stationName), lookup2.position(stationName));
    }
}

