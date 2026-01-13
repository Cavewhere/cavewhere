/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCRAP_H
#define CWSCRAP_H

//Qt includes
#include <QObject>
#include <QVector2D>
#include <QVector>
#include <QPolygonF>
#include <QQmlEngine>
#include <QMatrix4x4>
#include <QtCore/quuid.h>
#include <QPointer>

//Our includes
#include "cwGlobals.h"
#include "cwNote.h"
#include "cwNoteStation.h"
#include "cwTriangulatedData.h"
#include "cwLead.h"
// #include "cwStation.h"
#include "cwScrapData.h"
#include "cwScrapType.h"
#include "cwNoteTransformationData.h"
#include "cwNoteTransformCalculator.h"
class cwNoteTranformation;
class cwAbstractScrapViewMatrix;
class cwPlanScrapViewMatrix;
class cwRunningProfileScrapViewMatrix;
class cwProjectedProfileScrapViewMatrix;
class cwNote;
class cwCave;
class cwKeywordModel;
class cwScrapEditingScope;

/**
  cwScrap holds a polygon of points that represents a scrap

  Points can be added or removed from the scrap.  All the points will be in
  normalize note coordinates system.
  */
class CAVEWHERE_LIB_EXPORT cwScrap : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Scrap)

    Q_PROPERTY(cwNoteTranformation* noteTransformation READ noteTransformation NOTIFY noteTransformationChanged)
    Q_PROPERTY(bool calculateNoteTransform READ calculateNoteTransform WRITE setCalculateNoteTransform NOTIFY calculateNoteTransformChanged)
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)

    Q_PROPERTY(ScrapType type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(cwAbstractScrapViewMatrix* viewMatrix READ viewMatrix NOTIFY viewMatrixChanged)
    Q_PROPERTY(QStringList types READ types CONSTANT)
    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)

public:
    friend class cwScrapEditingScope;
    enum ScrapType {
        Plan = cwScrapType::Plan,
        RunningProfile = cwScrapType::RunningProfile,
        ProjectedProfile = cwScrapType::ProjectedProfile
    };
    Q_ENUM(ScrapType);

    enum StationDataRole {
        StationName,
        StationPosition
    };
    Q_ENUM(StationDataRole);

    enum LeadDataRole {
        LeadPositionOnNote,
        LeadPosition, //Global in view, calculated in TriangulationData
        LeadDesciption,
        LeadSize,
        LeadUnits,
        LeadSupportedUnits,
        LeadCompleted,
        LeadNumberOfRoles
    };
    Q_ENUM(LeadDataRole)

    // /**
    //  * @brief The ScrapShot class
    //  *
    //  * This is for averaging shot transform.  This class has the scale of the shot and the
    //  * QVector2D of how far off shot drawn on paper compared the shot's data.  The QVector3D
    //  * has no error when it's equal to (0.0, 1.0, 0.0)
    //  */
    // class ScrapShotTransform {
    // public:
    //     ScrapShotTransform() : Scale(0.0), RotationDiff(0.0) { }
    //     ScrapShotTransform(double scale, QVector3D errorVector) : Scale(scale), ErrorVector(errorVector), RotationDiff(0.0) {}
    //     ScrapShotTransform(double scale, QVector3D errorVector, double rotationDiff) : Scale(scale), ErrorVector(errorVector), RotationDiff(rotationDiff) {}

    //     double Scale;
    //     QVector3D ErrorVector;
    //     double RotationDiff;

    //     cwNoteTransformationData toNoteTransform() const;
    // };


    explicit cwScrap(QObject *parent = 0);
    // cwScrap(const cwScrap& other);
    // const cwScrap& operator =(const cwScrap& other);

    void setId(const QUuid& id);
    QUuid id() const { return m_id; }

    void setParentNote(cwNote* trip);
    cwNote* parentNote() const;

    // void setParentCave(cwCave* cave);
    cwCave* parentCave() const;

    cwAbstractScrapViewMatrix* viewMatrix() const;
    ScrapType type() const;
    void setType(ScrapType type);
    QStringList types() const;
    void updateIdKeyword();

    cwKeywordModel* keywordModel() const;

    void addPoint(QPointF point);
    Q_INVOKABLE void insertPoint(int index, QPointF point);
    Q_INVOKABLE void removePoint(int index);
    QVector<QPointF> points() const;
    QPolygonF polygon() const;
    void setPoints(QVector<QPointF> points);
    Q_INVOKABLE void setPoint(int index, QPointF point);
    Q_INVOKABLE int numberOfPoints() const;
    Q_INVOKABLE bool isClosed() const;
    Q_INVOKABLE void close();

    void addStation(cwNoteStation station);
    Q_INVOKABLE void removeStation(int stationId);
    const QList<cwNoteStation>& stations() const;
    void setStations(QList<cwNoteStation> stations);
    cwNoteStation station(int stationId);
    Q_INVOKABLE int numberOfStations() const;
    Q_INVOKABLE QVariant stationData(StationDataRole role, int noteStationIndex) const;
    Q_INVOKABLE void setStationData(StationDataRole role, int noteStationIndex, QVariant value);
    bool hasStation(QString name) const;

    void addLead(cwLead lead);
    Q_INVOKABLE void removeLead(int leadId);
    void setLeads(QList<cwLead> leads);
    QList<cwLead> leads() const;
    Q_INVOKABLE QVariant leadData(LeadDataRole role, int leadIndex) const;
    Q_INVOKABLE void setLeadData(LeadDataRole role, int leadIndex, QVariant value);
    Q_INVOKABLE int numberOfLeads() const;

    cwNoteTranformation* noteTransformation() const;
    cwNoteTransformationData noteTransformAdjustedDeclination() const;

    bool calculateNoteTransform() const;
    void setCalculateNoteTransform(bool calculateNoteTransform);

    bool editing() const;
    Q_INVOKABLE void beginEditing();
    Q_INVOKABLE void endEditing();

    QString guessNeighborStationName(const cwNoteStation& previousStation, QPointF stationNotePosition);

    QMatrix4x4 mapWorldToNoteMatrix(const cwNoteStation &referenceStation) const;

    void setLeadPositions(const QVector<QVector3D>& leadPositions);
    QVector<QVector3D> leadPositions() const { return m_leadPositions; }
    // void setTriangulationData(cwTriangulatedData data);
    // cwTriangulatedData triangulationData() const;

