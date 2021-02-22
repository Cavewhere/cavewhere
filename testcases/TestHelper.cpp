#include "TestHelper.h"
#include <catch2/catch.hpp>

//Qt includes
#include <QStandardPaths>

std::shared_ptr<cwProject> fileToProject(QString filename) {
    auto project = std::make_shared<cwProject>();
    fileToProject(project.get(), filename);
    return project;
}

void fileToProject(cwProject *project, const QString &filename) {
    QString datasetFile = copyToTempFolder(filename);

    project->loadFile(datasetFile);
    project->waitLoadToFinish();
}

QString copyToTempFolder(QString filename) {

    QFileInfo info(filename);
    QString newFileLocation = QDir::tempPath() + "/" + info.fileName();

    if(QFileInfo::exists(newFileLocation)) {
        QFile file(newFileLocation);
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadUser);

        bool couldRemove = file.remove();
        INFO("Trying to remove " << newFileLocation);
        REQUIRE(couldRemove == true);
    }

    bool couldCopy = QFile::copy(filename, newFileLocation);
    INFO("Trying to copy " << filename << " to " << newFileLocation);
    REQUIRE(couldCopy == true);

    bool couldPermissions = QFile::setPermissions(newFileLocation, QFile::WriteOwner | QFile::ReadOwner);
    INFO("Trying to set permissions for " << filename);
    REQUIRE(couldPermissions);

    return newFileLocation;
}

void checkStationLookup(cwStationPositionLookup lookup1, cwStationPositionLookup lookup2) {
    foreach(QString stationName, lookup1.positions().keys()) {
        INFO("Checking position for " << stationName);
        CHECK(lookup2.hasPosition(stationName) == true);
        checkQVector3D(lookup1.position(stationName), lookup2.position(stationName));
    }
}

void checkQVector3D(QVector3D v1, QVector3D v2, int decimals) {
    if(v1 != v2) {
        v1 = roundToDecimal(v1, decimals);
        v2 = roundToDecimal(v2, decimals);
        CHECK(v1 == v2);
    }
}

QVector3D roundToDecimal(QVector3D v, int decimals) {
    return QVector3D(roundToDecimal(v.x(), decimals),
                     roundToDecimal(v.y(), decimals),
                     roundToDecimal(v.z(), decimals));
}

void propertyCompare(QObject *tc1, QObject *tc2) {

    for(int i = 0; i < tc1->metaObject()->propertyCount(); i++) {
        QMetaProperty property = tc1->metaObject()->property(i);
        INFO("Testing property " << property);
        CHECK(property.read(tc1) == property.read(tc2));
    }
}

double roundToDecimal(double value, int decimals) {
    double decimalPlaces = 10.0 * decimals;
    return qRound(value * decimalPlaces) / decimalPlaces;
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

QString prependTempFolder(QString filename)
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + filename;
}

void fuzzyCompareVector(QVector3D v1, QVector3D v2, double delta)
{
    INFO(v1 << "==" << v2);
    CHECK(v1.x() == Approx(v2.x()).margin(delta));
    CHECK(v1.y() == Approx(v2.y()).margin(delta));
    CHECK(v1.z() == Approx(v2.z()).margin(delta));
}
