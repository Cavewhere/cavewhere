#ifndef CWCOMPASSEXPORTERCAVETASK_H
#define CWCOMPASSEXPORTERCAVETASK_H

//Our includes
#include "cwCaveExporterTask.h"
#include "cwUnits.h"
class cwCave;
class cwTrip;
class cwSurveyChunk;
class cwStationReference;
class cwShot;

class cwCompassExporterCaveTask : public cwCaveExporterTask
{
    Q_OBJECT
public:
    explicit cwCompassExporterCaveTask(QObject *parent = 0);

    void writeCave(QTextStream& stream, cwCave* cave);

signals:

public slots:

private:
    enum StationLRUDField {
        Left,
        Right,
        Up,
        Down
    };

    enum ShotField {
        Compass,
        Clino,
        BackCompass,
        BackClino
    };


    static const char* CompassNewLine;

    void writeTrip(QTextStream& stream, cwTrip* trip);
    void writeHeader(QTextStream& stream, cwTrip* trip);
    void writeData(QTextStream& stream, QString fieldName, int fieldLength, QString data);
    void writeChunk(QTextStream& stream, cwSurveyChunk* chunk);

    float convertField(cwStationReference station, StationLRUDField field, cwUnits::LengthUnit unit);
    float convertField(cwTrip* trip, cwShot* shot, ShotField field);
    QString formatFloat(float value);

    QString convertFromDownUp(QString clinoReading);
//    float fixCompass(cwTrip* trip, QString compass1, QString compass2);
//    float fixClino(cwTrip* trip, QString clino1, QString clino2);
    QString surveyTeam(cwTrip* trip);

};

#endif // CWCOMPASSEXPORTERCAVETASK_H
