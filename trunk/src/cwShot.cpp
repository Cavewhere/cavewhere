#include "cwShot.h"

cwShot::cwShot(QObject *parent) :
    QObject(parent)
{
    Distance = "";
    Compass = "";
    BackCompass = "";
    Clino = "";
    BackClino = "";

}

cwShot::cwShot(QVariant distance,
               QVariant compass,
               QVariant backCompass,
               QVariant clino,
               QVariant backClino,
               QObject* parent) :
    QObject(parent)
{

    Distance = distance;
    Compass = compass;
    BackCompass = backCompass;
    Clino = clino;
    BackClino = backClino;
}

void cwShot::SetDistance(QVariant distance) {
    if(Distance != distance) {
        Distance = distance;
        emit DistanceChanged();
    }
}

void cwShot::SetCompass(QVariant compass) {
    if(Compass != compass) {
        Compass = compass;
        emit CompassChanged();
    }
}

void cwShot::SetBackCompass(QVariant backCompass) {
    if(BackCompass != backCompass) {
        BackCompass = backCompass;
        emit BackCompassChanged();
    }
}

void cwShot::SetClino(QVariant clino) {
    if(Clino != clino) {
        Clino = clino;
        emit ClinoChanged();
    }
}

void cwShot::SetBackClino(QVariant backClino) {
    if(BackClino != backClino) {
        BackClino = backClino;
        emit BackClinoChanged();
    }
}
