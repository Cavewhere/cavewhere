#ifndef CWSURVEYDATAARTIFACT_H
#define CWSURVEYDATAARTIFACT_H

#include "cwArtifact.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QObject>
#include <QQmlEngine>

// Forward declarations for types assumed to exist.
class cwCavingRegion;
class cwSurveyChunkSignaler;
class cwTeam;
class cwCave;
class cwTrip;
class cwTripCalibration;
class cwTeam;
class cwSurveyChunk;
#include "cwTripCalibration.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwTeamMember.h"

/**
 * @brief The cwSurveyDataArtifact class extends cwArtifact to support survey data.
 *
 * It exposes a cwCavingRegion* as a Q_PROPERTY and sets up a cwSurveyChunkSignaler
 * to connect various survey-related signals directly to the dataChanged() signal.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyDataArtifact : public cwArtifact
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyDataArtifact)

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)

public:
    struct SurveyChunk {
        QVector<cwStation> stations;
        QVector<cwShot> shots;
        QMap<int, cwTripCalibrationData> calibrations;

        SurveyChunk() = default;

        // Constructs a safe snapshot from a cwSurveyChunk pointer.
        explicit SurveyChunk(const cwSurveyChunk* chunk);
    };

    struct Trip {
        QString name;
        QDate date;
        QVector<cwTeamMember> teamMembers;
        cwTripCalibrationData calibration;
        QVector<SurveyChunk> chunks;

        Trip() = default;

        // Constructs a safe snapshot from a cwTrip pointer.
        explicit Trip(const cwTrip* trip);
    };

    struct Cave {
        QString name;
        QVector<Trip> trips;

        Cave() = default;

        // Constructs a safe snapshot from a cwCave pointer.
        explicit Cave(const cwCave* cave);
    };

    struct Region {
        QVector<Cave> caves;

        Region() = default;

        // Constructs a safe snapshot from a cwCavingRegion pointer.
        explicit Region(const cwCavingRegion* region);
    };


    explicit cwSurveyDataArtifact(QObject *parent = nullptr);
    virtual ~cwSurveyDataArtifact();

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

signals:
    void regionChanged(cwCavingRegion* region);
    void surveyDataChanged();

private:
    cwCavingRegion* m_region;
    cwSurveyChunkSignaler* m_surveySignaler;
};

#endif // CWSURVEYDATAARTIFACT_H
