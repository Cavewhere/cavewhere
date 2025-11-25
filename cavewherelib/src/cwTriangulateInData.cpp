/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTriangulateInData.h"
#include "cwTriangulateWarpingData.h"

class cwTriangulatePrivateData : public QSharedData {
public:
    cwTriangulatePrivateData() {}
    cwTriangulatePrivateData(cwTriangulatePrivateData& other) :
        QSharedData(other),
        noteImage(other.noteImage),
        dotPerMeter(other.dotPerMeter),
        outline(other.outline),
        noteTransform(other.noteTransform),
        stations(other.stations),
        leads(other.leads),
        type(other.type),
        lookDirection(other.lookDirection),
        viewMatrix(other.viewMatrix->clone()),
        morphingSettings(other.morphingSettings)
    {

    }

    // Optional: you can allow moves, but they're not typically used on QSharedData:
    cwTriangulatePrivateData(cwTriangulatePrivateData&&) noexcept = default;

    ~cwTriangulatePrivateData() = default;

    // Strongly recommended for QSharedData subclasses:
    cwTriangulatePrivateData& operator=(const cwTriangulatePrivateData&) = delete;
    cwTriangulatePrivateData& operator=(cwTriangulatePrivateData&&) = delete;

    cwImage noteImage;
    double dotPerMeter = 0;
    QPolygonF outline;
    cwNoteTransformationData noteTransform;
    QList<cwNoteStation> noteStations;
    cwStationPositionLookup stationLookup;
    cwSurveyNetwork surveyNetwork;
    QList<cwTriangulateStation> stations;
    QList<cwLead> leads;
    cwScrap::ScrapType type;
    QVector3D lookDirection;
    std::unique_ptr<cwAbstractScrapViewMatrix::Data> viewMatrix;
    cwTriangulateWarpingData morphingSettings;
};

cwTriangulateInData::cwTriangulateInData() :
    data(new cwTriangulatePrivateData())
{
}

cwTriangulateInData::cwTriangulateInData(const cwTriangulateInData &data) :
    data(data.data)
{

}

cwTriangulateInData &cwTriangulateInData::operator=(const cwTriangulateInData &other)
{
    if (this != &other)
        data.operator=(other.data);
    return *this;
}

cwTriangulateInData::~cwTriangulateInData()
{

}

void cwTriangulateInData::setLookDirection(QVector3D eyeVector)
{
    data->lookDirection = eyeVector;
}

QVector3D cwTriangulateInData::lookDirection() const
{
    return data->lookDirection;
}

/**
Get noteImage
*/
cwImage cwTriangulateInData::noteImage() const {
    return data->noteImage;
}

/**
Sets noteImage
*/
void cwTriangulateInData::setNoteImage(cwImage noteImage) {
    data->noteImage = noteImage;
}

/**
Get variableName
*/
QPolygonF cwTriangulateInData::outline() const {
    return data->outline;
}

/**
Sets variableName
*/
void cwTriangulateInData::setOutline(QPolygonF outline) {
    data->outline = outline;
}

/**
  Get variableName
  */
QList<cwTriangulateStation> cwTriangulateInData::stations() const {
    return data->stations;
}

/**
  Sets variableName
  */
void cwTriangulateInData::setStations(QList<cwTriangulateStation> stations) {
    data->stations = stations;
}

QList<cwNoteStation> cwTriangulateInData::noteStations() const
{
    return data->noteStations;
}

void cwTriangulateInData::setNoteStation(const QList<cwNoteStation>& noteStations)
{
    data->noteStations = noteStations;
}

cwStationPositionLookup cwTriangulateInData::stationLookup() const
{
    return data->stationLookup;
}

void cwTriangulateInData::setStationLookup(const cwStationPositionLookup& lookup)
{
    data->stationLookup = lookup;
}

cwSurveyNetwork cwTriangulateInData::surveyNetwork() const
{
    return data->surveyNetwork;
}

void cwTriangulateInData::setSurveyNetwork(const cwSurveyNetwork& network)
{
    data->surveyNetwork = network;
}

/**
  Get variableName
  */
cwNoteTransformationData cwTriangulateInData::noteTransform() const {
    return data->noteTransform;
}

/**
  Sets variableName
  */
void cwTriangulateInData::setNoteTransform(const cwNoteTransformationData& noteTransform) {
    data->noteTransform = noteTransform;
}

/**
 * @brief cwTriangulateInData::noteImageResolution
 * @return Return the resolution of the note image, in DotsPerMeter
 */
double cwTriangulateInData::noteImageResolution() const
{
    return data->dotPerMeter;
}

/**
 * @brief cwTriangulateInData::setNoteImageResolution
 * @param dotsPerMeter - Sets the resolution
 */
void cwTriangulateInData::setNoteImageResolution(double dotsPerMeter)
{
    data->dotPerMeter = dotsPerMeter;
}

cwAbstractScrapViewMatrix::Data* cwTriangulateInData::viewMatrix() const
{
    return data->viewMatrix.get();
}

/**
 * @brief cwTriangulateInData::setType
 * @param type - The type of the scrap, Plan, RunningProfile, ProjectedProfile
 */
void cwTriangulateInData::setViewMatrix(cwAbstractScrapViewMatrix::Data *view)
{
    data->viewMatrix = std::unique_ptr<cwAbstractScrapViewMatrix::Data>(view);
}

/**
 * @brief cwTriangulateInData::leads
 * @return The leads that will be processed
 */
QList<cwLead> cwTriangulateInData::leads() const
{
    return data->leads;
}

/**
 * @brief cwTriangulateInData::setLead
 * @param leads - Returns the leads that will be morphed
 */
void cwTriangulateInData::setLeads(QList<cwLead> leads)
{
    data->leads = leads;
}

cwTriangulateWarpingData cwTriangulateInData::morphingSettings() const
{
    return data->morphingSettings;
}

void cwTriangulateInData::setMorphingSettings(const cwTriangulateWarpingData &morphingSettings)
{
    data->morphingSettings = morphingSettings;
}



