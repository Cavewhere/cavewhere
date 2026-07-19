/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QPainter>
#include <QFontMetricsF>

//Our includes
#include "cwScaleBarItem.h"
#include "cwScaleBarSelector.h"
#include "cwUnits.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    constexpr double kMinWidthInches = 0.75;
    constexpr double kPaddingInches = 0.25;
    constexpr double kLabelSpacingInches = 0.05;
    constexpr double kBarHeightInches = 0.1;
    constexpr double kPointsPerInch = cwUnits::PointsPerInch;
}

cwScaleBarItem::cwScaleBarItem(QGraphicsItem *parent) :
    QGraphicsObject(parent),
    m_scaleRatio(0.0),
    m_visible(false),
    m_labelPointSize(12.0)
{
    setZValue(1000.0);
    setVisible(m_visible);
    m_labelFont.setPointSizeF(m_labelPointSize);
}

void cwScaleBarItem::setBorderRect(const QRectF &rect)
{
    if(m_borderRect == rect) {
        return;
    }
    m_borderRect = rect;
    updateLayout();
}

void cwScaleBarItem::setScaleRatio(double ratio)
{
    if(qFuzzyCompare(1.0 + m_scaleRatio, 1.0 + ratio)) {
        return;
    }

    m_scaleRatio = ratio;
    updateLayout();
}

void cwScaleBarItem::setUnitSystem(cwUnits::UnitSystem system)
{
    if(m_unitSystem == system) {
        return;
    }
    m_unitSystem = system;
    updateLayout();
}

void cwScaleBarItem::setLabelPointSize(double pointSize)
{
    if(pointSize <= 0.0 || qFuzzyCompare(m_labelPointSize, pointSize)) {
        return;
    }
    m_labelPointSize = pointSize;
    m_labelFont.setPointSizeF(m_labelPointSize);
    updateLayout();
}

QRectF cwScaleBarItem::boundingRect() const
{
    return m_boundingRect;
}

void cwScaleBarItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if(!m_visible) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::black);
    painter->drawRect(m_barRect);

    painter->setPen(Qt::black);
    painter->setBrush(Qt::NoBrush);

    const QTransform world = painter->worldTransform();

    const qreal sx = std::hypot(world.m11(), world.m21());
    const qreal sy = std::hypot(world.m22(), world.m12());
    const qreal invScaleX = qFuzzyIsNull(sx) ? 1.0 : 1.0 / sx;
    const qreal invScaleY = qFuzzyIsNull(sy) ? 1.0 : 1.0 / sy;

    painter->translate(m_labelRect.topLeft());
    painter->scale(invScaleX, invScaleY);
    painter->setFont(m_labelFont);
    QRectF deviceRect(QPointF(0.0, 0.0), QSizeF(m_labelRect.width() * sx, m_labelRect.height() * sy));
    painter->drawText(deviceRect, Qt::AlignCenter, m_label);

    painter->restore();
}

cwScaleBarItem::ScaleSelection cwScaleBarItem::selectScale(double scaleRatio,
                                                          double availableWidthInches,
                                                          cwUnits::UnitSystem system)
{
    ScaleSelection result;
    if(scaleRatio <= 0.0 || availableWidthInches <= 0.0) {
        return result;
    }

    auto paperWidthFromMeters = [scaleRatio](double meters) {
        return cwUnits::convert(scaleRatio * meters, cwUnits::Meters, cwUnits::Inches);
    };

    const std::vector<cwScaleBarSelector::Candidate>& candidates =
            cwScaleBarSelector::niceCandidates(system);

    const cwScaleBarSelector::Candidate* selected = nullptr;
    const cwScaleBarSelector::Candidate* fallback = nullptr;

    for(const cwScaleBarSelector::Candidate& candidate : candidates) {
        double candidateWidth = paperWidthFromMeters(candidate.meters);
        if(candidateWidth > availableWidthInches) {
            break;
        }

        fallback = &candidate;

        if(candidateWidth >= kMinWidthInches) {
            selected = &candidate;
            break;
        }
    }

    if(fallback == nullptr) {
        return result;
    }

    if(selected == nullptr) {
        selected = fallback;
    }

    const double width = paperWidthFromMeters(selected->meters);
    if(width <= 0.0) {
        return result;
    }

    result.valid = true;
    result.value = selected->value;
    result.unit = selected->unit;
    result.widthInches = width;
    return result;
}

