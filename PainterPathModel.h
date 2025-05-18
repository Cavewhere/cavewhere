#ifndef PAINTERPATHMODEL_H
#define PAINTERPATHMODEL_H

//Our includes
#include "PenLineModel.h"
#include "CaveWhereSketchLibExport.h"
#include "AbstractPainterPathModel.h"

//Qt includes
#include <QAbstractListModel>
#include <QPainterPath>
#include <QPointer>
#include <QObjectBindableProperty>

namespace cwSketch {

/**
 * @brief Model converting PenLineModel data into QPainterPath batches for efficient QML rendering.
 *
 * PainterPathModel transforms pen line points into QPainterPath objects,
 * grouping finished paths by stroke width into m_finishedPaths and
 * maintaining the currently drawn path in m_activePath.
 *
 * In QML, an Instantiator watches this model and creates Shape elements,
 * reducing QML object count and batching geometry for GPU performance.
 */
class CAVEWHERE_SKETCH_LIB_EXPORT PainterPathModel : public AbstractPainterPathModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QAbstractItemModel* penLineModel READ penLineModel WRITE setPenLineModel NOTIFY penLineModelChanged)
    Q_PROPERTY(int activeLineIndex READ activeLineIndex WRITE setActiveLineIndex NOTIFY activeLineIndexChanged BINDABLE bindableActiveLineIndex)


public:
    explicit PainterPathModel(QObject *parent = nullptr);

    enum Roles {
        PainterPathRole = Qt::UserRole + 1,
        StrokeWidthRole,
    };

    //There's the active which is 0, and
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    //Implemented in AbstractPainterPathModel
    // QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    // QHash<int, QByteArray> roleNames() const override;

    QAbstractItemModel* penLineModel() const;
    void setPenLineModel(QAbstractItemModel* penLineModel);

    int activeLineIndex() const { return m_activeLineIndex.value(); }
    void setActiveLineIndex(const int& activeLineIndex) { m_activeLineIndex = activeLineIndex; }
    QBindable<int> bindableActiveLineIndex() { return &m_activeLineIndex; }

    Q_INVOKABLE QPainterPath debugPainterPath() const {
        QPainterPath debugPath;
        debugPath.addRect(10, 10, 50, 50);
        return debugPath;
    }

protected:
    const Path &path(const QModelIndex &index) const override;


signals:
    void penLineModelChanged();
    void activeLineIndexChanged();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(PainterPathModel, int, m_activeLineIndex, -1, &PainterPathModel::activeLineIndexChanged);

    struct ActivePath : Path {
        //Catch vectors of the top and bottom part of the polygon line, these aren't used
        //if the lineWidth isn't -1
        QVector<QPointF> beginCap;
        QVector<QPointF> topLine;
        QVector<QPointF> bottomLine;
    };

    struct VariableWidthLine {
        QVector<QPointF> beginCap;
        QVector<QPointF> topLine;
        QVector<QPointF> bottomLine; //doesn't include the end cap
        QPolygonF polygon; //includes the end cap
    };

    ActivePath m_activePath; //At index 0
    QList<Path> m_finishedPaths;
    QPointer<QAbstractItemModel> m_penLineModel;
    int m_previousActivePath = -1;

    static const int m_finishLineIndexOffset = 1;

    //Free stoke pen
    double m_maxHalfWidth = 3.0; //This is half the width the line
    double m_minHalfWidth = 0.25;
    double m_widthScale = 1.5;
    int m_endPointTessellation = 5; //should be greater than 3
    int m_smoothingPressureWindow = 5;
    double pressureToLineHalfWidth(const PenPoint& point) const;


    void addPointToActivePath(const PenPoint& newPoint);
    void addLinePolygon(QPainterPath& path, int modelRow);

    QLineF perpendicularLineAt(const QVector<PenPoint> &points, int index) const;

    void updateActivePath();
    void updateActivePathStrokeWidth();

    void rebuildFinalPath();

    QList<PainterPathModel::Path>::iterator indexOfFinalPaths(double strokeWidth);

    void addOrUpdateFinishPath(int penLineIndex);

    //For polygon generation
    QVector<QPointF> capFromNormal(const QPointF& normal, const QLineF& perpendicularLine, double radius) const;
    QVector<QPointF> cap(const QPointF& firstPoint, const QPointF& secondPoint, const QLineF& perpendicularLine) const;
    QVector<QPointF> endCap(const QVector<PenPoint>& points, const QLineF& perpendicularLine) const;

    QVector<PenPoint> smoothPressure(const QVector<PenPoint>& points, int begin, int end) const;
};
};

#endif // PAINTERPATHMODEL_H
