/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Std includes
#include <limits>

// Our includes
#include "cwRenderLinePlot.h"
#include "cwRHILinePlot.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"

cwRenderLinePlot::cwRenderLinePlot(QObject *parent) :
    cwRenderObject(parent)
{
}

void cwRenderLinePlot::setGeometry(QVector<QVector3D> pointData,
                                   QVector<unsigned int> indexData,
                                   QVector<quint32> tripIds,
                                   int tripCount)
{
    Data data;

    // Find the max and min Z values
    data.maxZValue = -std::numeric_limits<float>::max();
    data.minZValue = std::numeric_limits<float>::max();

    for(const auto& point : pointData) {
        data.maxZValue = qMax(data.maxZValue, point.z());
        data.minZValue = qMin(data.minZValue, point.z());
    }

    // Hand the geometry to the intersecter before moving it into data — the
    // by-value params still own the only copy at this point, so the moves below
    // avoid a second deep copy of the (large) vertex/index vectors.
    if (auto* intersecter = geometryItersecter()) {
        cwGeometry geometry {
            { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
        };
        geometry.set(cwGeometry::Semantic::Position, pointData);
        geometry.setIndices(indexData);
        geometry.setType(cwGeometry::Type::Lines);

        intersecter->addObject(cwGeometryItersecter::Object(cwGeometryItersecter::Key{this, 0},
                                                            std::move(geometry)));
    }

    data.points = std::move(pointData);
    data.indexes = std::move(indexData);
    data.tripIds = std::move(tripIds);

    m_data.setValue(data);

    // New geometry renumbers trip ids, so the prior visibility no longer maps
    // to the right trips; reset to all-visible. The owner re-applies hidden
    // trips by id right after (cwLinePlotManager::reconcileTripKeywordItems).
    m_tripVisibility.setValue(QVector<quint8>(tripCount, kTripVisible));

    update();
}

void cwRenderLinePlot::setTripVisible(int tripId, bool visible)
{
    QVector<quint8> visibility = m_tripVisibility.value();
    if (tripId < 0 || tripId >= visibility.size()) {
        return;
    }

    const quint8 flag = visible ? kTripVisible : kTripHidden;
    if (visibility.at(tripId) == flag) {
        return;
    }

    visibility[tripId] = flag;
    m_tripVisibility.setValue(std::move(visibility));
    update();
}

cwRHIObject* cwRenderLinePlot::createRHIObject()
{
    return new cwRHILinePlot();
}


