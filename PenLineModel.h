#ifndef PENLINEMODEL_H
#define PENLINEMODEL_H

//Qt includes
#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QPointF>
#include <QObjectBindableProperty>

//Our includes
#include "CaveWhereSketchLibExport.h"

// A simple structure representing a point with a specific width.
class CAVEWHERE_SKETCH_LIB_EXPORT PenPoint {
    Q_GADGET
    QML_VALUE_TYPE(penPoint)

public:
    PenPoint() = default;
    PenPoint(QPointF position, double pressure) :
        position(position),
        pressure(pressure)
    {
    }

    bool isNull() const {
        return pressure < 0.0;
    }

    QPointF position;
    double pressure = -1.0;
};

// A pen line consisting of multiple pen points.
struct CAVEWHERE_SKETCH_LIB_EXPORT PenLine {
    QVector<PenPoint> points;
    double width = 2.5;
};
Q_DECLARE_METATYPE(PenLine)


class CAVEWHERE_SKETCH_LIB_EXPORT PenLineModel : public QAbstractItemModel {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(double currentStrokeWidth READ currentStrokeWidth WRITE setCurrentStrokeWidth NOTIFY currentStrokeWidthChanged BINDABLE bindableCurrentStrokeWidth)

public:
    enum PenLineRoles {
        LineRole = Qt::UserRole + 1,
        LineWidthRole,
        PenPointRole
    };

    explicit PenLineModel(QObject* parent = nullptr);

    // Reimplemented from QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int columnCount(const QModelIndex &parent) const override { Q_UNUSED(parent); return 0; }

    // Utility methods to add or clear lines.
    Q_INVOKABLE int addNewLine();
    void addLine(const PenLine& line);
    Q_INVOKABLE void addPoint(int lineIndex, PenPoint point);
    void clear();

    Q_INVOKABLE PenPoint penPoint(QPointF point, double width)
    {
        return PenPoint(point, width);
    }

    double currentStrokeWidth() const { return m_currentStrokeWidth.value(); }
    void setCurrentStrokeWidth(const double& currentStrokeWidth) { m_currentStrokeWidth = currentStrokeWidth; }
    QBindable<double> bindableCurrentStrokeWidth() { return &m_currentStrokeWidth; }

signals:
    void currentStrokeWidthChanged();


private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(PenLineModel, double, m_currentStrokeWidth, 2.5, &PenLineModel::currentStrokeWidthChanged);
    QVector<PenLine> m_lines;

};



#endif // PENLINEMODEL_H