//    static QMatrix4x4 toProfileRotation(QVector3D fromStationPos, QVector3D toStationPos);

    void updateImage();

    void setData(const cwScrapData& data);
    cwScrapData data() const;


public slots:
    void updateNoteTransformation();

signals:
    //For scrap outline
    void insertedPoints(int begin, int end);
    void removedPoints(int begin, int end);
    void pointChanged(int begin, int end);
    void pointsReset();
    void closeChanged();

    //Scrap geometry
    void triangulationDataChange();

    //For stations
    void stationAdded();
    void stationPositionChanged(int begin, int end);
    void stationNameChanged(int noteStationIndex);
    void stationRemoved(int index);
    void stationsReset();

    //For leads
    void leadsBeginInserted(int begin, int end);
    void leadsInserted(int begin, int end);
    void leadsBeginRemoved(int begin, int end);
    void leadsRemoved(int begin, int end);
    void leadsDataChanged(int begin, int end, QList<int> roles);
    void leadsReset();

    void noteTransformationChanged();
    void calculateNoteTransformChanged();
    void viewMatrixChanged();
    void typeChanged();

    void triangulationDataChanged();
    void editingChanged(bool editing);

private:

    /**
     * @brief The ScrapShot class
     *
     * This is for averaging shot transform.  This class has the scale of the shot and the
     * QVector2D of how far off shot drawn on paper compared the shot's data.  The QVector3D
     * has no error when it's equal to (0.0, 1.0, 0.0)
     */
    // class ScrapShotTransform {
    // public:
    //     ScrapShotTransform() : Scale(0.0), RotationDiff(0.0) { }
    //     ScrapShotTransform(double scale, QVector3D errorVector) : Scale(scale), ErrorVector(errorVector), RotationDiff(0.0) {}
    //     ScrapShotTransform(double scale, QVector3D errorVector, double rotationDiff) : Scale(scale), ErrorVector(errorVector), RotationDiff(rotationDiff) {}

    //     double Scale;
    //     QVector3D ErrorVector;
    //     double RotationDiff;

    //     cwNoteTransformationData toNoteTransform() const;
    // };

    // /**
    //  * @brief The ProfileTransform class
    //  *
    //  * This class is used to re-project the x-axis for drawing running profile. Rotation is what
    //  * direction up is, and mirror is left to right, or right to left (mirror on x-axis)
    //  */
    // class ProfileTransform {
    // public:
    //     ProfileTransform() {}
    //     ProfileTransform(QMatrix4x4 rotation, QMatrix4x4 mirror) : Rotation(rotation), Mirror(mirror) {}

    //     QMatrix4x4 Rotation;
    //     QMatrix4x4 Mirror;
    // };

    //The object id
    QUuid m_id;

    //The outline of the scrap, in normalized points
    QPolygonF OutlinePoints;

    //The stations that the scrap owns
    QList<cwNoteStation> Stations;

    //All the leads that are with this scrap
    QList<cwLead> Leads;
    QList<QVector3D> m_leadPositions;

    //The note transform, this is used for guessing the station name's for the user
    cwNoteTranformation* NoteTransformation;
    bool CalculateNoteTransform; //!< If true this will automatically calculate the note transform

    //The type of scrap
    cwAbstractScrapViewMatrix* ViewMatrix;
    cwPlanScrapViewMatrix* CachedPlanViewMatrix = nullptr;
    cwRunningProfileScrapViewMatrix* CachedRunningProfileViewMatrix = nullptr;
    cwProjectedProfileScrapViewMatrix* CachedProjectedProfileViewMatrix = nullptr;


    //The parent trip, this is for referencing the stations
    cwNote* ParentNote;
    // cwCave* ParentCave;

    //For rendering, points in note coordinates
    cwTriangulatedData TriangulationData;
    bool TriangulationDataDirty;

    //For visiblity and object orginization
    cwKeywordModel* KeywordModel = nullptr;
    void updateTypeKeyword();

    //Clamps a pointF that's in note coordinates to the scrap
    QPointF clampToScrap(QPointF point);
    bool pointOnLine(QLineF line, QPointF point);

    //For note station transformation, automatic calculation
    QList< QPair <cwNoteStation, cwNoteStation> > noteShots() const;
    cwNoteTransformCalculator::ProfileTransform profileTransform() const;
    // QList<ScrapShotTransform> calculateShotTransformations(QList< QPair <cwNoteStation, cwNoteStation> > shots,
    //                                                        const ProfileTransform& profileTransform = ProfileTransform()) const;
    // ScrapShotTransform calculateShotTransformation(cwNoteStation station1,
    //                                                cwNoteStation station2,
    //                                                const ProfileTransform& profileTransform) const;

    // cwNoteTransformationData projectedAverageTransform(QList< QPair<cwNoteStation, cwNoteStation> > shotStations) const;
    cwNoteTransformationData runningProfileAverageTransform(QList< QPair<cwNoteStation, cwNoteStation> > shotStations) const;

    const cwScrap& copy(const cwScrap& other);

    QStringList allNeighborStations(const QString& stationName) const;

    void setViewMatrix(cwAbstractScrapViewMatrix* viewMatrix);

    cwNoteTransformationData noteTransformAdjustedDeclination(cwNoteTransformationData transformation) const;