void cwScaleBarItem::updateLayout()
{
    auto hideScale = [this]() {
        if(m_visible) {
            prepareGeometryChange();
            m_boundingRect = QRectF();
            m_barRect = QRectF();
            m_labelRect = QRectF();
            m_label.clear();
            m_visible = false;
        }
        setVisible(m_visible);
    };

    if(!m_borderRect.isValid() || m_scaleRatio <= 0.0) {
        hideScale();
        return;
    }

    double availableWidth = std::max(0.0, m_borderRect.width() - 2.0 * kPaddingInches);
    if(availableWidth <= 0.0) {
        hideScale();
        return;
    }

    const ScaleSelection selection = selectScale(m_scaleRatio, availableWidth, m_unitSystem);
    if(!selection.valid) {
        hideScale();
        return;
    }

    double selectedWidth = selection.widthInches;
    // No space between number and unit ("10m", "2000ft") to match the on-screen
    // view scale bar (ScaleBar.qml).
    QString labelText = QStringLiteral("%1%2")
            .arg(QString::number(selection.value), cwUnits::unitName(selection.unit));

    QFontMetricsF metrics(m_labelFont);
    const double pointSize = m_labelFont.pointSizeF();
    const double targetHeightInches = pointSize > 0.0 ? pointSize / kPointsPerInch : 0.0;
    double metricsHeight = metrics.height();
    double unitToInches = 0.0;
    if(metricsHeight > 0.0 && targetHeightInches > 0.0) {
        unitToInches = targetHeightInches / metricsHeight;
    }

    double labelWidth = metrics.horizontalAdvance(labelText) * unitToInches;
    double labelHeight = targetHeightInches;

    if(labelWidth <= 0.0) {
        labelWidth = std::min(selectedWidth, availableWidth);
    }

    if(availableWidth > 0.0) {
        labelWidth = std::min(labelWidth, availableWidth);
    }

    labelWidth = std::min(labelWidth, selectedWidth);

    labelWidth = std::max(labelWidth, 0.01);
    labelHeight = std::max(labelHeight, 0.01);

    double requiredHeight = kBarHeightInches + kLabelSpacingInches + labelHeight + kPaddingInches;
    if(requiredHeight > m_borderRect.height()) {
        hideScale();
        return;
    }

    double rectRight = m_borderRect.right() - kPaddingInches;
    double rectLeft = rectRight - selectedWidth;
    if(rectLeft < m_borderRect.left() + kPaddingInches) {
        rectLeft = m_borderRect.left() + kPaddingInches;
        // rectRight = rectLeft + selectedWidth;
    }

    double rectBottom = m_borderRect.bottom() - kPaddingInches - labelHeight - kLabelSpacingInches;
    double rectTop = rectBottom - kBarHeightInches;
    if(rectTop < m_borderRect.top() + kPaddingInches) {
        rectTop = m_borderRect.top() + kPaddingInches;
        // rectBottom = rectTop + kBarHeightInches;
    }

    QRectF barRectScene(QPointF(rectLeft, rectTop), QSizeF(selectedWidth, kBarHeightInches));

    double labelLeft = barRectScene.center().x() - labelWidth / 2.0;
    double minLabelLeft = m_borderRect.left() + kPaddingInches;
    double maxLabelLeft = m_borderRect.right() - kPaddingInches - labelWidth;
    labelLeft = std::clamp(labelLeft, minLabelLeft, maxLabelLeft);

    double labelY = barRectScene.bottom() + kLabelSpacingInches;
    double maxLabelY = m_borderRect.bottom() - kPaddingInches - labelHeight;
    labelY = std::min(labelY, maxLabelY);
    QRectF labelRectScene(QPointF(labelLeft, labelY), QSizeF(labelWidth, labelHeight));

    QRectF geometryRect = barRectScene.united(labelRectScene);
    if(!geometryRect.isValid() || geometryRect.isEmpty()) {
        hideScale();
        return;
    }

    prepareGeometryChange();

    setPos(geometryRect.topLeft());
    m_boundingRect = QRectF(QPointF(0.0, 0.0), geometryRect.size());
    m_barRect = QRectF(barRectScene.topLeft() - geometryRect.topLeft(), barRectScene.size());
    m_labelRect = QRectF(labelRectScene.topLeft() - geometryRect.topLeft(), labelRectScene.size());
    m_label = labelText;
    m_visible = true;
    setVisible(m_visible);
}
