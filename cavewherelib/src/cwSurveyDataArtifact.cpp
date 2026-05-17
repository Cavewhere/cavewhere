#include "cwSurveyDataArtifact.h"
#include "cwCavingRegion.h"
#include "cwSurveyChunkSignaler.h"
#include "cwSurveyChunk.h"
#include "cwTeam.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwFixStationModel.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwError.h"
#include "cwSurvexExporterUtils.h"

cwSurveyDataArtifact::cwSurveyDataArtifact(QObject *parent)
    : cwArtifact(parent)
    , m_region(nullptr)
    , m_surveySignaler(new cwSurveyChunkSignaler(this))
{
    setName("CaveWhere Survey Data");

    // Connect signals from the survey signaler directly to the dataChanged() signal.
    m_surveySignaler->addConnectionToCaves(SIGNAL(insertedTrips(int,int)), this, SIGNAL(surveyDataChanged()));
    m_surveySignaler->addConnectionToCaves(SIGNAL(removedTrips(int,int)), this, SIGNAL(surveyDataChanged()));
    m_surveySignaler->addConnectionToCaves(SIGNAL(nameChanged()), this, SIGNAL(surveyDataChanged()));

    m_surveySignaler->addConnectionToTrips(SIGNAL(chunksInserted(int,int)), this, SIGNAL(surveyDataChanged()));
    m_surveySignaler->addConnectionToTrips(SIGNAL(chunksRemoved(int,int)), this, SIGNAL(surveyDataChanged()));
    m_surveySignaler->addConnectionToTrips(SIGNAL(nameChanged()), this, SIGNAL(surveyDataChanged()));
    m_surveySignaler->addConnectionToTripCalibrations(SIGNAL(calibrationsChanged()), this, SIGNAL(surveyDataChanged()));

    m_surveySignaler->addConnectionToChunks(SIGNAL(shotsAdded(int,int)), this, SIGNAL(surveyDataChanged()));
    m_surveySignaler->addConnectionToChunks(SIGNAL(shotsRemoved(int,int)), this, SIGNAL(surveyDataChanged()));
    m_surveySignaler->addConnectionToChunks(SIGNAL(stationsAdded(int,int)), this, SIGNAL(surveyDataChanged()));
    m_surveySignaler->addConnectionToChunks(SIGNAL(stationsRemoved(int,int)), this, SIGNAL(surveyDataChanged()));
    m_surveySignaler->addConnectionToChunks(SIGNAL(dataChanged(cwSurveyChunk::DataRole,int)), this, SIGNAL(surveyDataChanged()));
}

cwSurveyDataArtifact::~cwSurveyDataArtifact()
{
    // m_surveySignaler is a child of this object so will be cleaned up automatically.
}

cwCavingRegion* cwSurveyDataArtifact::region() const
{
    return m_region;
}

void cwSurveyDataArtifact::setRegion(cwCavingRegion* region)
{
    if (m_region != region) {
        // Disconnect old region signals if any.
        if (m_region) {
            disconnect(m_region, SIGNAL(insertedCaves(int,int)), this, SIGNAL(surveyDataChanged()));
            disconnect(m_region, SIGNAL(removedCaves(int,int)), this, SIGNAL(surveyDataChanged()));
        }
        m_region = region;
        if (m_region) {
            // Connect region signals directly to the surveyDataChanged() signal.
            auto addOrRemoveCave = [this](int, int) { emit surveyDataChanged(); };
            connect(m_region, &cwCavingRegion::insertedCaves, this, addOrRemoveCave);
            connect(m_region, &cwCavingRegion::removedCaves, this, addOrRemoveCave);
        }
        emit regionChanged(m_region);

        // Let the survey signaler know about the new region.
        m_surveySignaler->setRegion(m_region);
    }
}

cwSurveyDataArtifact::SurveyChunk::SurveyChunk(const cwSurveyChunk *chunk) {
    stations = chunk->stations();

    int shotCount = chunk->stationCount() - 1;
    for (int i = 0; i < shotCount; ++i) {
        shots.append(chunk->shot(i));
    }
}

cwSurveyDataArtifact::Trip::Trip(const cwTrip *trip) {
    name = trip->name();
    date = trip->date().date();
    // Copy team members directly.
    teamMembers = trip->team()->teamMembers();
    // Assume trip->calibrations() returns a pointer to cwTripCalibration.
    calibration = trip->calibrations()->data();
    // Copy each survey chunk.
    const auto tripChunks = trip->chunks();
    chunks.reserve(tripChunks.size());
    for (const auto& chunk : tripChunks) {
        chunks.append(SurveyChunk(chunk));
    }
}

cwSurveyDataArtifact::Cave::Cave(const cwCave *cave) {
    name = cave->name();
    int tripCount = cave->tripCount();
    trips.reserve(tripCount);
    for (int i = 0; i < tripCount; ++i) {
        trips.append(Trip(cave->trip(i)));
    }

    // Validate fixes on the main thread, before the snapshot is moved into
    // the export worker, so cwError reaches the cave's errorModel without
    // a worker→GUI thread hop.
    QSet<QString> stationNamesLower;
    for (const Trip& trip : std::as_const(trips)) {
        for (const SurveyChunk& chunk : trip.chunks) {
            for (const cwStation& station : chunk.stations) {
                if (station.isValid()) {
                    stationNamesLower.insert(cwStation::canonicalKey(station.name()));
                }
            }
        }
    }

    QStringList fixErrors;
    fixStations = cwSurvexExporterUtils::validateFixStations(
        cave->fixStations()->fixStations(), stationNamesLower, fixErrors);

    cwErrorListModel* errors = cave->errorModel()->errors();
    for (const QString& message : std::as_const(fixErrors)) {
        errors->append(cwError(message, cwError::Warning));
    }
}

cwSurveyDataArtifact::Region::Region(const cwCavingRegion *region) {
    globalCoordinateSystem = region->globalCoordinateSystem();
    int caveCount = region->caveCount();
    caves.reserve(caveCount);
    for (int i = 0; i < caveCount; ++i) {
        caves.append(Cave(region->cave(i)));
    }
}
