/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCH_H
#define CWSKETCH_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUndoStack>
#include <QUuid>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenStroke.h"
#include "cwSketchData.h"
#include "cwSketchViewState.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurveyNetworkArtifact.h"

class cwScale;
class cwKeywordModel;
class cwPenStrokeModel;
class cwAbstractScrapViewMatrix;
class cwMatrix4x4Artifact;
class cwSurvey2DGeometryRule;
class cwTrip;

class CAVEWHERE_LIB_EXPORT cwSketch : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Sketch)

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(ViewType viewType READ viewType WRITE setViewType NOTIFY viewTypeChanged)
    Q_PROPERTY(cwScale* mapScale READ mapScale CONSTANT)
    Q_PROPERTY(QString iconImagePath READ iconImagePath WRITE setIconImagePath NOTIFY iconImagePathChanged)
    Q_PROPERTY(cwPenStrokeModel* strokeModel READ strokeModel CONSTANT)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack CONSTANT)
    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)
    Q_PROPERTY(cwSurveyNetworkArtifact* surveyNetworkArtifact READ surveyNetworkArtifact WRITE setSurveyNetworkArtifact NOTIFY surveyNetworkArtifactChanged)
    Q_PROPERTY(cwSurvey2DGeometryArtifact* survey2DGeometry READ survey2DGeometry CONSTANT)
    Q_PROPERTY(QString anchorStation READ anchorStation WRITE setAnchorStation NOTIFY anchorStationChanged)
    Q_PROPERTY(cwSketchViewState* viewState READ viewState CONSTANT)

public:
    enum ViewType {
        Plan = 0,
        RunningProfile = 1,
        ProjectedProfile = 2
    };
    Q_ENUM(ViewType)

    explicit cwSketch(QObject *parent = nullptr);
    ~cwSketch() override;

    QString name() const { return m_name; }
    void setName(const QString &name);

    QUuid id() const { return m_id; }
    void setId(const QUuid &id);

    ViewType viewType() const { return m_viewType; }
    void setViewType(ViewType type);

    cwScale *mapScale() const { return m_mapScale; }

    // Icon thumbnails are written by cwSketchManager into the shared project
    // disk cache and exposed here as an `image://cwcache/...` URL. The sketch
    // itself does not know how to produce or resolve them.
    QString iconImagePath() const { return m_iconImagePath; }
    void setIconImagePath(const QString &path);

    // Station within parentTrip() that selects *which* connected component of
    // the trip's line plot this sketch renders, AND whose world position is
    // translated to the sketch's (0,0). Empty until the user picks it (or the
    // single-component auto-pick fires). Persisted across loads.
    QString anchorStation() const { return m_anchorStation; }
    void setAnchorStation(const QString &name);

    cwSketchViewState *viewState() const { return m_viewState; }

    const QVector<cwPenStroke> &strokes() const { return m_strokes; }
    void setStrokes(const QVector<cwPenStroke> &strokes);

    cwPenStrokeModel *strokeModel() const { return m_strokeModel; }
    QUndoStack       *undoStack() const { return m_undoStack; }
    cwKeywordModel   *keywordModel() const { return m_keywordModel; }

    // Region-wide network input; typically pointed at
    // cwLinePlotManager::surveyNetworkArtifact(). When it or the view matrix
    // changes, survey2DGeometry() regenerates.
    cwSurveyNetworkArtifact   *surveyNetworkArtifact() const;
    void setSurveyNetworkArtifact(cwSurveyNetworkArtifact *artifact);

    cwSurvey2DGeometryArtifact *survey2DGeometry() const;

    void setParentTrip(cwTrip *trip);
    Q_INVOKABLE cwTrip *parentTrip() const { return m_parentTrip; }

    Q_INVOKABLE int  beginStroke(cwPenStroke::Kind kind, double width, const QColor &color = QColor());

    // Contract: appendPoint is for the live pen input stream only — one
    // active row at a time, between beginStroke() and endStroke(). The
    // per-frame coalescer uses a single-row sentinel, so calling this across
    // different strokeIndex values in one event-loop iteration would drop
    // dataChanged() emits on all but the first row. Bulk programmatic inserts
    // (point symbols, fill areas, etc.) should go through a future bulk API
    // that batches its own begin/endInsertRows, not through appendPoint.
    Q_INVOKABLE void appendPoint(int strokeIndex, const cwPenPoint &p);
    Q_INVOKABLE void appendPoint(int strokeIndex, QPointF position, double pressure, qint64 timestampMs = 0);
    Q_INVOKABLE void endStroke();
    Q_INVOKABLE void clearStrokes();

    cwSketchData data() const;
    void setData(const cwSketchData &d);

signals:
    void nameChanged();
    void viewTypeChanged();
    void iconImagePathChanged();
    void strokesReset();
    void strokeEnded();
    void surveyNetworkArtifactChanged();
    void anchorStationChanged();

private:
    QVector<cwPenStroke> m_strokes;
    QVector<cwPenStroke> m_startStrokes;

    QString  m_name;
    QUuid    m_id;
    ViewType m_viewType = Plan;
    cwScale *m_mapScale = nullptr;
    QString  m_iconImagePath;
    QString  m_anchorStation;

    cwPenStrokeModel  *m_strokeModel = nullptr;
    QUndoStack        *m_undoStack   = nullptr;
    cwKeywordModel    *m_keywordModel = nullptr;
    cwSketchViewState *m_viewState   = nullptr;

    cwAbstractScrapViewMatrix *m_viewMatrix      = nullptr;
    cwMatrix4x4Artifact       *m_matrixArtifact  = nullptr;
    cwSurvey2DGeometryRule    *m_geometryRule    = nullptr;
    cwTrip                    *m_parentTrip      = nullptr;

    // -1 sentinel doubles as the "no flush scheduled" flag; see Decision 2
    // in the sketch feature plan for why coalescing matters.
    int m_pendingDirtyRow = -1;

    void applyStrokes(const QVector<cwPenStroke> &strokes);
    void rebuildViewMatrixForType();
    void syncMatrixArtifact();
};

#endif // CWSKETCH_H
