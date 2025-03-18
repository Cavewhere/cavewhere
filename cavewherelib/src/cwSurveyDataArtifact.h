#ifndef CWSURVEYDATAARTIFACT_H
#define CWSURVEYDATAARTIFACT_H

#include "cwArtifact.h"
#include <QObject>

// Forward declarations for types assumed to exist.
class cwCavingRegion;
class cwSurveyChunkSignaler;

/**
 * @brief The cwSurveyDataArtifact class extends cwArtifact to support survey data.
 *
 * It exposes a cwCavingRegion* as a Q_PROPERTY and sets up a cwSurveyChunkSignaler
 * to connect various survey-related signals directly to the dataChanged() signal.
 */
class cwSurveyDataArtifact : public cwArtifact
{
    Q_OBJECT
    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)

public:
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
