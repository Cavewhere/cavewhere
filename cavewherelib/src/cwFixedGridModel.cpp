/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixedGridModel.h"

//Our includes
#include "cwLength.h"

//Qt includes
#include <QFontMetricsF>

cwFixedGridModel::cwFixedGridModel(QObject *parent) :
    QObject(parent),
    m_gridInterval(new cwLength(10.0, cwUnits::Meters, this)),
    m_textModel(new cwGridTextModel(this))
{
    // Any property feeding paths() republishes the whole list on change; the
    // canvas coalesces the repaint, so per-property granularity isn't needed.
    auto notifyPaths = [this](auto &property) {
        return property.addNotifier([this]() { emit pathsChanged(); });
    };

    m_gridVisibleNotifier = notifyPaths(m_gridVisible);
    m_labelVisibleNotifier = notifyPaths(m_labelVisible);

    connect(m_gridInterval, &cwLength::valueChanged, this, [this]() {
        m_gridIntervalLength.notify();
    });

    connect(m_gridInterval, &cwLength::unitChanged, this, [this]() {
        m_gridIntervalUnit.notify();
    });

    auto gridLines = [this](double left, double right) {
        QVector<GridLine> positions;

        if (!m_gridVisible.value()) {
            return positions;
        }

        const double interval = cwUnits::convert(m_gridIntervalLength.value(),
                                                 (cwUnits::LengthUnit)m_gridIntervalUnit.value(),
                                                 cwUnits::Meters);
        const double ignoreInterval = cwUnits::convert(m_hiddenInterval.value(),
                                                       (cwUnits::LengthUnit)m_gridIntervalUnit.value(),
                                                       cwUnits::Meters);

        if (interval <= 0.0) {
            return positions;
        }

        double leftMeter  = left;
        double rightMeter = right;

        const double flipped = leftMeter > rightMeter ? -1.0 : 1.0;

        leftMeter  *= flipped;
        rightMeter *= flipped;

        const int leftCount  = leftMeter / interval;
        const int rightCount = rightMeter / interval;

        Q_ASSERT(leftMeter <= rightMeter);

        const int count = rightCount - leftCount + 1;
        positions.reserve(count);

        leftMeter = leftCount * interval;

        for (int i = 0; i < count; ++i) {
            const double positionInMeters = (leftMeter + i * interval) * flipped;
            const bool keep = ignoreInterval > 0.0
                                  ? std::abs(std::fmod(positionInMeters, ignoreInterval)) > 0.0001
                                  : true;

            if (keep) {
                positions.append(GridLine{
                    positionInMeters, // in cave units (meters)
                    positionInMeters  // cave-unit label value
                });
            }
        }

        return positions;
    };

    m_xGridLines.setBinding([this, gridLines]() {
        const QRectF viewport = m_viewport.value();
        return gridLines(viewport.left(), viewport.right());
    });

    // Screen-pixel width of one grid interval: meters × (screen-px/meter) × zoom.
    // labelScale = world-units-per-screen-pixel, so 1/labelScale = screen-px/world-unit.
    m_gridIntervalPixels.setBinding([this]() {
        const double interval = cwUnits::convert(m_gridIntervalLength.value(),
                                                 (cwUnits::LengthUnit)m_gridIntervalUnit.value(),
                                                 cwUnits::Meters);
        const double labelScale = m_labelScale.value();
        return labelScale > 0.0 ? interval / labelScale : 0.0;
    });

    m_yGridLines.setBinding([this, gridLines]() {
        const QRectF viewport = m_viewport.value();
        return gridLines(viewport.top(), viewport.bottom());
    });

    m_gridPath.setBinding([this]() {
        QPainterPath painterPath;
        if (m_gridVisible.value()) {
            const auto viewport = m_viewport.value();
            const auto xGridLines = m_xGridLines.value();
            for (const GridLine &xValue : xGridLines) {
                painterPath.moveTo(xValue.position, viewport.top());
                painterPath.lineTo(xValue.position, viewport.bottom());
            }

            const auto yGridLines = m_yGridLines.value();
            for (const GridLine &yValue : yGridLines) {
                painterPath.moveTo(viewport.left(),  yValue.position);
                painterPath.lineTo(viewport.right(), yValue.position);
            }
        }
        return painterPath;
    });

    auto extraWidth = [](QRectF bounds) {
        bounds.setWidth(bounds.width() + 1);
        return bounds;
    };

    auto scaleBounds = [this](QRectF bounds) {
        const double labelScale = m_labelScale.value();
        bounds.setWidth(bounds.width() * labelScale);
        bounds.setHeight(bounds.height() * labelScale);
        return bounds;
    };

    auto textBounds = [extraWidth, scaleBounds](const QString &text, const QFontMetricsF &fontMetrics) {
        return scaleBounds(extraWidth(fontMetrics.boundingRect(text)));
    };

    auto toString = [this](double num) {
        if (num == 0.0) {
            return QString::number(num) + m_gridInterval->unitName(m_gridInterval->unit());
        } else {
            return QString::number(num);
        }
    };

    m_gridLabels.setBinding([this, textBounds, toString]() {
        QVector<GridLabel> labels;

        if (m_labelVisible.value()) {
            const auto viewport = m_viewport.value();
            const auto xGridLines = m_xGridLines.value();
            const auto yGridLines = m_yGridLines.value();

            const QFont font = m_labelFont.value();
            const QFontMetricsF fontMetrics(font);

            labels.reserve(xGridLines.size() + yGridLines.size());

            const auto maskRects = m_labelMasks.value();
            auto isHidden = [maskRects](const QRectF &bounds) {
                for (const auto &rect : maskRects) {
                    if (bounds.intersects(rect)) {
                        return true;
                    }
                }
                return false;
            };

            auto appendLabel = [&labels, isHidden](const GridLabel &label) {
                if (!isHidden(label.bounds)) {
                    labels.append(label);
                }
            };

            // Callers are expected to pass a Y-up world viewport (sketch's
            // mapMatrix has a Y-flip), so viewport.top() — the smaller-Y edge
            // in QRectF terms — maps to the bottom of the screen, matching
            // the prototype's "labels along the bottom" convention.
            for (const GridLine &xValue : xGridLines) {
                const QString text = toString(xValue.value);
                QRectF bounds = textBounds(text, fontMetrics);
                bounds.moveTo(xValue.position - bounds.width() * 0.5,
                              viewport.top());
                appendLabel({bounds, text});
            }

            for (const GridLine &yValue : yGridLines) {
                const QString text = toString(yValue.value);
                QRectF bounds = textBounds(text, fontMetrics);
                bounds.moveTo(viewport.left(),
                              yValue.position - bounds.height() * 0.5);
                appendLabel({bounds, text});
            }
        }

        return labels;
    });

    m_maxLabelSizePixels.setBinding([this, toString]() {
        const QFont font = m_labelFont.value();
        QFontMetricsF fontMetrics(font);
        const double gridInterval = m_gridIntervalLength.value();
        const double squared = gridInterval * gridInterval * 10 * 9;
        const auto bounds = fontMetrics.boundingRect(toString(-squared));
        return bounds.size();
    });

    m_labelBackgroundPath.setBinding([this]() {
        QPainterPath painterPath;

        const auto gridLabels = m_gridLabels.value();

        const double marginSize   = m_labelBackgroundMargin.value() * m_labelScale.value();
        const double marginSize2x = 2.0 * marginSize;
        const QPointF margin(marginSize, marginSize);

        for (const GridLabel &label : gridLabels) {
            auto bounds = label.bounds;
            bounds.moveTopLeft(bounds.topLeft() - margin);
            bounds.setSize(QSizeF(bounds.width() + marginSize2x, bounds.height() + marginSize2x));
            painterPath.addRect(bounds);
        }

        return painterPath;
    });

    m_lineWidthNotifier = notifyPaths(m_lineWidth);
    m_lineColorNotifier = notifyPaths(m_lineColor);
    m_labelColorNotifier = notifyPaths(m_labelColor);
    m_gridPathNotifier = notifyPaths(m_gridPath);
    m_labelsBackgroundNotifier = notifyPaths(m_labelBackgroundPath);

    m_textRows.setBinding([this]() {
        const auto font = m_labelFont.value();
        const auto fillColor = m_labelColor.value();

        const auto labels = m_gridLabels.value();

        QVector<cwGridTextModel::TextRow> rows;
        rows.reserve(labels.size());

        for (const auto &label : labels) {
            cwGridTextModel::TextRow row;
            row.text        = label.text;
            // Hand the full bounds to the painter; it maps through the
            // world-to-screen transform and derives the baseline anchor,
            // which is the only way to stay correct whether the caller's
            // world is Y-up (sketch with mapMatrix Y-flip) or Y-down.
            row.bounds      = label.bounds;
            row.position    = label.bounds.topLeft();
            row.font        = font;
            row.fillColor   = fillColor;
            row.strokeColor = Qt::transparent;
            rows.append(row);
        }

        return rows;
    });

    m_gridLabelsNotifier = m_textRows.addNotifier([this]() {
        m_textModel->replaceRows(m_textRows);
    });
}

double cwFixedGridModel::gridIntervalLength() const
{
    return m_gridInterval->value();
}

int cwFixedGridModel::gridIntervalUnit() const
{
    return m_gridInterval->unit();
}

QList<cwSketchPathSource::Path> cwFixedGridModel::paths() const
{
    QList<Path> result;

    if (!m_gridVisible.value()) {
        return result;
    }

    result.append(Path{
        m_gridPath,
        m_lineColor,
        m_lineWidth,
        m_gridZ,
    });

    if (m_labelVisible.value()) {
        result.append(Path{
            m_labelBackgroundPath,
            m_labelBackgroundColor,
            -1.0, // No line width — fill pass.
            m_labelBackgroundZ
        });
    }

    return result;
}
