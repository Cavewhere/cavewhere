#pragma once

// Our includes
#include "AbstractPainterPathModel.h"
#include "cwLength.h"

// Qt includes
#include <QRectF>
#include <QColor>
#include <QFont>
#include <QObjectBindableProperty>
#include <QQmlEngine>
#include <QMatrix4x4>

namespace cwSketch {

class InfiniteGridModel : public AbstractPainterPathModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool gridVisible READ gridVisible WRITE setGridVisible NOTIFY gridVisibleChanged BINDABLE bindableGridVisible)
    Q_PROPERTY(double lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged BINDABLE bindableLineWidth)
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY lineColorChanged BINDABLE bindableLineColor)
    Q_PROPERTY(cwLength* gridInterval READ gridInterval CONSTANT)

    Q_PROPERTY(QMatrix4x4 mapMatrix READ mapMatrix WRITE setMapMatrix NOTIFY mapMatrixChanged BINDABLE bindableMapMatrix)
    Q_PROPERTY(QRectF viewport READ viewport WRITE setViewport NOTIFY viewportChanged BINDABLE bindableViewport)
    Q_PROPERTY(QPointF origin READ origin WRITE setOrigin NOTIFY originChanged BINDABLE bindableOrigin)


    Q_PROPERTY(bool labelVisible READ labelVisible WRITE setLabelVisible NOTIFY labelVisibleChanged BINDABLE bindableLabelVisible)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor NOTIFY labelColorChanged BINDABLE bindableLabelColor)
    Q_PROPERTY(QFont labelFont READ labelFont WRITE setLabelFont NOTIFY labelFontChanged BINDABLE bindableLabelFont)
    Q_PROPERTY(double labelScale READ labelScale WRITE setLabelScale NOTIFY labelScaleChanged BINDABLE bindableLabelScale)

public:
    explicit InfiniteGridModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    // Grid properties
    bool gridVisible() const { return m_gridVisible; }
    void setGridVisible(bool visible) { m_gridVisible = visible; }
    QBindable<bool> bindableGridVisible() { return &m_gridVisible; }

    double lineWidth() const { return m_lineWidth; }
    void setLineWidth(double width) { m_lineWidth = width; }
    QBindable<double> bindableLineWidth() { return &m_lineWidth; }

    QColor lineColor() const { return m_lineColor; }
    void setLineColor(const QColor& color) { m_lineColor = color; }
    QBindable<QColor> bindableLineColor() { return &m_lineColor; }

    cwLength* gridInterval() const { return m_gridInterval; }

    QMatrix4x4 mapMatrix() const { return m_mapMatrix; }
    void setMapMatrix(const QMatrix4x4& mapMatrix) { m_mapMatrix = mapMatrix; }
    QBindable<double> bindableMapMatrix() { return &m_mapMatrix; }

    QRectF viewport() const { return m_viewport; }
    void setViewport(const QRectF& rect) { m_viewport = rect; }
    QBindable<QRectF> bindableViewport() { return &m_viewport; }

    //In map pixel where (0, 0) is in world coordinates
    QPointF origin() const { return m_origin; }
    void setOrigin(const QPointF& origin) { m_origin = origin; }
    QBindable<QPointF> bindableOrigin() { return &m_origin; }

    // Label properties
    bool labelVisible() const { return m_labelVisible; }
    void setLabelVisible(bool visible) { m_labelVisible = visible; }
    QBindable<bool> bindableLabelVisible() { return &m_labelVisible; }

    QColor labelColor() const { return m_labelColor; }
    void setLabelColor(const QColor& color) { m_labelColor = color; }
    QBindable<QColor> bindableLabelColor() { return &m_labelColor; }

    QFont labelFont() const { return m_labelFont; }
    void setLabelFont(const QFont& font) { m_labelFont = font; }
    QBindable<QFont> bindableLabelFont() { return &m_labelFont; }

    double labelScale() const { return m_labelScale; }
    void setLabelScale(double labelScale) { m_labelScale = labelScale; }
    QBindable<double> bindableLabelScale() { return &m_labelScale; }


signals:
    void gridVisibleChanged();
    void lineWidthChanged();
    void lineColorChanged();
    void mapMatrixChanged();
    void viewportChanged();
    void originChanged();
    void labelVisibleChanged();
    void labelColorChanged();
    void labelFontChanged();
    void labelScaleChanged();

protected:
    Path path(const QModelIndex& index) const override;

private:
    void updatePaths();

    // storage
    cwLength* m_gridInterval;

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, bool, m_gridVisible, true, &InfiniteGridModel::gridVisibleChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, double, m_lineWidth, 1.0, &InfiniteGridModel::lineWidthChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, QColor, m_lineColor, QColor(0xCC, 0xCC, 0xCC), &InfiniteGridModel::lineColorChanged)

    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QMatrix4x4, m_mapMatrix, &InfiniteGridModel::mapMatrixChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, QRectF, m_viewport, QRectF(), &InfiniteGridModel::viewportChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, QPointF, m_origin, QPointF(), &InfiniteGridModel::originChanged);

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, bool, m_labelVisible, true, &InfiniteGridModel::labelVisibleChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, QColor, m_labelColor, QColor(0x88, 0x88, 0x88), &InfiniteGridModel::labelColorChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, QFont, m_labelFont, QFont(), &InfiniteGridModel::labelFontChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, double, m_labelScale, 1.0, &InfiniteGridModel::labelScaleChanged);

    struct GridLine {
        double position; //Position on the map
        double value; //The cave distance label
    };

    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, int, m_size); //Number of paths in the model, will 0, 1, or 2
    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QVector<GridLine>, m_xGridLines);
    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QVector<GridLine>, m_yGridLines);
    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QPainterPath, m_gridPath);
    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QPainterPath, m_labelsPath);
    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QFont, m_scaledFont);

    QPropertyNotifier m_gridVisibleNotifier;
    QPropertyNotifier m_labelVisibleNotifier;

    //dataChanged updates
    QPropertyNotifier m_lineWidthNotifier;
    QPropertyNotifier m_lineColorNotifier;
    QPropertyNotifier m_labelColorNotifier;
    QPropertyNotifier m_labelFontNotifier;
    QPropertyNotifier m_gridPathNotifier;
    QPropertyNotifier m_labelsPathNotifier;

    QPropertyNotifier m_fixedScaleFontNotifier;


};

} // namespace cwSketch


