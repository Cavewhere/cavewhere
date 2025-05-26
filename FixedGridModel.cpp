#include "FixedGridModel.h"


cwSketch::FixedGridModel::FixedGridModel(QObject *parent) :
    m_gridInterval(new cwLength(10.0, cwUnits::Meters, this))
{
    m_size.setBinding([this]() {
        if(!m_gridVisible) {
            //Labels are also hidden when the grid lines are hidden
            return 0;
        }

        if(m_labelVisible) {
            return 3;
        }

        return 1;
    });



    auto gridLines = [this](double left, double right, double origin, double scale) {
        QVector<GridLine> positions;

        // nothing to do if grid is hidden or interval/scale invalid
        if (!m_gridVisible.value()) {
            return positions;
        }

        //Interval in cave (meters)
        double interval = cwUnits::convert(m_gridInterval->value(), (cwUnits::LengthUnit)m_gridInterval->unit(), cwUnits::Meters);

        if (interval <= 0.0) {
            return positions;
        }

        //Convert from pixel to meters in cave
        double originMeter = origin / scale;
        double leftMeter = left / scale;
        double rightMeter = right / scale;

        double flipped = leftMeter > rightMeter ? -1.0 : 1.0;

        leftMeter *= flipped;
        rightMeter *= flipped;

        //Clamp to the nearest interval gridline
        int leftCount = leftMeter / interval;
        int rightCount = rightMeter / interval;

        Q_ASSERT(leftMeter <= rightMeter);

        int count = rightCount - leftCount + 1;
        positions.reserve(count);

        //Clamp to the nearest interval gridline
        leftMeter = leftCount * interval;

        // generate: world → pixel
        // qDebug() << "Lines: left(m):" << leftMeter << "right(m):" << right << "scale:" << scale << "count:" << count << "interval:" << interval << "origin:" << origin;
        for(int i = 0; i < count; ++i) {
            double positionInMeters = (leftMeter + i * interval) * flipped;

            const GridLine gridLine {
                positionInMeters * scale, //in map pixels
                positionInMeters //in cave units
            };

            positions.append(gridLine);
            // qDebug() << "Position:" << positions.last().position;
        }

        return positions;
    };

    m_xGridLines.setBinding([this, gridLines]() {
        auto viewport = m_viewport.value();
        auto mapMatrix = m_mapMatrix.value();
        const double xScale = mapMatrix(0,0);
        qDebug() << "xGridLines:" << viewport.left() << viewport.right();
        return gridLines(viewport.left(), viewport.right(), m_origin.value().x(), xScale);
    });

    m_yGridLines.setBinding([this, gridLines]() {
        auto viewport = m_viewport.value();
        auto mapMatrix = m_mapMatrix.value();
        const double yScale = mapMatrix(1,1);
        qDebug() << "yGridLines:";
        return gridLines(viewport.top(), viewport.bottom(), m_origin.value().y(), yScale);
    });

    m_gridPath.setBinding([this]() {
        QPainterPath painterPath;
        if(m_gridVisible.value()) {
            const auto viewport = m_viewport.value();
            const auto xGridLines = m_xGridLines.value();
            for(const GridLine& xValue : xGridLines) {
                painterPath.moveTo(xValue.position, viewport.top());
                painterPath.lineTo(xValue.position, viewport.bottom());
            }

            const auto yGridLines = m_yGridLines.value();
            for(const GridLine& yValue : yGridLines) {
                painterPath.moveTo(viewport.left(), yValue.position);
                painterPath.lineTo(viewport.right(), yValue.position);
            }
        }
        return painterPath;
    });

    m_gridLabels.setBinding([this]() {
        QVector<GridLabel> labels;

        if(m_labelVisible.value()) {

            const auto viewport = m_viewport.value();
            const auto xGridLines = m_xGridLines.value();
            const auto yGridLines = m_yGridLines.value();

            const QFont font = m_scaledFont.value();
            QFontMetricsF fontMetrics(font);

            auto toString = [this](double num) {
                if(num == 0.0) {
                    return QString::number(num) + m_gridInterval->unitName(m_gridInterval->unit());
                } else {
                    return QString::number(num);
                }
            };

            labels.reserve(xGridLines.size() + yGridLines.size());

            const double zeroHeight = fontMetrics.boundingRect('0').height(); //height() function doesn't give good results, use boundingbox instead

            for(const GridLine& xValue: xGridLines) {
                const QString text = toString(xValue.value);
                QRectF bounds = fontMetrics.boundingRect(text);
                bounds.moveTo(xValue.position - bounds.center().x(), viewport.bottom() - bounds.height());
                bounds.setHeight(zeroHeight);
                labels.append(GridLabel{
                    bounds,
                    text
                });
            }

            for(const GridLine& yValue : yGridLines) {
                const QString text = toString(yValue.value);
                QRectF bounds = fontMetrics.boundingRect(text);
                bounds.moveTo(viewport.left(), yValue.position + bounds.center().y());
                bounds.setHeight(zeroHeight);

                labels.append(GridLabel{
                    bounds,
                    text
                });
            }
        }

        return labels;
    });

    m_labelsPath.setBinding([this]() {
        QPainterPath painterPath;

        const auto gridLabels = m_gridLabels.value();
        const auto font = m_scaledFont.value();

        for(const GridLabel& label : gridLabels) {
            painterPath.addText(label.bounds.bottomLeft(), font, label.text);
        }

        return painterPath;
    });

    m_labelBackgroundPath.setBinding([this]() {
        QPainterPath painterPath;

        const auto gridLabels = m_gridLabels.value();

        double marginSize = m_labelBackgroundMargin.value() * m_labelScale.value();
        double marginSize2x = 2.0 * marginSize;
        QPointF margin(marginSize, marginSize);

        for(const GridLabel& label : gridLabels) {
            auto bounds = label.bounds;
            bounds.moveTopLeft(bounds.topLeft() - margin);
            bounds.setSize(QSizeF(bounds.width() + marginSize2x, bounds.height() + marginSize2x));
            painterPath.addRect(bounds);
        }

        return painterPath;
    });

    m_scaledFont.setBinding([this]() {
        auto font = m_labelFont.value();
        font.setPointSizeF(m_labelScale.value() * font.pointSizeF());
        return font;
    });

    m_gridVisibleNotifier = m_gridVisible.addNotifier([this]() {
        if(m_gridVisible.value()) {
            beginInsertRows(QModelIndex(), 0, 2);
            endInsertRows();
        } else {
            beginRemoveRows(QModelIndex(), 0, 2);
            endRemoveRows();
        }
    });

    m_labelVisibleNotifier = m_labelVisible.addNotifier([this]() {
        if(m_labelVisible.value()) {
            beginInsertRows(QModelIndex(), 1, 2);
            endInsertRows();
        } else {
            beginRemoveRows(QModelIndex(), 1, 2);
            endRemoveRows();
        }
    });

    // Notify when line width changes (grid stroke width role)
    m_lineWidthNotifier = m_lineWidth.addNotifier([this]() {
        QModelIndex idx = index(0, 0);
        emit dataChanged(idx, idx, { StrokeWidthRole });
    });

    // Notify when line color changes (grid stroke color role)
    m_lineColorNotifier = m_lineColor.addNotifier([this]() {
        QModelIndex idx = index(0, 0);
        emit dataChanged(idx, idx, { StrokeColorRole });
    });

    // Notify when label color changes (label stroke color role)
    m_labelColorNotifier = m_labelColor.addNotifier([this]() {
        // label row is 1 when grid is visible, otherwise 0
        int row = m_gridVisible ? 1 : 0;
        QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, { StrokeColorRole });
    });

    // Notify when grid geometry/path changes (grid painter path role)
    m_gridPathNotifier = m_gridPath.addNotifier([this]() {
        QModelIndex idx = index(0, 0);
        emit dataChanged(idx, idx, { PainterPathRole });
    });

    // Notify when labels geometry/path changes (label painter path role)
    m_labelsPathNotifier = m_labelsPath.addNotifier([this]() {
        QModelIndex idx = index(2, 0);
        emit dataChanged(idx, idx, { PainterPathRole });
    });

    m_labelsBackgroundNotifier = m_labelBackgroundPath.addNotifier([this]() {
        QModelIndex idx = index(1, 0);
        emit dataChanged(idx, idx, { PainterPathRole });
    });
}

int cwSketch::FixedGridModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_size;
}

cwSketch::AbstractPainterPathModel::Path cwSketch::FixedGridModel::path(const QModelIndex &index) const
{

    auto gridPath = [this]() {
        return Path{
            m_gridPath,
            m_lineWidth,
            m_lineColor
        };
    };

    auto labelPath = [this]() {
        return Path{
            m_labelsPath,
            -1, //No line width
            m_labelColor
        };
    };

    auto labelBackground = [this]() {
        return Path{
            m_labelBackgroundPath,
            -1, //No line width
            m_labelBackgroundColor
        };
    };

    if(m_gridVisible.value() && m_labelVisible.value()) {
        if(index.row() == 0) {
            return gridPath();
        } else if(index.row() == 1) {
            return labelBackground();
        } else if(index.row() == 2) {
            return labelPath();
        }
    } else if(m_gridVisible.value()) {
        if(index.row() == 0) {
            return gridPath();
        }
    }

    // else if(m_labelVisible.value()) {
    //     if(index.row() == 0) {
    //         return labelPath();
    //     }
    // }

    Q_ASSERT(false); //Unhandled case
    static const Path emptyPath{};
    return emptyPath;
}

