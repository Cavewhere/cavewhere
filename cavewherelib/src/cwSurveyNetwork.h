// cwSurveyNetwork.h


#ifndef CWSURVEYNETWORK_H
#define CWSURVEYNETWORK_H

// Qt
#include <QSharedDataPointer>
#include <QHash>
#include <QStringList>
#include <QVector3D>

// Our
#include "cwGlobals.h"

class cwSurveyNetworkData;

class CAVEWHERE_LIB_EXPORT cwSurveyNetwork
{
public:
    cwSurveyNetwork();
    cwSurveyNetwork(const cwSurveyNetwork& other);
    cwSurveyNetwork& operator=(const cwSurveyNetwork& other);
    ~cwSurveyNetwork();

    void clear();
    void addShot(const QString& from, const QString& to);

    QStringList neighbors(const QString& stationName) const;
    QStringList stations() const;
    bool isEmpty() const;

    void setPosition(const QString& stationName, const QVector3D& stationPosition);
    QVector3D position(const QString& stationName) const;
    bool hasPosition(const QString& stationName) const;

    static QStringList changedStations(const cwSurveyNetwork& n1,
                                       const cwSurveyNetwork& n2);

    bool operator==(const cwSurveyNetwork& other) const;
    bool operator!=(const cwSurveyNetwork& other) const { return !operator==(other); }

private:
    QSharedDataPointer<cwSurveyNetworkData> m_data;
};

#endif // CWSURVEYNETWORK_H
