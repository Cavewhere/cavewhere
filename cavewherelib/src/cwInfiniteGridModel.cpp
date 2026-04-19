/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwInfiniteGridModel.h"

//Qt includes
#include <QScopedPropertyUpdateGroup>
#include <cmath>

//Our includes
#include "cwLength.h"

cwInfiniteGridModel::cwInfiniteGridModel(QObject *parent)
    : QObject(parent),
      m_majorGrid(new cwFixedGridModel(this)),
      m_minorGrid(new cwFixedGridModel(this))
{
    m_minorGrid->setObjectName(QStringLiteral("minorGridModel"));
    m_majorGrid->setObjectName(QStringLiteral("majorGridModel"));

    // Interleave grids and labels via Z-order.
    m_majorGrid->setLabelZ(2);
    m_minorGrid->setLabelZ(2);
    m_majorGrid->setLabelBackgroundZ(1);
    m_minorGrid->setLabelBackgroundZ(1);
    m_majorGrid->setGridZ(0);
    m_minorGrid->setGridZ(0);

    // Hide origin labels under the major grid's font metrics. maxLabelSizePixels
    // is in screen pixels; convert to world-meters via labelScale so the mask
    // lives in the same space as the meter-space viewport and label bounds.
    m_originMask.setBinding([this]() {
        QSizeF size = m_majorGrid->maxLabelSizePixels();
        const double labelScale = m_majorGrid->labelScale();
        size.setHeight(size.height() * 0.5 * labelScale);
        size.setWidth(size.width() * 0.25 * labelScale);
        const QRectF viewport = m_viewport.value();
        const QPointF bottomLeft = viewport.bottomLeft();
        return QRectF(QPointF(bottomLeft.x(), bottomLeft.y() - size.height()),
                      size);
    });

    m_majorGrid->bindableLabelMasks().setBinding([this]() {
        return QList<QRectF>({m_originMask});
    });

    m_minorGrid->bindableLabelMasks().setBinding([this]() {
        return m_majorGrid->labelMasks();
    });

    m_majorGrid->bindableViewport().setBinding([this]() {
        return m_viewport.value();
    });
    m_minorGrid->bindableViewport().setBinding([this]() {
        return m_viewport.value();
    });

    m_majorGrid->bindableLabelColor().setBinding([this]() {
        return m_labelColor.value();
    });

    auto clampLightness = [](double factor) -> int {
        return std::max(100, std::min(200, (int)(100.0 + factor * 100.0)));
    };

    m_minorGrid->bindableLabelColor().setBinding([this, clampLightness]() {
        return m_labelColor.value().lighter(clampLightness(m_minorColorLightness));
    });

    m_majorGrid->bindableLineColor().setBinding([this]() {
        return m_lineColor.value();
    });
    m_minorGrid->bindableLineColor().setBinding([this, clampLightness]() {
        return m_lineColor.value().lighter(clampLightness(m_minorColorLightness));
    });

    m_majorGrid->bindableLabelFont().setBinding([this]() {
        return m_labelFont.value();
    });
    m_minorGrid->bindableLabelFont().setBinding([this]() {
        auto font = m_labelFont.value();
        font.setPointSizeF(font.pointSizeF() * m_labelMinorScale.value());
        return font;
    });

    m_majorGrid->bindableLabelBackgroundMargin().setBinding([this]() {
        return m_labelBackgroundMargin.value();
    });
    m_minorGrid->bindableLabelBackgroundMargin().setBinding([this]() {
        return m_labelBackgroundMargin.value();
    });

    m_majorGrid->bindableLabelBackgroundColor().setBinding([this]() {
        return m_labelBackgroundColor.value();
    });
    m_minorGrid->bindableLabelBackgroundColor().setBinding([this]() {
        return m_labelBackgroundColor.value();
    });

    // labelScale = world-units (meters) per screen pixel = viewScale / mapScale.
    // Grid positions are in meters and label bounds come from fontMetrics in
    // screen pixels; scaleBounds() in cwFixedGridModel multiplies by labelScale
    // to convert the screen-pixel bounds into the same meter-space as positions.
    auto labelScaleBinding = [this]() {
        const double mapScale = m_mapMatrix.value()(0, 0);
        return mapScale > 0.0 ? m_viewScale.value() / mapScale : m_viewScale.value();
    };
    m_majorGrid->bindableLabelScale().setBinding(labelScaleBinding);
    m_minorGrid->bindableLabelScale().setBinding(labelScaleBinding);

    m_majorGrid->bindableMapMatrix().setBinding([this]() {
        return m_mapMatrix.value();
    });
    m_minorGrid->bindableMapMatrix().setBinding([this]() {
        return m_mapMatrix.value();
    });

    m_majorGrid->bindableLineWidth().setBinding([this]() {
        const double lineWidth = m_lineWidth.value();
        return std::max(lineWidth, lineWidth * m_viewScale.value());
    });

    m_minorGrid->bindableLineWidth().setBinding([this]() {
        auto lerp = [](double start, double end, double t) {
            return start + t * (end - start);
        };

        constexpr double minorLineWidthScale = 0.5;
        const double viewScale = m_viewScale.value();

        const double minorLineWidth = m_lineWidth.value() * minorLineWidthScale;
        const double minorLineWidthWithScale = minorLineWidth * viewScale;
        const double majorLineWidth = m_majorGrid->lineWidth();

        // Fade from major → minor width once we cross zoom level 1.0.
        if (m_zoomLevel >= 1.0) {
            const double transition = std::min(1.0, m_zoomTransition * 2.0);
            return lerp(majorLineWidth, minorLineWidthWithScale, transition);
        } else {
            return minorLineWidth;
        }
    });

    m_minorGrid->bindableLabelVisible().setBinding([this]() {
        const double minorGridPixels = m_minorGrid->gridIntervalPixels();
        const double margin = m_labelMinorMargin.value() * 2.0; // pixels
        return minorGridPixels > m_minorGrid->maxLabelSizePixels().width() + margin;
    });

    auto interval = [](double base, double zoomLevel) {
        return base * std::pow(10, zoomLevel);
    };

    m_majorZoomGridInterval.setBinding([this, interval]() {
        return interval(m_majorGridInterval.value(), m_clampedZoomLevel.value());
    });

    m_minorZoomGridInterval.setBinding([this, interval]() {
        return interval(m_minorGridInterval.value(), m_clampedZoomLevel.value());
    });

    auto updateMajorInterval = [this]() {
        m_majorGrid->gridInterval()->setValue(m_majorZoomGridInterval.value());
        m_minorGrid->setHiddenInterval(m_majorZoomGridInterval.value());
    };
    m_majorZoomGridIntervalNotifier = m_majorZoomGridInterval.addNotifier(updateMajorInterval);
    updateMajorInterval();

    auto updateMinorInterval = [this]() {
        m_minorGrid->gridInterval()->setValue(m_minorZoomGridInterval.value());
    };
    m_minorZoomGridIntervalNotifier = m_minorZoomGridInterval.addNotifier(updateMinorInterval);
    updateMinorInterval();

    m_zoomLevel.setBinding([this]() -> double {
        const double mapScale = m_mapMatrix.value()(0, 0);
        const double zoomScale = m_viewScale / mapScale;
        const double minPixelScale = m_minorGridMinPixelInterval / 10.0;
        constexpr double offset = 2.0; // Empirically tuned.
        return std::log10(zoomScale * minPixelScale) + offset;
    });

    m_zoomTransition.setBinding([this]() -> double {
        return std::fmod(m_zoomLevel, 1.0);
    });
}

int cwInfiniteGridModel::clampZoomLevel() const
{
    return std::max(0, std::min((int)m_zoomLevel.value(), m_maxZoomLevel.value()));
}

cwFixedGridModel *cwInfiniteGridModel::majorGridModel() const
{
    return m_majorGrid;
}

cwFixedGridModel *cwInfiniteGridModel::minorGridModel() const
{
    return m_minorGrid;
}

cwGridTextModel *cwInfiniteGridModel::majorTextModel() const
{
    return m_majorGrid->textModel();
}

cwGridTextModel *cwInfiniteGridModel::minorTextModel() const
{
    return m_minorGrid->textModel();
}
