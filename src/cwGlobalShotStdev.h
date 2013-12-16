/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWGLOBALSHOTSTDEV_H
#define CWGLOBALSHOTSTDEV_H

#include <QObject>

class cwShotStdev {
public:

    cwShotStdev() {
        DistanceStd = 0.05;
        CompassStd = 1.25; //In degrees
        ClinoStd = 1.25; //In degrees
    }

    double DistanceStd; //!<
    double ClinoStd; //!<
    double CompassStd; //!<
};

class cwGlobalShotStdev : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double distanceStd READ distanceStd WRITE setDistanceStd NOTIFY distanceStdChanged)
    Q_PROPERTY(double clinoStd READ clinoStd WRITE setClinoStd NOTIFY clinoStdChanged)
    Q_PROPERTY(double compassStd READ compassStd WRITE setCompassStd NOTIFY compassStdChanged)

public:
    explicit cwGlobalShotStdev(QObject *parent = 0);

    double distanceStd() const;
    void setDistanceStd(double distanceStd);

    double clinoStd() const;
    void setClinoStd(double clinoStd);

    double compassStd() const;
    void setCompassStd(double compassStd);

    cwShotStdev data() const;
    void setData(cwShotStdev newData);

signals:
    void distanceStdChanged();
    void clinoStdChanged();
    void compassStdChanged();

public slots:

private:
    cwShotStdev Data;

};

/**
Gets distanceStd
*/
inline double cwGlobalShotStdev::distanceStd() const {
    return Data.DistanceStd;
}

/**
  Gets clinoStd
  */
inline double cwGlobalShotStdev::clinoStd() const {
    return Data.ClinoStd;
}

/**
Gets compassStd
*/
inline double cwGlobalShotStdev::compassStd() const {
    return Data.CompassStd;
}
#endif // CWGLOBALSHOTSTDEV_H
