/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwKeywordColorBy.h"
#include "cwKeywordGroupByKeyModel.h"
#include "cwKeywordItemModel.h"
#include "cwVisibilityProxy.h"

// Qt includes
#include <QSet>

cwKeywordColorBy::cwKeywordColorBy(QObject* parent) :
    QObject(parent),
    m_groups(new cwKeywordGroupByKeyModel(this))
{
    // A value appearing or disappearing shifts every later slot, so each
    // change re-walks all rows.
    connect(m_groups, &QAbstractItemModel::rowsInserted, this, &cwKeywordColorBy::assignSlots);
    connect(m_groups, &QAbstractItemModel::rowsRemoved, this, &cwKeywordColorBy::assignSlots);
    connect(m_groups, &QAbstractItemModel::modelReset, this, &cwKeywordColorBy::assignSlots);
    connect(m_groups, &QAbstractItemModel::dataChanged, this, &cwKeywordColorBy::assignSlots);
}

void cwKeywordColorBy::setKeywordItemModel(cwKeywordItemModel* model)
{
    m_groups->setSourceModel(model);
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
    if (!m_groups->key().isEmpty()) {
        // An item moving between rows sits in both its value row and "Others"
        // for one signal, so a value row's slot wins. "Others" is always last.
        QSet<QObject*> assigned;
        const int otherRow = m_groups->rowCount() - 1;
        for (int row = 0; row <= otherRow; ++row) {
            const bool isOther = row == otherRow;
            const float slot = static_cast<float>(isOther ? kOtherSlot : row);
            const QVector<QObject*> objects = m_groups->index(row)
                .data(cwKeywordGroupByKeyModel::ObjectsRole).value<QVector<QObject*>>();
            for (QObject* object : objects) {
                // Point clouds, label groups and the like carry no proxy.
                auto* proxy = qobject_cast<cwVisibilityProxy*>(object);
                if (proxy == nullptr || (isOther && assigned.contains(proxy))) {
                    continue;
                }
                assigned.insert(proxy);
                proxy->setColorValue(slot);
            }
        }
    }
    updateValues();
}

void cwKeywordColorBy::updateValues()
{
    QStringList values;
    int slotCount = 0;
    if (!m_groups->key().isEmpty()) {
        const int otherRow = m_groups->rowCount() - 1;
        for (int row = 0; row < otherRow; ++row) {
            values.append(m_groups->index(row).data(cwKeywordGroupByKeyModel::ValueRole).toString());
        }
        slotCount = values.size();
        const QModelIndex other = m_groups->index(otherRow);
        if (other.data(cwKeywordGroupByKeyModel::ObjectCountRole).toInt() > 0) {
            values.append(other.data(cwKeywordGroupByKeyModel::ValueRole).toString());
        }
    }

    if (values == m_values && slotCount == m_slotCount) {
        return;
    }
    m_values = values;
    m_slotCount = slotCount;
    emit slotsChanged();
}
