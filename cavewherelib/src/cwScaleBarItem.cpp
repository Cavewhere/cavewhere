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
#include "cwUnits.h"

#include <array>
#include <algorithm>
#include <cmath>

namespace {
    constexpr std::array<double, 7> kScaleLengthsMeters = {1.0, 5.0, 10.0, 50.0, 100.0, 500.0, 1000.0};
    constexpr double kMinWidthInches = 0.75;
    constexpr double kPaddingInches = 0.25;
    constexpr double kLabelSpacingInches = 0.05;
    constexpr double kBarHeightInches = 0.1;
    constexpr double kPointsPerInch = 72.0;
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

    auto paperWidthFromMeters = [this](double meters) {
        double paperMeters = m_scaleRatio * meters;
        return cwUnits::convert(paperMeters, cwUnits::Meters, cwUnits::Inches);
    };

    double selectedMeters = -1.0;
    double selectedWidth = 0.0;
    double fallbackMeters = kScaleLengthsMeters.front();
    double fallbackWidth = 0.0;
    bool hasWidthCandidate = false;

    for(double candidate : kScaleLengthsMeters) {
        double candidateWidth = paperWidthFromMeters(candidate);
        if(candidateWidth > availableWidth) {
            break;
        }

        hasWidthCandidate = true;
        fallbackMeters = candidate;
        fallbackWidth = candidateWidth;

        if(candidateWidth >= kMinWidthInches) {
            selectedMeters = candidate;
            selectedWidth = candidateWidth;
            break;
        }
    }

    if(!hasWidthCandidate) {
        hideScale();
        return;
    }

    if(selectedMeters < 0.0) {
        selectedMeters = fallbackMeters;
        selectedWidth = fallbackWidth;
    }

    if(selectedWidth <= 0.0) {
        hideScale();
        return;
    }

    QString labelText;
    if(qFuzzyCompare(selectedMeters, 1000.0)) {
        labelText = tr("1 km");
    } else {
        labelText = tr("%1 m").arg(QString::number(static_cast<int>(selectedMeters)));
    }

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
