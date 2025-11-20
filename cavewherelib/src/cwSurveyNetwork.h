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
    cwSurveyNetwork() = default;
    cwSurveyNetwork(const cwSurveyNetwork& other) = default;
    cwSurveyNetwork& operator=(const cwSurveyNetwork& other) = default;
    ~cwSurveyNetwork() = default;

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
    // single struct holding both adjacency and optional position
    struct StationData {
        QStringList neighborList;
        QVector3D stationPosition;
        bool positionIsSet;

        StationData() : neighborList(), stationPosition(), positionIsSet(false) {}

        bool operator==(const StationData& other) const {
            return neighborList == other.neighborList
                   && positionIsSet == other.positionIsSet
                   && stationPosition == other.stationPosition;
        }

        bool operator!=(const StationData& other) const { return !operator==(other); }
    };

    QHash<QString, StationData> stationDataMap;
};

#endif // CWSURVEYNETWORK_H
