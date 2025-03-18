#ifndef CWSURVEXEXPORTERRULE_H
#define CWSURVEXEXPORTERRULE_H

//Qt includes
#include <QPointer>

//Our includes
#include "cwAbstractRule.h"
#include "cwFutureFileNameArtifact.h"
#include "cwSurveyDataArtifact.h"
#include "cwFileNameArtifact.h"
#include "cwReadingStates.h"
#include "cwUnits.h"
class cwTeam;
class cwCave;
class cwTrip;
class cwTripCalibration;
class cwTeam;
class cwSurveyChunk;

#include <QProperty>

class cwSurvexExporterRule : public cwAbstractRule {
    Q_OBJECT

    //Sources
    Q_PROPERTY(cwSurveyDataArtifact* surveyData READ surveyData WRITE setSurveyData NOTIFY surveyDataChanged)
    Q_PROPERTY(cwFileNameArtifact* survexFile READ survexFile WRITE setSurvexFile NOTIFY survexFileChanged)

    //Outputs
    Q_PROPERTY(cwFutureFileNameArtifact* survexFileArtifact READ survexFileArtifact CONSTANT)

public:
    explicit cwSurvexExporterRule(QObject* parent = nullptr);
    virtual ~cwSurvexExporterRule();

    cwSurveyDataArtifact* surveyData() const;
    void setSurveyData(cwSurveyDataArtifact* surveyData);

    cwFileNameArtifact* survexFileName() const { return m_survexFilename; }
    void setSurvexFileName(cwFileNameArtifact* survexFileName);

    // Provide property names for introspection via the base class
    QList<QByteArray> sourcesNames() const override {
        return {
            "surveyData"
            "survexFilename",
        };
    }
    QList<QByteArray> outputsNames() const override {
        return { "survexFile" };
    }

    cwFutureFileNameArtifact* survexFileArtifact() const
    {
        return m_survexFileArtifact;
    }

    static Monad::ResultBase writeRegion(QTextStream& stream, const cwCavingRegion* region);
    static Monad::ResultBase writeCave(QTextStream& stream, const cwCave* cave);
    static Monad::ResultBase writeTrip(QTextStream& stream, const cwTrip* trip);

signals:
    void surveyDataChanged();
    void survexFileChanged();

private slots:
    void updatePipeline();

private:
    QPointer<cwSurveyDataArtifact> m_surveyData;
    QPointer<cwFileNameArtifact> m_survexFilename;
    cwFutureFileNameArtifact* m_survexFileArtifact;


    static void writeCalibrations(QTextStream& stream, cwTripCalibration* calibrations);
    static void writeCalibration(QTextStream& stream, QString type, double value, double scale = 1.0);

    static void writeLengthUnits(QTextStream &stream, cwUnits::LengthUnit unit);
    static void writeShotData(QTextStream &stream, const cwTrip *trip, int textPadding = -11);
    static void writeLRUDData(QTextStream &stream, const cwTrip *trip, int textPadding = -11);
    static void writeTeamData(QTextStream &stream, const cwTeam *team);
    static void writeDate(QTextStream &stream, QDate date);
    static QString toSupportedLength(const cwTrip* trip, double length, cwDistanceStates::State state);
    static QString compassToString(double compass, cwCompassStates::State state);
    static QString clinoToString(double clino, cwClinoStates::State state);
    static Monad::ResultBase writeChunk(QTextStream &stream,
                           bool hasFrontSights,
                           bool hasBackSights,
                           const cwTrip* trip,
                           const cwSurveyChunk *chunk,
                           int textPadding = -11);
    static void fixFirstStation(QTextStream &stream, const cwCave *cave);
};

#endif // CWSURVEXEXPORTERRULE_H
