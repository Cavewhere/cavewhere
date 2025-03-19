#ifndef CWSURVEXEXPORTERRULE_H
#define CWSURVEXEXPORTERRULE_H

//Qt includes
#include <QPointer>
#include <QMap>
#include <QQmlEngine>

//Our includes
#include "cwAbstractRule.h"
#include "cwFutureFileNameArtifact.h"
#include "cwSurveyDataArtifact.h"
#include "cwFileNameArtifact.h"
#include "cwReadingStates.h"
#include "cwUnits.h"
#include "cwTripCalibration.h"
#include "cwShot.h"
#include "cwTeamMember.h"
class cwTeam;
class cwCave;
class cwTrip;
class cwTripCalibration;
class cwTeam;
class cwSurveyChunk;

class cwSurvexExporterRule : public cwAbstractRule {
    Q_OBJECT
    QML_NAMED_ELEMENT(SurvexExporterRule)

    //Sources
    Q_PROPERTY(cwSurveyDataArtifact* surveyData READ surveyData WRITE setSurveyData NOTIFY surveyDataChanged)
    Q_PROPERTY(cwFileNameArtifact* survexFilename READ survexFileName WRITE setSurvexFileName NOTIFY survexFileNameChanged)

    //Outputs
    Q_PROPERTY(cwFutureFileNameArtifact* survexFileArtifact READ survexFileArtifact CONSTANT)

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

    explicit cwSurvexExporterRule(QObject* parent = nullptr);
    virtual ~cwSurvexExporterRule();

    cwSurveyDataArtifact* surveyData() const;
    void setSurveyData(cwSurveyDataArtifact* surveyData);

    cwFileNameArtifact* survexFileName() const { return m_survexFilename; }
    void setSurvexFileName(cwFileNameArtifact* survexFileName);

    // Provide property names for introspection via the base class
    QList<QByteArray> sourcesNames() const override {
        return {
            "surveyData",
            "survexFilename"
        };
    }
    QList<QByteArray> outputsNames() const override {
        return { "survexFileArtifact" };
    }

    cwFutureFileNameArtifact* survexFileArtifact() const
    {
        return m_survexFileArtifact;
    }

    static Monad::ResultBase writeRegion(QTextStream& stream, const Region& region);
    static Monad::ResultBase writeCave(QTextStream& stream, const Cave& cave);
    static Monad::ResultBase writeTrip(QTextStream& stream, const Trip& trip);

signals:
    void surveyDataChanged();
    void survexFileNameChanged();

private slots:
    void updatePipeline();

private:
    QPointer<cwSurveyDataArtifact> m_surveyData;
    QPointer<cwFileNameArtifact> m_survexFilename;
    cwFutureFileNameArtifact* m_survexFileArtifact;


    static void writeCalibrations(QTextStream& stream, const cwTripCalibrationData& calibrations);
    static void writeCalibration(QTextStream& stream, QString type, double value, double scale = 1.0);

    static void writeLengthUnits(QTextStream &stream, cwUnits::LengthUnit unit);
    static void writeShotData(QTextStream &stream, const Trip trip, int textPadding = -11);
    static void writeLRUDData(QTextStream &stream, const Trip trip, int textPadding = -11);
    static void writeTeamData(QTextStream &stream, const QVector<cwTeamMember>& team);
    static void writeDate(QTextStream &stream, QDate date);
    static QString toSupportedLength(const Trip& trip, double length, cwDistanceStates::State state);
    static QString compassToString(double compass, cwCompassStates::State state);
    static QString clinoToString(double clino, cwClinoStates::State state);
    static Monad::ResultBase writeChunk(QTextStream &stream,
                           bool hasFrontSights,
                           bool hasBackSights,
                           const Trip& trip,
                           const SurveyChunk& chunk,
                           int textPadding = -11);
    static void fixFirstStation(QTextStream &stream, const Cave& cave);
};

#endif // CWSURVEXEXPORTERRULE_H
