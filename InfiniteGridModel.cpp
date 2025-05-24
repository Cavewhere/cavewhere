#include "InfiniteGridModel.h"


cwSketch::InfiniteGridModel::InfiniteGridModel(QObject *parent) :
    m_gridInterval(new cwLength(10.0, cwUnits::Meters, this))
{
    m_size.setBinding([this]() {
        if(!m_gridVisible) {
            //Labels are also hidden when the grid lines are hidden
            return 0;
        }

        if(m_labelVisible) {
            return 2;
        }

        return 1;
    });

    auto gridLines = [this](double left, double right, double scale) {
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
        double leftMeter = left / scale;
        double rightMeter = right / scale;

        double flipped = leftMeter > rightMeter ? -1.0 : 1.0;


        // first grid line at or before left edge
        int count = static_cast<int>(fabs(rightMeter - leftMeter) / interval) + 1;
        positions.reserve(count);

        // generate: world → pixel
        qDebug() << "Lines:" << leftMeter << "right m:" << right << "scale:" << scale << "count:" << count << "interval:" << interval;
        for(int i = 0; i < count; ++i) {
            const GridLine gridLine {
                (left + i * interval) * scale * flipped,
                i * interval * flipped
            };

            positions.append(gridLine);
            qDebug() << "Position:" << positions.last().position;
        }

        return positions;
    };

    m_xGridLines.setBinding([this, gridLines]() {
        auto viewport = m_viewport.value();
        auto mapMatrix = m_mapMatrix.value();
        const double xScale = mapMatrix(0,0);
        return gridLines(viewport.left(), viewport.right(), xScale);
    });

    m_yGridLines.setBinding([this, gridLines]() {
        auto viewport = m_viewport.value();
        auto mapMatrix = m_mapMatrix.value();
        const double yScale = mapMatrix(1,1);
        return gridLines(viewport.top(), viewport.bottom(), yScale);
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

    m_labelsPath.setBinding([this]() {
        QPainterPath painterPath;
        if(m_labelVisible.value()) {
            const auto viewport = m_viewport.value();
            const auto xGridLines = m_xGridLines.value();

            auto toString = [](double num) {
                return QString::number(num);
            };

            for(const GridLine& xValue: xGridLines) {
                painterPath.addText(QPointF(xValue.position, viewport.top()), m_labelFont, toString(xValue.value));
                painterPath.addText(QPointF(xValue.position, viewport.bottom()), m_labelFont, toString(xValue.value));
            }

            const auto yGridLines = m_yGridLines.value();
            for(const GridLine& yValue : yGridLines) {

                painterPath.addText(QPointF(viewport.left(), yValue.position), m_labelFont, toString(yValue.value));
                painterPath.addText(QPointF(viewport.right(), yValue.position), m_labelFont, toString(yValue.value));
            }
        }
        return painterPath;
    });

    m_gridVisibleNotifier = m_gridVisible.addNotifier([this]() {
        if(m_gridVisible.value()) {
            beginInsertRows(QModelIndex(), 0, 1);
            endInsertRows();
        } else {
            beginRemoveRows(QModelIndex(), 0, 1);
            endRemoveRows();
        }
    });

    m_labelVisibleNotifier = m_labelVisible.addNotifier([this]() {
        if(m_labelVisible.value()) {
            beginInsertRows(QModelIndex(), 1, 1);
            endInsertRows();
        } else {
            beginRemoveRows(QModelIndex(), 1, 1);
            endRemoveRows();
        }
    });

}

int cwSketch::InfiniteGridModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_size;
}

cwSketch::AbstractPainterPathModel::Path cwSketch::InfiniteGridModel::path(const QModelIndex &index) const
{

    auto gridPath = [this]() {
        return Path{
            m_gridPath,
            m_lineWidth,
            m_lineColor
        };
    };

    auto labelPath = [this] {
        qDebug() << "returning label path:" << m_labelsPath.value().elementCount();
        return Path{
            m_labelsPath,
            -1, //No line width
            m_labelColor
        };
    };

    if(m_gridVisible.value() && m_labelVisible.value()) {
        if(index.row() == 0) {
            return gridPath();
        } else if(index.row() == 1) {
            return labelPath();
        }
    } else if(m_gridVisible.value()) {
        if(index.row() == 0) {
            return gridPath();
        }
    } else if(m_labelVisible.value()) {
        if(index.row() == 0) {
            return labelPath();
        }
    }

    Q_ASSERT(false); //Unhandled case
    static const Path emptyPath{};
    return emptyPath;
}

