#pragma once

// Our includes
#include "AbstractPainterPathModel.h"
#include "cwLength.h"

// Qt includes
#include <QRectF>
#include <QColor>
#include <QFont>
#include <QObjectBindableProperty>
#include <array>

namespace cwSketch {

class InfiniteGridModel : public AbstractPainterPathModel
{
    Q_OBJECT

    Q_PROPERTY(bool gridVisible READ gridVisible WRITE setGridVisible NOTIFY gridVisibleChanged BINDABLE bindableGridVisible)
    Q_PROPERTY(double lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged BINDABLE bindableLineWidth)
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY lineColorChanged BINDABLE bindableLineColor)
    Q_PROPERTY(cwLength* gridInterval READ gridInterval CONSTANT)

    Q_PROPERTY(double mapScale READ mapScale WRITE setMapScale NOTIFY mapScaleChanged BINDABLE bindableMapScale)
    Q_PROPERTY(QRectF viewport READ viewport WRITE setViewport NOTIFY viewportChanged BINDABLE bindableViewport)

    Q_PROPERTY(bool labelVisible READ labelVisible WRITE setLabelVisible NOTIFY labelVisibleChanged BINDABLE bindableLabelVisible)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor NOTIFY labelColorChanged BINDABLE bindableLabelColor)
    Q_PROPERTY(QFont labelFont READ labelFont WRITE setLabelFont NOTIFY labelFontChanged BINDABLE bindableLabelFont)

public:
    explicit InfiniteGridModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override { Q_UNUSED(parent); return 2; }

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

    double mapScale() const { return m_mapScale; }
    void setMapScale(double scale) { m_mapScale = scale; }
    QBindable<double> bindableMapScale() { return &m_mapScale; }

    QRectF viewport() const { return m_viewport; }
    void setViewport(const QRectF& rect) { m_viewport = rect; }
    QBindable<QRectF> bindableViewport() { return &m_viewport; }

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

signals:
    void gridVisibleChanged(bool);
    void lineWidthChanged(double);
    void lineColorChanged(const QColor&);
    void mapScaleChanged(double);
    void viewportChanged(const QRectF&);
    void labelVisibleChanged(bool);
    void labelColorChanged(const QColor&);
    void labelFontChanged(const QFont&);

protected:
    Path path(const QModelIndex& index) const override;

private:
    void updatePaths();

    // storage
    cwLength* m_gridInterval;

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, bool, m_gridVisible, true, &InfiniteGridModel::gridVisibleChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, double, m_lineWidth, 1.0, &InfiniteGridModel::lineWidthChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, QColor, m_lineColor, QColor(0xCC, 0xCC, 0xCC), &InfiniteGridModel::lineColorChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, double, m_mapScale, 50.0, &InfiniteGridModel::mapScaleChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, QRectF, m_viewport, QRectF(), &InfiniteGridModel::viewportChanged)

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, bool, m_labelVisible, true, &InfiniteGridModel::labelVisibleChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, QColor, m_labelColor, QColor(0x88, 0x88, 0x88), &InfiniteGridModel::labelColorChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InfiniteGridModel, QFont, m_labelFont, QFont(), &InfiniteGridModel::labelFontChanged)

    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, int, m_size); //Number of paths in the model, will 0, 1, or 2
    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QVector<double>, m_xGridLines);
    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QVector<double>, m_yGridLines);
    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QPainterPath, m_gridPath);
    Q_OBJECT_BINDABLE_PROPERTY(InfiniteGridModel, QPainterPath, m_labelsPath);
};

} // namespace cwSketch


