/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwKeywordColorBy.h"
#include "cwKeywordGroupByKeyModel.h"
#include "cwVisibilityProxy.h"

// Qt includes
#include <QHash>
#include <QTimer>

cwKeywordColorBy::cwKeywordColorBy(QObject* parent) :
    QObject(parent),
    m_groups(new cwKeywordGroupByKeyModel(this)),
    m_assignTimer(new QTimer(this))
{
    m_assignTimer->setSingleShot(true);
    m_assignTimer->setInterval(0);
    connect(m_assignTimer, &QTimer::timeout, this, &cwKeywordColorBy::assignSlots);

    // A value appearing or disappearing shifts every later slot, so each
    // change re-walks all rows.
    const auto scheduleAssign = [this]() { m_assignTimer->start(); };
    connect(m_groups, &QAbstractItemModel::rowsInserted, this, scheduleAssign);
    connect(m_groups, &QAbstractItemModel::rowsRemoved, this, scheduleAssign);
    connect(m_groups, &QAbstractItemModel::modelReset, this, scheduleAssign);
    connect(m_groups, &QAbstractItemModel::dataChanged, this, scheduleAssign);
}

void cwKeywordColorBy::setItemModel(QAbstractItemModel* model)
{
    m_groups->setSourceModel(model);
}

void cwKeywordColorBy::regroup()
{
    if (m_groups->key().isEmpty()) {
        return;
    }
    m_groups->updateAllRows();
}

QString cwKeywordColorBy::key() const
{
    return m_groups->key();
}

void cwKeywordColorBy::setKey(const QString& key)
{
    m_groups->setKey(key);
}

void cwKeywordColorBy::assignSlots()
{
    bool hasOthers = false;
    if (!m_groups->key().isEmpty()) {
        const auto rowProxies = [this](int row) {
            QVector<cwVisibilityProxy*> proxies;
            const QVector<QObject*> objects = m_groups->index(row)
                .data(cwKeywordGroupByKeyModel::ObjectsRole).value<QVector<QObject*>>();
            for (QObject* object : objects) {
                // Point clouds, label groups and the like carry no proxy.
                if (auto* proxy = qobject_cast<cwVisibilityProxy*>(object)) {
                    proxies.append(proxy);
                }
            }
            return proxies;
        };

        // An item with two values for the key (two cavers on one trip) sits
        // in both value rows; one color cannot show both, so it goes to
        // "Others". "Others" is always the last row.
        QHash<cwVisibilityProxy*, int> slotByProxy;
        const int otherRow = m_groups->rowCount() - 1;
        for (int row = 0; row < otherRow; ++row) {
            for (cwVisibilityProxy* proxy : rowProxies(row)) {
                const auto existing = slotByProxy.constFind(proxy);
                if (existing == slotByProxy.cend()) {
                    slotByProxy.insert(proxy, row);
                } else {
                    slotByProxy.insert(proxy, kOtherSlot);
                    hasOthers = true;
                }
            }
        }
        // An item moving between rows sits in a value row and "Others" for
        // one signal, so a value row's slot wins.
        for (cwVisibilityProxy* proxy : rowProxies(otherRow)) {
            if (!slotByProxy.contains(proxy)) {
                slotByProxy.insert(proxy, kOtherSlot);
            }
        }

        for (auto it = slotByProxy.cbegin(); it != slotByProxy.cend(); ++it) {
            it.key()->setColorValue(static_cast<float>(it.value()));
        }

        hasOthers = hasOthers
                    || m_groups->index(otherRow).data(cwKeywordGroupByKeyModel::ObjectCountRole).toInt() > 0;
    }
    updateValues(hasOthers);
}

void cwKeywordColorBy::updateValues(bool hasOthers)
{
    QStringList values;
    int slotCount = 0;
    if (!m_groups->key().isEmpty()) {
        const int otherRow = m_groups->rowCount() - 1;
        for (int row = 0; row < otherRow; ++row) {
            values.append(m_groups->index(row).data(cwKeywordGroupByKeyModel::ValueRole).toString());
        }
        slotCount = values.size();
        if (hasOthers) {
            values.append(m_groups->index(otherRow).data(cwKeywordGroupByKeyModel::ValueRole).toString());
        }
    }

    if (values == m_values && slotCount == m_slotCount) {
        return;
    }
    m_values = values;
    m_slotCount = slotCount;
    emit slotsChanged();
}
