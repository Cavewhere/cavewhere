#include "InfiniteGridModel.h"


cwSketch::InfiniteGridModel::InfiniteGridModel(QObject *parent) :
    m_gridInterval(new cwLength(10.0, cwUnits::Meters, this))
{
    m_size.setBinding([this]() {
        int count = 0;
        if(m_gridVisible) {
            ++count;
        }
        if(m_labelVisible) {
            ++count;
        }
        return count;
    });

    auto gridLines = [this](double left, double right) {
        QVector<double> positions;

        // nothing to do if grid is hidden or interval/scale invalid
        if (!m_gridVisible.value()) {
            return positions;
        }

        double interval = m_gridInterval->value();
        if (interval <= 0.0 || m_mapScale.value() <= 0.0) {
            return positions;
        }

        // world‐space bounds
        double scale = m_mapScale.value();  // pixels per world unit

        // first grid line at or before left edge
        double start = std::floor(left / interval) * interval;

        // generate: world → pixel
        for (double wx = start; wx <= right; wx += interval) {
            positions.append((wx - left) * scale);
        }

        return positions;
    };

    m_xGridLines.setBinding([this, gridLines]() {
        auto viewport = m_viewport.value();
        return gridLines(viewport.left(), viewport.right());
    });

    m_yGridLines.setBinding([this, gridLines]() {
        auto viewport = m_viewport.value();
        return gridLines(viewport.left(), viewport.right());
    });

    m_gridPath.setBinding([this]() {
        QPainterPath painterPath;
        if(m_gridVisible.value()) {
            const auto viewport = m_viewport.value();
            const auto xGridLines = m_xGridLines.value();
            for(double xValue : xGridLines) {
                painterPath.moveTo(xValue, viewport.top());
                painterPath.lineTo(xValue, viewport.bottom());
            }

            const auto yGridLines = m_yGridLines.value();
            for(double yValue : yGridLines) {
                painterPath.moveTo(viewport.left(), yValue);
                painterPath.lineTo(viewport.right(), yValue);
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

            for(double xValue : xGridLines) {
                painterPath.addText(QPointF(xValue, viewport.top()), m_labelFont, toString(xValue));
                painterPath.addText(QPointF(xValue, viewport.bottom()), m_labelFont, toString(xValue));
            }

            const auto yGridLines = m_yGridLines.value();
            for(double yValue : yGridLines) {
                painterPath.addText(QPointF(yValue, viewport.left()), m_labelFont, toString(yValue));
                painterPath.addText(QPointF(yValue, viewport.right()), m_labelFont, toString(yValue));
            }
        }
        return painterPath;
    });


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
        return Path{
            m_labelsPath,
            -1, //No line width
            m_labelColor
        };
    };

    if(m_gridVisible.value() && m_labelVisible.value()) {
        if(index.row() == 0) {
            return gridPath();
        } else if(index.row() == 0) {
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