private slots:
//    void updateStationsWithNewCave();

private:
    int m_editingDepth;
};

class CAVEWHERE_LIB_EXPORT cwScrapEditingScope
{
public:
    explicit cwScrapEditingScope(cwScrap* scrap);
    ~cwScrapEditingScope();

private:
    QPointer<cwScrap> m_scrap;
};

Q_DECLARE_METATYPE(cwScrap*)

/**
  \brief Adds a point to the scrap at the end of the scrap line
  */
inline void cwScrap::addPoint(QPointF point) {
    insertPoint(OutlinePoints.size(), point);
}

/**
  \brief Gets all the bound points in the scrappoi
  */
inline QVector<QPointF> cwScrap::points() const {
    return OutlinePoints;
}

/**
 * @brief cwScrap::polygon
 * @return The polygon that repesents the scrap
 *
 * This is the same as the OutlinePoints
 */
inline QPolygonF cwScrap::polygon() const {
    return QPolygonF(OutlinePoints);
}

/**
  \brief Gets number of points in the scrap
  */
inline int cwScrap::numberOfPoints() const {
    return OutlinePoints.size();
}

/**
 * @brief cwScrap::isClosed
 * @return Returns true if the the scrap geometry is closed, ie. the last point
 * equal to first point
 */
inline bool cwScrap::isClosed() const {
    if(numberOfPoints() <= 2) { return false; }
    return polygon().isClosed();
}



/**
  \brief Gets the number of stations for a page of notes
  */
inline int cwScrap::numberOfStations() const {
    return Stations.count();
}

/**
  \brief Gets all the stations that the scrap owns
  */
inline const QList<cwNoteStation>& cwScrap::stations() const {
    return Stations;
}


/**
  Gets the station note transformation,  This is the note page real scale and
  rotation!
  */
inline cwNoteTranformation* cwScrap::noteTransformation() const {
    return NoteTransformation;
}

/**
  Returns the parent trip
  */
inline cwNote* cwScrap::parentNote() const {
    return ParentNote;
}

/**
Gets calculateNoteTransform
*/
inline bool cwScrap::calculateNoteTransform() const {
    return CalculateNoteTransform;
}

/**
    \brief Gets the parent cave
  */
inline cwCave *cwScrap::parentCave() const {
    return ParentNote->parentCave();
    // return ParentCave;
}

inline cwAbstractScrapViewMatrix *cwScrap::viewMatrix() const
{
    return ViewMatrix;
}

/**
  \brief Gets the triangulation data
  */
// inline cwTriangulatedData cwScrap::triangulationData() const {
//     return TriangulationData;
// }

/**
*
*/

/**
* Returns the keywords for the scrap
*/
inline cwKeywordModel* cwScrap::keywordModel() const {
    return KeywordModel;
}


#endif // CWSCRAP_H
