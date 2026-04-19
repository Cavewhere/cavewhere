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
#include <QUndoStack>
#include <QUuid>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenStroke.h"
#include "cwSketchData.h"

class cwScale;
class cwKeywordModel;
class cwPenStrokeModel;

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

    QString iconImagePath() const { return m_iconImagePath; }
    void setIconImagePath(const QString &path);

    const QVector<cwPenStroke> &strokes() const { return m_strokes; }
    void setStrokes(const QVector<cwPenStroke> &strokes);

    cwPenStrokeModel *strokeModel() const { return m_strokeModel; }
    QUndoStack       *undoStack() const { return m_undoStack; }
    cwKeywordModel   *keywordModel() const { return m_keywordModel; }

    Q_INVOKABLE int  beginStroke(cwPenStroke::Kind kind, double width, const QColor &color = QColor());

    // Contract: appendPoint is for the live pen input stream only — one
    // active row at a time, between beginStroke() and endStroke(). The
    // per-frame coalescer uses a single-row sentinel, so calling this across
    // different strokeIndex values in one event-loop iteration would drop
    // dataChanged() emits on all but the first row. Bulk programmatic inserts
    // (point symbols, fill areas, etc.) should go through a future bulk API
    // that batches its own begin/endInsertRows, not through appendPoint.
    Q_INVOKABLE void appendPoint(int strokeIndex, const cwPenPoint &p);
    Q_INVOKABLE void endStroke();
    Q_INVOKABLE void clearStrokes();

    cwSketchData data() const;
    void setData(const cwSketchData &d);

signals:
    void nameChanged();
    void viewTypeChanged();
    void iconImagePathChanged();
    void strokesReset();

private:
    QVector<cwPenStroke> m_strokes;
    QVector<cwPenStroke> m_startStrokes;

    QString  m_name;
    QUuid    m_id;
    ViewType m_viewType = Plan;
    cwScale *m_mapScale = nullptr;
    QString  m_iconImagePath;

    cwPenStrokeModel *m_strokeModel = nullptr;
    QUndoStack       *m_undoStack   = nullptr;
    cwKeywordModel   *m_keywordModel = nullptr;

    // -1 sentinel doubles as the "no flush scheduled" flag; see Decision 2
    // in the sketch feature plan for why coalescing matters.
    int m_pendingDirtyRow = -1;

    void applyStrokes(const QVector<cwPenStroke> &strokes);
};

#endif // CWSKETCH_H
