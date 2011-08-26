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

    QString distance() const;
    void setDistance(QString distance);

    QString compass() const;
    void setCompass(QString compass);

    QString backCompass() const;
    void setBackCompass(QString backCompass);

    QString clino() const;
    void setClino(QString backClino);

    QString backClino() const;
    void setbackClino(QString backClino);

    void setParentChunk(cwSurveyChunk* parentChunk);
    cwSurveyChunk* parentChunk() const;

    cwStationReference toStation() const;
    cwStationReference fromStation() const;

    bool isForesightOnly();

private:
    QString Distance;
    QString Compass;
    QString BackCompass;
    QString Clino;
    QString BackClino;
    cwSurveyChunk* ParentChunk;
};

inline QString cwShot::distance() const {
    return Distance;
}

inline QString cwShot::compass() const {
    return Compass;
}

inline QString cwShot::backCompass() const {
    return BackCompass;
}

inline QString cwShot::clino() const {
    return Clino;
}

inline QString cwShot::backClino() const {
    return BackClino;
}


#endif // CSHOT_H
