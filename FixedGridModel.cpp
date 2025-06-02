#include "FixedGridModel.h"
#include "TextModel.h"

#include "cwLength.h"


cwSketch::FixedGridModel::FixedGridModel(QObject *parent) :
    m_gridInterval(new cwLength(10.0, cwUnits::Meters, this)),
    m_textModel(new TextModel(this))
{
    m_gridVisibleNotifier = m_gridVisible.addNotifier([this]() {
        if(m_gridVisible.value()) {
            beginInsertRows(QModelIndex(), GridLineIndex, LabelBackgroundIndex);
            m_size = 2;
            endInsertRows();
        } else {
            beginRemoveRows(QModelIndex(), GridLineIndex, LabelBackgroundIndex);
            m_size = 0;
            endRemoveRows();
        }
    });

    m_labelVisibleNotifier = m_labelVisible.addNotifier([this]() {
        if(m_gridVisible.value()) {
            if(m_labelVisible.value()) {
                beginInsertRows(QModelIndex(), LabelBackgroundIndex, LabelBackgroundIndex);
                m_size = 2;
                endInsertRows();
            } else {
                qDebug() << "remove background labels: rowCount:" << rowCount() << LabelBackgroundIndex;
                beginRemoveRows(QModelIndex(), LabelBackgroundIndex, LabelBackgroundIndex);
                m_size = 1;
                endRemoveRows();
            }
        }
    });

    connect(m_gridInterval, &cwLength::valueChanged, this, [this]() {
        m_gridIntervalLength.notify();
    });

    connect(m_gridInterval, &cwLength::unitChanged, this, [this]() {
        m_gridIntervalUnit.notify();
    });


    // m_size.setBinding([this]() {
    //     if(!m_gridVisible) {
    //         //Labels are also hidden when the grid lines are hidden
    //         return 0;
    //     }

    //     if(m_labelVisible) {
    //         return LabelBackgroundIndex + 1;
    //     }

    //     return GridLineIndex + 1;
    // });


    auto gridLines = [this](double left, double right, double origin, double scale) {
        QVector<GridLine> positions;

        // nothing to do if grid is hidden or interval/scale invalid
        if (!m_gridVisible.value()) {
            return positions;
        }

        //Interval in cave (meters)
        double interval = cwUnits::convert(m_gridIntervalLength.value(), (cwUnits::LengthUnit)m_gridIntervalUnit.value(), cwUnits::Meters);
        double ignoreInterval = cwUnits::convert(m_hiddenInterval.value(), (cwUnits::LengthUnit)m_gridIntervalUnit.value(), cwUnits::Meters);

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
            bool keep = ignoreInterval > 0.0 ? std::abs(std::fmod(positionInMeters, ignoreInterval)) > 0.0001 : true;

            if(keep) {
                const GridLine gridLine {
                    positionInMeters * scale, //in map pixels
                    positionInMeters //in cave units
                };

                positions.append(gridLine);
            }
            // qDebug() << "Position:" << positions.last().position;
        }

        return positions;
    };

    m_xGridLines.setBinding([this, gridLines]() {
        QRectF viewport = m_viewport.value();
        QMatrix4x4 mapMatrix = m_mapMatrix.value();
        const double xScale = mapMatrix(0,0);
        return gridLines(viewport.left(), viewport.right(), m_origin.value().x(), xScale);
    });

    m_gridIntervalPixels.setBinding([this]() {
        //Interval in cave (meters)
        double interval = cwUnits::convert(m_gridIntervalLength.value(), (cwUnits::LengthUnit)m_gridIntervalUnit.value(), cwUnits::Meters);
        QMatrix4x4 mapMatrix = m_mapMatrix.value();
        const double xScale = mapMatrix(0,0);
        // qDebug() << "xScale:" << xScale;
        // return interval * xScale * 1.0 / m_labelScale;
        return interval * xScale * 1.0 / m_labelScale;
    });

    m_yGridLines.setBinding([this, gridLines]() {
        QRectF viewport = m_viewport.value();
        QMatrix4x4 mapMatrix = m_mapMatrix.value();
        const double yScale = mapMatrix(1,1);
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

    auto extraWidth = [](QRectF bounds) {
        bounds.setWidth(bounds.width() + 1);
        return bounds;
    };

    auto scaleBounds = [this](QRectF bounds){
        double labelScale = m_labelScale.value();
        bounds.setWidth(bounds.width() * labelScale);
        bounds.setHeight(bounds.height() * labelScale);
        return bounds;
    };

    auto textBounds = [extraWidth, scaleBounds](const QString& text, const QFontMetricsF& fontMetrics) {
        return scaleBounds(extraWidth(fontMetrics.boundingRect(text)));
    };

    auto toString = [this](double num) {
        if(num == 0.0) {
            return QString::number(num) + m_gridInterval->unitName(m_gridInterval->unit());
        } else {
            return QString::number(num);
        }
    };

    m_gridLabels.setBinding([this, textBounds, toString]() {
        QVector<GridLabel> labels;

        if(m_labelVisible.value()) {

            const auto viewport = m_viewport.value();
            const auto xGridLines = m_xGridLines.value();
            const auto yGridLines = m_yGridLines.value();

            const QFont font = m_labelFont.value();
            QFontMetricsF fontMetrics(font);



            labels.reserve(xGridLines.size() + yGridLines.size());



            for(const GridLine& xValue: xGridLines) {
                const QString text = toString(xValue.value);
                QRectF bounds = textBounds(text, fontMetrics);
                bounds.moveTo(xValue.position - bounds.width() * 0.5, viewport.bottom() - bounds.height());


                labels.append(GridLabel{
                    bounds,
                    text
                });
            }

            for(const GridLine& yValue : yGridLines) {
                const QString text = toString(yValue.value);
                QRectF bounds = textBounds(text, fontMetrics);
                bounds.moveTo(viewport.left(), yValue.position - bounds.height() * 0.5);

                labels.append(GridLabel{
                    bounds,
                    text
                });
            }
        }

        return labels;
    });

    m_maxLabelSizePixels.setBinding([this, toString]() {
        // const auto gridLabels = m_gridLabels.value();

        // if(!gridLabels.isEmpty()) {
        //     QSizeF max;
        //     for(const auto& gridLabel : gridLabels ) {
        //         double newWidth = gridLabel.bounds.size().width();
        //         double newHeight = gridLabel.bounds.size().height();
        //         if(newWidth > max.width()) {
        //             max.setWidth(newWidth);
        //         } else if(newHeight > max.height()){
        //             max.setHeight(newHeight);
        //         }
        //     }
        //     return max;
        // } else {
            const QFont font = m_labelFont.value();
            QFontMetricsF fontMetrics(font);
            double gridInterval = m_gridIntervalLength.value(); //in meters
            double squared = gridInterval * gridInterval * 10 * 9;
            auto bounds = fontMetrics.boundingRect(toString(-squared)); //textBounds(QStringLiteral("-100"), fontMetrics);
            qDebug() << "Squared:" << squared << toString(-squared) << bounds;
            return bounds.size();
        // }
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



    // Notify when line width changes (grid stroke width role)
    m_lineWidthNotifier = m_lineWidth.addNotifier([this]() {
        // qDebug() << "LineWidth changed:" << m_lineWidth.value() << this;
        QModelIndex idx = index(GridLineIndex);
        if(idx.isValid()) {
            emit dataChanged(idx, idx, { StrokeWidthRole });
        }
    });

    // Notify when line color changes (grid stroke color role)
    m_lineColorNotifier = m_lineColor.addNotifier([this]() {
        QModelIndex idx = index(GridLineIndex);
        if(idx.isValid()) {
            emit dataChanged(idx, idx, { StrokeColorRole });
        }
    });

    // Notify when label color changes (label stroke color role)
    m_labelColorNotifier = m_labelColor.addNotifier([this]() {
        // label row is 1 when grid is visible, otherwise 0
        int row = m_gridVisible ? LabelBackgroundIndex : GridLineIndex;
        QModelIndex idx = index(row);
        if(idx.isValid()) {
            emit dataChanged(idx, idx, { StrokeColorRole });
        }
    });

    // Notify when grid geometry/path changes (grid painter path role)
    m_gridPathNotifier = m_gridPath.addNotifier([this]() {
        QModelIndex idx = index(GridLineIndex);
        if(idx.isValid()) {
            emit dataChanged(idx, idx, { PainterPathRole });
        }
    });

    m_labelsBackgroundNotifier = m_labelBackgroundPath.addNotifier([this]() {
        QModelIndex idx = index(LabelBackgroundIndex);
        if(idx.isValid()) {
            emit dataChanged(idx, idx, { PainterPathRole });
        }
    });

    m_textRows.setBinding([this]() {
        const auto font = m_labelFont.value();
        const auto fillColor = m_labelColor.value();

        // Helper to convert a GridLabel -> TextRow
        auto makeRow = [this, font, fillColor](const GridLabel& gridLabel){
            TextModel::TextRow row;
            row.text        = gridLabel.text;
            row.position    = gridLabel.bounds.topLeft();
            row.font        = font;
            row.fillColor   = fillColor;
            row.strokeColor = Qt::transparent;
            return row;
        };

        auto labels = m_gridLabels.value();

        QVector<TextModel::TextRow> rows;
        rows.reserve(labels.size());

        for(const auto& label : labels) {
            rows.append(makeRow(label));
        }

        return rows;
    });

    m_gridLabelsNotifier = m_textRows.addNotifier([this]() {
        m_textModel->replaceRows(m_textRows);
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

    auto labelBackground = [this]() {
        return Path{
            m_labelBackgroundPath,
            -1, //No line width
            m_labelBackgroundColor
        };
    };


    if(m_gridVisible.value() && m_labelVisible.value()) {
        if(index.row() == GridLineIndex) {
            return gridPath();
        } else if(index.row() == LabelBackgroundIndex) {
            return labelBackground();
        }
    } else if(m_gridVisible.value()) {
        if(index.row() == GridLineIndex) {
            return gridPath();
        }
    }

    //This is an unhandled case, because the model and QProperty will be in an
    //intermidant state, we need to return an empty path.
    return Path {};
}


