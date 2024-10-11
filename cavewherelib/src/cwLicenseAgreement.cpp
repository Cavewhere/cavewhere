/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLicenseAgreement.h"

//Qt includes
#include <QFile>
#include <QSettings>

cwLicenseAgreement::cwLicenseAgreement(QObject *parent) :
    QObject(parent)
{
}

/**
Gets text
*/
QString cwLicenseAgreement::text() const {
    QFile file(":/LICENSE.txt");
    bool okay = file.open(QFile::ReadOnly);

    if(!okay) {
        return "Error: " + file.errorString();
    }

    return file.readAll();
}

/**
 * @brief cwLicenseAgreement::setVersion
 * @param version
 */
void cwLicenseAgreement::setVersion(QString version)
{
    Version = version;
}

/**
Gets hasReadLicenseAgreement
*/
bool cwLicenseAgreement::hasReadLicenseAgreement() const {
    QSettings settings;
    QVariant readLicenseVariant = settings.value("readLicense");
    if(!readLicenseVariant.isNull() && readLicenseVariant.canConvert<bool>()) {
        return readLicenseVariant.toBool();
    }
    return false;
}


/**
Sets hasReadLicenseAgreement
*/
void cwLicenseAgreement::setHasReadLicenseAgreement(bool hasReadLicenseAgreement) {
    QSettings settings;
    settings.setValue("readLicense", hasReadLicenseAgreement);

    //This save the version that the user accepted the license
    //This will allow use to change the license, and make the user
    //re-accept the new licence
    settings.setValue("acceptLicenseVersion", Version);

    emit hasReadLicenseAgreementChanged();
}
