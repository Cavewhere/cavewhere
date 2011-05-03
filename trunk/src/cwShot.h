#ifndef CSHOT_H
#define CSHOT_H

//Our includes
class cwSurveyChunk;
class cwStation;
class cwStationReference;

//Qt includes
#include <QVariant>

class cwShot
{

public:
    explicit cwShot();

    cwShot(QString distance,
           QString compass, QString backCompass,
           QString clino, QString backClino);
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

    void setParentChunk(cwSurveyChunk* parentChunk);
    cwSurveyChunk* parentChunk() const;

    cwStationReference toStation() const;
    cwStationReference fromStation() const;

private:
    QString Distance;
    QString Compass;
    QString BackCompass;
    QString Clino;
    QString BackClino;
    cwSurveyChunk* ParentChunk;
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
