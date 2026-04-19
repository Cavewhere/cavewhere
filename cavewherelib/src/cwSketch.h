/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCH_H
#define CWSKETCH_H

//Qt includes
#include <QByteArray>
#include <QObject>
#include <QQmlEngine>
#include <QUndoStack>
#include <QUuid>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenStroke.h"
#include "cwSketchData.h"
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
    Q_PROPERTY(QByteArray iconImage READ iconImage WRITE setIconImage NOTIFY iconImageChanged)
    Q_PROPERTY(cwPenStrokeModel* strokeModel READ strokeModel CONSTANT)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack CONSTANT)
    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)
    Q_PROPERTY(cwSurveyNetworkArtifact* surveyNetworkArtifact READ surveyNetworkArtifact WRITE setSurveyNetworkArtifact NOTIFY surveyNetworkArtifactChanged)
    Q_PROPERTY(cwSurvey2DGeometryArtifact* survey2DGeometry READ survey2DGeometry CONSTANT)

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

    QByteArray iconImage() const { return m_iconImage; }
    void setIconImage(const QByteArray &image);

    // Materialises m_iconImage to a cache file on first call and returns the
    // path. Empty if no icon is set.
    QString iconImagePath() const;

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
    void iconImageChanged();
    void strokesReset();
    void surveyNetworkArtifactChanged();

private:
    QVector<cwPenStroke> m_strokes;
    QVector<cwPenStroke> m_startStrokes;

    QString  m_name;
    QUuid    m_id;
    ViewType m_viewType = Plan;
    cwScale *m_mapScale = nullptr;
    QByteArray m_iconImage;
    mutable QString m_cachedIconPath;

    cwPenStrokeModel *m_strokeModel = nullptr;
    QUndoStack       *m_undoStack   = nullptr;
    cwKeywordModel   *m_keywordModel = nullptr;

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
