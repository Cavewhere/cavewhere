/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwNodePageModel.h"
#include "cwSurveyNode.h"
#include "cwSurveyTreeModel.h"
#include "cwTrip.h"

cwNodePageModel::cwNodePageModel(QObject* parent) :
    QObject(parent)
{
}

void cwNodePageModel::setNode(cwSurveyNode* node)
{
    if(m_node == node) {
        return;
    }

    disconnect(m_subtreeConnection);
    disconnect(m_kindConnection);
    disconnect(m_destroyedConnection);
    m_node = node;

    if(m_node != nullptr) {
        m_subtreeConnection = connect(m_node, &cwSurveyNode::subtreeChanged,
                                      this, &cwNodePageModel::rewireTrips);
        m_kindConnection = connect(m_node, &cwSurveyNode::kindChanged,
                                   this, &cwNodePageModel::kindLabelChanged);
        //The QPointer is already null here, so the rewire reads an empty node.
        m_destroyedConnection = connect(m_node, &QObject::destroyed, this, [this]() {
            rewireTrips();
            emit kindLabelChanged();
            emit nodeChanged();
        });
    }

    rewireTrips();
    emit kindLabelChanged();
    emit nodeChanged();
}

QString cwNodePageModel::kindLabel() const
{
    return m_node != nullptr ? cwSurveyTreeModel::kindLabel(m_node->kind()) : QString();
}

void cwNodePageModel::rewireTrips()
{
    disconnectTrips();

    if(m_node == nullptr) {
        setTripCount(0);
        setLastSurvey(QDateTime());
        return;
    }

    const QList<cwTrip*> trips = m_node->allTrips();
    m_tripConnections.reserve(trips.size());
    for(cwTrip* trip : trips) {
        m_tripConnections.append(connect(trip, &cwTrip::dateChanged,
                                         this, &cwNodePageModel::updateLastSurvey));
    }

    setTripCount(trips.size());
    updateLastSurvey();
}

void cwNodePageModel::updateLastSurvey()
{
    QDateTime latest;
    if(m_node != nullptr) {
        for(const cwTrip* trip : m_node->allTrips()) {
            const QDateTime date = trip->date();
            if(date.isValid() && (!latest.isValid() || date > latest)) {
                latest = date;
            }
        }
    }
    setLastSurvey(latest);
}

void cwNodePageModel::disconnectTrips()
{
    for(const auto& connection : std::as_const(m_tripConnections)) {
        disconnect(connection);
    }
    m_tripConnections.clear();
}

void cwNodePageModel::setTripCount(int count)
{
    if(m_tripCount == count) {
        return;
    }
    m_tripCount = count;
    emit tripCountChanged();
}

void cwNodePageModel::setLastSurvey(const QDateTime& date)
{
    if(m_lastSurvey == date) {
        return;
    }
    m_lastSurvey = date;
    emit lastSurveyChanged();
}
