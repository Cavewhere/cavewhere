/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLICENSEAGREEMENT_H
#define CWLICENSEAGREEMENT_H

#include <QObject>
#include <QQmlEngine>

class cwLicenseAgreement : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LicenseAgreement)

    Q_PROPERTY(bool hasReadLicenseAgreement READ hasReadLicenseAgreement WRITE setHasReadLicenseAgreement NOTIFY hasReadLicenseAgreementChanged)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)



public:
    explicit cwLicenseAgreement(QObject *parent = 0);

    bool hasReadLicenseAgreement() const;
    void setHasReadLicenseAgreement(bool hasReadLicenseAgreement);

    QString text() const;

    void setVersion(QString version);

signals:
    void hasReadLicenseAgreementChanged();
    void textChanged();

public slots:

private:
    QString Version;
    
};

#endif // CWLICENSEAGREEMENT_H
