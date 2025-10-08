/**************************************************************************
**
**    Copyright (C) 2025 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTriangulateLiDARInData.h"

// Qt includes
#include <QSharedData>
#include <QMatrix4x4>

// Our includes
#include "cwNoteLiDARStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"

// ---------------------------------------------------------------------
// Private data
// ---------------------------------------------------------------------
class cwTriangulateLiDARPrivateData : public QSharedData {
public:
    cwTriangulateLiDARPrivateData() = default;

    cwTriangulateLiDARPrivateData(const cwTriangulateLiDARPrivateData& other)
        : QSharedData(other),
        m_noteStations(other.m_noteStations),
        m_stationLookup(other.m_stationLookup),
        m_surveyNetwork(other.m_surveyNetwork),
        m_gltfFilename(other.m_gltfFilename)
    {
    }

    QList<cwNoteLiDARStation> m_noteStations;
    cwStationPositionLookup m_stationLookup;
    cwSurveyNetwork m_surveyNetwork;
    QString m_gltfFilename;
    QMatrix4x4 m_modelMatrix;
};

// ---------------------------------------------------------------------
// Public class
// ---------------------------------------------------------------------

cwTriangulateLiDARInData::cwTriangulateLiDARInData()
    : data(new cwTriangulateLiDARPrivateData())
{
}

cwTriangulateLiDARInData::cwTriangulateLiDARInData(const cwTriangulateLiDARInData& other)
    : data(other.data)
{
}

cwTriangulateLiDARInData& cwTriangulateLiDARInData::operator=(const cwTriangulateLiDARInData& other)
{
    if (this != &other) {
        data = other.data; // QSharedDataPointer handles ref-counting and COW
    }
    return *this;
}

cwTriangulateLiDARInData::~cwTriangulateLiDARInData() = default;

// --------------------
// Accessors / Mutators
// --------------------

QList<cwNoteLiDARStation> cwTriangulateLiDARInData::noteStations() const
{
    return data->m_noteStations;
}

void cwTriangulateLiDARInData::setNoteStations(const QList<cwNoteLiDARStation>& stations)
{
    data->m_noteStations = stations;
}

cwStationPositionLookup cwTriangulateLiDARInData::stationLookup() const
{
    return data->m_stationLookup;
}

void cwTriangulateLiDARInData::setStationLookup(const cwStationPositionLookup& lookup)
{
    data->m_stationLookup = lookup;
}

cwSurveyNetwork cwTriangulateLiDARInData::surveyNetwork() const
{
    return data->m_surveyNetwork;
}

void cwTriangulateLiDARInData::setSurveyNetwork(const cwSurveyNetwork& network)
{
    data->m_surveyNetwork = network;
}

QString cwTriangulateLiDARInData::gltfFilename() const
{
    return data->m_gltfFilename;
}

void cwTriangulateLiDARInData::setGltfFilename(const QString& filename)
{
    data->m_gltfFilename = filename;
}

QMatrix4x4 cwTriangulateLiDARInData::modelMatrix() const
{
    return data->m_modelMatrix;
}

void cwTriangulateLiDARInData::setModelMatrix(const QMatrix4x4 &matrix)
{
    data->m_modelMatrix = matrix;
}
