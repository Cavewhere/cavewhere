/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTriangulateInData.h"

class cwTriangulatePrivateData : public QSharedData {
public:
    cwTriangulatePrivateData() {}
    cwTriangulatePrivateData(cwTriangulatePrivateData& other) :
        QSharedData(other),
        NoteImage(other.NoteImage),
        DotPerMeter(other.DotPerMeter),
        Outline(other.Outline),
        NoteTransform(other.NoteTransform),
        Stations(other.Stations),
        Leads(other.Leads),
        Type(other.Type),
        LookDirection(other.LookDirection),
        ViewMatrix(other.ViewMatrix->clone())
    {

    }

    cwImage NoteImage;
    double DotPerMeter = 0;
    QPolygonF Outline;
    cwNoteTransformationData NoteTransform;
    QList<cwTriangulateStation> Stations;
    QList<cwLead> Leads;
    cwScrap::ScrapType Type;
    QVector3D LookDirection;
    std::unique_ptr<cwAbstractScrapViewMatrix::Data> ViewMatrix;
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
    data->LookDirection = eyeVector;
}

QVector3D cwTriangulateInData::lookDirection() const
{
    return data->LookDirection;
}

/**
Get NoteImage
*/
cwImage cwTriangulateInData::noteImage() const {
    return data->NoteImage;
}

/**
Sets NoteImage
*/
void cwTriangulateInData::setNoteImage(cwImage noteImage) {
    data->NoteImage = noteImage;
}

/**
Get variableName
*/
QPolygonF cwTriangulateInData::outline() const {
    return data->Outline;
}

/**
Sets variableName
*/
void cwTriangulateInData::setOutline(QPolygonF outline) {
    data->Outline = outline;
}

/**
  Get variableName
  */
QList<cwTriangulateStation> cwTriangulateInData::stations() const {
    return data->Stations;
}

/**
  Sets variableName
  */
void cwTriangulateInData::setStations(QList<cwTriangulateStation> stations) {
    data->Stations = stations;
}

/**
  Get variableName
  */
cwNoteTransformationData cwTriangulateInData::noteTransform() const {
    return data->NoteTransform;
}

/**
  Sets variableName
  */
void cwTriangulateInData::setNoteTransform(const cwNoteTransformationData& noteTransform) {
    data->NoteTransform = noteTransform;
}

/**
 * @brief cwTriangulateInData::noteImageResolution
 * @return Return the resolution of the note image, in DotsPerMeter
 */
double cwTriangulateInData::noteImageResolution() const
{
    return data->DotPerMeter;
}

/**
 * @brief cwTriangulateInData::setNoteImageResolution
 * @param dotsPerMeter - Sets the resolution
 */
void cwTriangulateInData::setNoteImageResolution(double dotsPerMeter)
{
    data->DotPerMeter = dotsPerMeter;
}

cwAbstractScrapViewMatrix::Data* cwTriangulateInData::viewMatrix() const
{
    return data->ViewMatrix.get();
}

/**
 * @brief cwTriangulateInData::setType
 * @param type - The type of the scrap, Plan, RunningProfile, ProjectedProfile
 */
void cwTriangulateInData::setViewMatrix(cwAbstractScrapViewMatrix::Data *view)
{
    data->ViewMatrix = std::unique_ptr<cwAbstractScrapViewMatrix::Data>(view);
}

/**
 * @brief cwTriangulateInData::leads
 * @return The leads that will be processed
 */
QList<cwLead> cwTriangulateInData::leads() const
{
    return data->Leads;
}

/**
 * @brief cwTriangulateInData::setLead
 * @param leads - Returns the leads that will be morphed
 */
void cwTriangulateInData::setLeads(QList<cwLead> leads)
{
    data->Leads = leads;
}




