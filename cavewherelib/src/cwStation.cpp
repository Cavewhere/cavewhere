/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStation.h"
#include "cwStationValidator.h"
#include "cwError.h"

//Qt includes
#include <QDebug>

cwStation::cwStation() :
    m_data(new PrivateData())

{ }

cwStation::cwStation(const cwStation &station) :
    m_data(station.m_data)
{
}

cwStation::cwStation(QString name) :
    m_data(new PrivateData())
{
    setName(name);
}

cwStation::PrivateData::PrivateData() :
    m_id(QUuid::createUuid())
{

}

cwStation::PrivateData::PrivateData(QString name) :
    m_id(QUuid::createUuid()),
    m_name(name)
{

}

/**
  \brief Sets the data in the station
  */
void cwStation::setData(QVariant data, DataRoles role) {
    switch(role) {
    case NameRole:
        setName(data.toString());
        break;
    case LeftRole:
        setLeft(data.toString());
        break;
    case RightRole:
        setRight(data.toString());
        break;
    case UpRole:
        setUp(data.toString());
        break;
    case DownRole:
        setDown(data.toString());
        break;
    }
}

/**
  \brief Gets the data in the station
  */
QVariant cwStation::data(DataRoles role) const {
    switch(role) {
    case NameRole:
        return name();
    case LeftRole:
        return left().value();
    case RightRole:
        return right().value();
    case UpRole:
        return up().value();
    case DownRole:
        return down().value();
    }
    return QVariant();
}


/**
 * @brief cwStation::nameIsValid
 * @param stationName
 * @return Returns true if the station name is valid, and false if it isn't
 *
 * This function is useful for writing importers.
 */
bool cwStation::nameIsValid(QString stationName)
{
    cwStationValidator validator;
    return QValidator::Acceptable == validator.validate(stationName);
}

QString cwStation::caseDistinctKey(const QStringList& surveyLevels, const QString& stationName)
{
    QString key;
    for (const QString& level : surveyLevels) {
        key += canonicalKey(level) + QLatin1Char('.');
    }
    return key + stationName;
}

QString cwStation::keyIn(const QString& name, const QSet<QString>& keys)
{
    return keys.contains(name) ? name : canonicalKey(name);
}

QString cwStation::spellingIn(const QString& name, const QStringList& spellings)
{
    if (spellings.contains(name)) {
        return name;
    }

    QString match;
    int matchCount = 0;
    for (const QString& spelling : spellings) {
        if (spelling.compare(name, Qt::CaseInsensitive) == 0) {
            match = spelling;
            ++matchCount;
        }
    }
    return matchCount == 1 ? match : QString();
}

/**
 * @brief cwStation::setName
 * @param name
 * @return True if the name was set, and false if it wasn't.
 *
 * Make sure the nameIsValid() before set the name of the station.
 */
bool cwStation::setName(QString name) {
    if(nameIsValid(name)) {
        m_data->m_name = name;
        return true;
    }
    return false;
}


QSet<int> cwStation::ignoredWarnings() const
{
    return m_data->m_ignoredWarnings;
}

void cwStation::setIgnoredWarnings(const QSet<int>& ids)
{
    m_data->m_ignoredWarnings = ids;
}

bool cwStation::isWarningIgnored(cwErrorTypeId id) const
{
    return m_data->m_ignoredWarnings.contains(static_cast<int>(id));
}
