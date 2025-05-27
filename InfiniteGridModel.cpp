#include "InfiniteGridModel.h"

//Qt incudes
#include <QScopedPropertyUpdateGroup>

using namespace cwSketch;

InfiniteGridModel::InfiniteGridModel(QObject *parent)
    : QConcatenateTablesProxyModel(parent)
    , m_majorGrid(new FixedGridModel(this))
    , m_minorGrid(new FixedGridModel(this))
{
    addSourceModel(m_minorGrid);
    addSourceModel(m_majorGrid);

    m_minorGrid->setObjectName(QStringLiteral("minorGridModel"));
    m_majorGrid->setObjectName(QStringLiteral("majorGridModel"));

    // Individual bindings
    m_majorGrid->bindableViewport().setBinding([this]() {
        return m_viewport.value();
    });
    m_minorGrid->bindableViewport().setBinding([this]() {
        return m_viewport.value();
    });

    m_majorGrid->bindableLabelColor().setBinding([this]() {
        return m_labelColor.value();
    });
    m_minorGrid->bindableLabelColor().setBinding([this]() {
        return m_labelColor.value();
    });

    m_majorGrid->bindableLineColor().setBinding([this]() {
        return m_lineColor.value();
    });
    m_minorGrid->bindableLineColor().setBinding([this]() {
        return m_lineColor.value();
    });

    m_majorGrid->bindableLabelFont().setBinding([this]() {
        return m_labelFont.value();
    });
    m_minorGrid->bindableLabelFont().setBinding([this]() {
        return m_labelFont.value();
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

    m_majorGrid->bindableLabelScale().setBinding([this]() {
        return m_viewScale.value();
    });
    m_minorGrid->bindableLabelScale().setBinding([this]() {
        return m_viewScale.value();
    });

    m_majorGrid->bindableMapMatrix().setBinding([this]() {
        return m_mapMatrix.value();
    });
    m_minorGrid->bindableMapMatrix().setBinding([this]() {
        return m_mapMatrix.value();
    });

    m_majorGrid->bindableLineWidth().setBinding([this]() {
        double lineWidth = m_lineWidth.value();
        return std::max(lineWidth, lineWidth * m_viewScale.value());
    });

    m_minorGrid->bindableLineWidth().setBinding([this]() {
        const double minorLineWidthScale = 0.5;
        double lineWidth = m_lineWidth.value() * minorLineWidthScale;
        return std::max(lineWidth, lineWidth * m_viewScale.value());
    });

    // m_minorGrid->bindableLabelVisible().setBinding([this]() {
    //     double minorGridPixels = m_minorGrid->gridIntervalPixels();
    //     return minorGridPixels > 40.0;
    // });

    m_minorGrid->setLabelVisible(false);

    auto interval = [](double base, double zoomLevel) {
        return base * std::pow(10, zoomLevel);
    };

    m_majorZoomGridInterval.setBinding([this, interval]() {
        qDebug() << "Major interval:" << interval(m_majorGridInterval.value(), m_clampedZoomLevel.value());
        return interval(m_majorGridInterval.value(), m_clampedZoomLevel.value());
    });

    m_minorZoomGridInterval.setBinding([this, interval]() {
        qDebug() << "Minor interval:" << interval(m_minorGridInterval.value(), m_clampedZoomLevel.value());
        return interval(m_minorGridInterval.value(), m_clampedZoomLevel.value());
    });

    auto updateMajorInterval = [this]() {
        qDebug() << "Major interval2:" << m_majorZoomGridInterval.value();
        m_majorGrid->gridInterval()->setValue(m_majorZoomGridInterval.value());
    };
    m_majorZoomGridIntervalNotifier = m_majorZoomGridInterval.addNotifier(updateMajorInterval);
    updateMajorInterval();

    auto updateMinorInterval = [this]() {
        qDebug() << "Minor interval2:" << m_minorZoomGridInterval.value();
        m_minorGrid->gridInterval()->setValue(m_minorZoomGridInterval.value());
    };
    m_minorZoomGridIntervalNotifier = m_minorZoomGridInterval.addNotifier(updateMinorInterval);
    updateMinorInterval();

    m_minorGridIntervalPixelNotifier = m_minorGrid->bindableGridIntervalPixels().addNotifier([this]() {
        double minorGridPixels = m_minorGrid->gridIntervalPixels();
        if(minorGridPixels < m_minorGridMinPixelInterval) {
            m_zoomLevel.setValue(m_zoomLevel.value() + 1);
        } else if (minorGridPixels > m_minorGridMinPixelInterval * 10.0) {
            m_zoomLevel.setValue(m_zoomLevel.value() - 1);
        }
        // m_clampedZoomLevel.notify();
        qDebug() << "Minor grid pixel size:" << m_minorGrid->gridIntervalPixels() << m_zoomLevel << "clamped zoom:" << m_clampedZoomLevel;
    });

    // test_clampedZoomLevelNotifier = m_clampedZoomLevel.addNotifier([this]() {
    //     qDebug() << "Clamped zoom changed:" << m_clampedZoomLevel;
    // });




}

TextModel *InfiniteGridModel::textModel() const
{
    return m_majorGrid->textModel();
}

int InfiniteGridModel::clampZoomLevel() const
{
    // qDebug() << "Clamped Zoom level:" << std::max(0, std::min(m_zoomLevel.value(), m_maxZoomLevel.value()));
    return std::max(0, std::min(m_zoomLevel.value(), m_maxZoomLevel.value()));
}

// double InfiniteGridModel::majorZoomGridInterval() const
// {
//     qDebug() << "MajorZoomGridInterval!" << m_clampedZoomLevel.value() * m_majorGridInterval.value();
//     return m_clampedZoomLevel.value() * m_majorGridInterval.value();
// }

// double InfiniteGridModel::minorZoomGridInterval() const
// {
//     return m_clampedZoomLevel.value() * m_minorGridInterval.value();
// }

