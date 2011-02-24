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
    Q_PROPERTY(QVariant Distance READ GetDistance WRITE SetDistance NOTIFY DistanceChanged);
    Q_PROPERTY(QVariant Compass READ GetCompass WRITE SetCompass NOTIFY CompassChanged);
    Q_PROPERTY(QVariant BackCompass READ GetBackCompass WRITE SetBackCompass NOTIFY BackCompassChanged);
    Q_PROPERTY(QVariant Clino READ GetClino WRITE SetClino NOTIFY ClinoChanged);
    Q_PROPERTY(QVariant BackClino READ GetBackClino WRITE SetBackClino NOTIFY BackClinoChanged);

public:
    explicit cwShot(QObject *parent = 0);

    cwShot(QVariant distance, QVariant compass, QVariant backCompass, QVariant clino, QVariant backClino, QObject* parent = 0);
    cwShot(const cwShot& shot);

    QVariant GetDistance() const;
    void SetDistance(QVariant distance);

    QVariant GetCompass() const;
    void SetCompass(QVariant compass);

    QVariant GetBackCompass() const;
    void SetBackCompass(QVariant backCompass);

    QVariant GetClino() const;
    void SetClino(QVariant backClino);

    QVariant GetBackClino() const;
    void SetBackClino(QVariant backClino);

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
    QVariant Distance;
    QVariant Compass;
    QVariant BackCompass;
    QVariant Clino;
    QVariant BackClino;
};

inline QVariant cwShot::GetDistance() const {
    return Distance;
}

inline QVariant cwShot::GetCompass() const {
    return Compass;
}

inline QVariant cwShot::GetBackCompass() const {
    return BackCompass;
}

inline QVariant cwShot::GetClino() const {
    return Clino;
}

inline QVariant cwShot::GetBackClino() const {
    return BackClino;
}


#endif // CSHOT_H
