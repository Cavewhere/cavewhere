/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTriangulateInData.h"

class PrivateData : public QSharedData {
public:
    PrivateData() : Type(cwScrap::Plan) {}

    cwImage NoteImage;
    double DotPerMeter;
    QPolygonF Outline;
    cwNoteTranformation NoteTransform;
    QList<cwTriangulateStation> Stations;
    QList<cwLead> Leads;
    cwScrap::ScrapType Type;
    QVector3D LookDirection;
};

cwTriangulateInData::cwTriangulateInData() :
    Data(new PrivateData())
{
}

void cwTriangulateInData::setLookDirection(QVector3D eyeVector)
{
    Data->LookDirection = eyeVector;
}

QVector3D cwTriangulateInData::lookDirection() const
{
    return Data->LookDirection;
}

/**
Get NoteImage
*/
cwImage cwTriangulateInData::noteImage() const {
    return Data->NoteImage;
}

/**
Sets NoteImage
*/
void cwTriangulateInData::setNoteImage(cwImage noteImage) {
    Data->NoteImage = noteImage;
}

/**
Get variableName
*/
QPolygonF cwTriangulateInData::outline() const {
    return Data->Outline;
}

/**
Sets variableName
*/
void cwTriangulateInData::setOutline(QPolygonF outline) {
    Data->Outline = outline;
}

/**
  Get variableName
  */
QList<cwTriangulateStation> cwTriangulateInData::stations() const {
    return Data->Stations;
}

/**
  Sets variableName
  */
void cwTriangulateInData::setStations(QList<cwTriangulateStation> stations) {
    Data->Stations = stations;
}

/**
  Get variableName
  */
cwNoteTranformation cwTriangulateInData::noteTransform() const {
    return Data->NoteTransform;
}

/**
  Sets variableName
  */
void cwTriangulateInData::setNoteTransform(cwNoteTranformation noteTransform) {
    Data->NoteTransform = noteTransform;
}

/**
 * @brief cwTriangulateInData::noteImageResolution
 * @return Return the resolution of the note image, in DotsPerMeter
 */
double cwTriangulateInData::noteImageResolution() const
{
    return Data->DotPerMeter;
}

/**
 * @brief cwTriangulateInData::setNoteImageResolution
 * @param dotsPerMeter - Sets the resolution
 */
void cwTriangulateInData::setNoteImageResolution(double dotsPerMeter)
{
    Data->DotPerMeter = dotsPerMeter;
}

/**
 * @brief cwTriangulateInData::type
 * @return The type of the scrap
 */
cwScrap::ScrapType cwTriangulateInData::type() const
{
    return Data->Type;
}

/**
 * @brief cwTriangulateInData::setType
 * @param type - The type of the scrap, Plan or RunningProfile
 */
void cwTriangulateInData::setType(cwScrap::ScrapType type)
{
    Data->Type = type;
}

/**
 * @brief cwTriangulateInData::leads
 * @return The leads that will be processed
 */
QList<cwLead> cwTriangulateInData::leads() const
{
    return Data->Leads;
}

/**
 * @brief cwTriangulateInData::setLead
 * @param leads - Returns the leads that will be morphed
 */
void cwTriangulateInData::setLeads(QList<cwLead> leads)
{
    Data->Leads = leads;
}




