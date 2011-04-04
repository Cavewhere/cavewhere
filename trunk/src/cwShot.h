#ifndef CSHOT_H
#define CSHOT_H

//Our includes
class cwSurveyChunk;
class cwStation;
class cwStationReference;

//Qt includes
#include <QObject>
#include <QVariant>

class cwShot : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString Distance READ GetDistance WRITE SetDistance NOTIFY DistanceChanged);
    Q_PROPERTY(QString Compass READ GetCompass WRITE SetCompass NOTIFY CompassChanged);
    Q_PROPERTY(QString BackCompass READ GetBackCompass WRITE SetBackCompass NOTIFY BackCompassChanged);
    Q_PROPERTY(QString Clino READ GetClino WRITE SetClino NOTIFY ClinoChanged);
    Q_PROPERTY(QString BackClino READ GetBackClino WRITE SetBackClino NOTIFY BackClinoChanged);

public:
    explicit cwShot(QObject *parent = 0);

    cwShot(QString distance, QString compass, QString backCompass, QString clino, QString backClino, QObject* parent = 0);
    cwShot(const cwShot& shot);

    QString GetDistance() const;
    void SetDistance(QString distance);

    QString GetCompass() const;
    void SetCompass(QString compass);

    QString GetBackCompass() const;
    void SetBackCompass(QString backCompass);

    QString GetClino() const;
    void SetClino(QString backClino);

    QString GetBackClino() const;
    void SetBackClino(QString backClino);

    cwSurveyChunk* parentChunk() const;
    cwStationReference* toStation() const;
    cwStationReference* fromStation() const;


signals:
    void DistanceChanged();
    void CompassChanged();
    void BackCompassChanged();
    void ClinoChanged();
    void BackClinoChanged();

private:
    QString Distance;
    QString Compass;
    QString BackCompass;
    QString Clino;
    QString BackClino;
};

inline QString cwShot::GetDistance() const {
    return Distance;
}

inline QString cwShot::GetCompass() const {
    return Compass;
}

inline QString cwShot::GetBackCompass() const {
    return BackCompass;
}

inline QString cwShot::GetClino() const {
    return Clino;
}

inline QString cwShot::GetBackClino() const {
    return BackClino;
}


#endif // CSHOT_H
