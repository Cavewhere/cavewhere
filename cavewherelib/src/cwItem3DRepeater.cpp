/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

#include "cwItem3DRepeater.h"
#include "cwTransformUpdater.h"
#include "cwSelectionManager.h"
#include "cwCamera.h"
#include "cwDebug.h"

#include <QQmlContext>
#include <QQmlProperty>
#include <QVariant>

cwItem3DRepeater::cwItem3DRepeater(QQuickItem* parent) :
    cwAbstractPointManager(parent),
    m_transformUpdater(new cwTransformUpdater(this))
{
    connect(m_transformUpdater, &cwTransformUpdater::cameraChanged, this, &cwItem3DRepeater::cameraChanged);
    connect(this, &QQuickItem::visibleChanged, this, [this]() {
        if(m_transformUpdater != nullptr) {
            m_transformUpdater->setEnabled(isVisible());
        }
    });
}

cwItem3DRepeater::~cwItem3DRepeater()
{
    // children are deleteLater by Qt; nothing extra here
}

void cwItem3DRepeater::componentComplete()
{
    cwAbstractPointManager::componentComplete();
    if(m_transformUpdater != nullptr) {
        m_transformUpdater->setEnabled(isVisible());
    }
}

QAbstractItemModel* cwItem3DRepeater::model() const
{
    return m_model;
}

void cwItem3DRepeater::setModel(QAbstractItemModel* model)
{
    if(m_model == model) {
        return;
    }

    if(m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }

    m_model = model;

    if(m_model) {
        connect(m_model, &QAbstractItemModel::rowsInserted, this, &cwItem3DRepeater::onRowsInserted);
        connect(m_model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &cwItem3DRepeater::onRowsRemoved);
        connect(m_model, &QAbstractItemModel::rowsRemoved,
                this,
                [this](const QModelIndex &parent, int first, int last) {
            m_removingItems = false;
            updateAllItemData();
        });
        connect(m_model, &QAbstractItemModel::dataChanged, this, &cwItem3DRepeater::onDataChanged);
        connect(m_model, &QAbstractItemModel::modelReset, this, &cwItem3DRepeater::onModelReset);
    }

    const int count = m_model ? m_model->rowCount() : 0;
    resizeNumberOfItems(count);
    updateAllItemData();

    emit modelChanged();
}

int cwItem3DRepeater::positionRole() const
{
    return m_positionRole;
}

void cwItem3DRepeater::setPositionRole(int role)
{
    if(m_positionRole == role) {
        return;
    }
    m_positionRole = role;

    const int count = m_model ? m_model->rowCount() : 0;
    if(count > 0) {
        updateItemsPositions(0, count - 1);
    }

    emit positionRoleChanged();
}

cwCamera* cwItem3DRepeater::camera() const
{
    if(m_transformUpdater == nullptr) {
        return nullptr;
    }
    return m_transformUpdater->camera();
}

void cwItem3DRepeater::setCamera(cwCamera* camera)
{
    if(m_transformUpdater != nullptr) {
        m_transformUpdater->setCamera(camera);
    }
    emit cameraChanged();
}

void cwItem3DRepeater::onRowsInserted(const QModelIndex& parent, int first, int last)
{
    if(!isFlatListModel(parent)) {
        return;
    }
    pointsInserted(first, last);
}

void cwItem3DRepeater::onRowsRemoved(const QModelIndex& parent, int first, int last)
{
    if(!isFlatListModel(parent)) {
        return;
    }
    m_removingItems = true;
    pointsRemoved(first, last);
}

void cwItem3DRepeater::onDataChanged(const QModelIndex& topLeft,
                                     const QModelIndex& bottomRight,
                                     const QList<int>& roles)
{
    if(!m_model) {
        return;
    }
    if(!topLeft.isValid() || !bottomRight.isValid()) {
        return;
    }
    if(topLeft.parent().isValid() || bottomRight.parent().isValid()) {
        return;
    }

    const bool updateAllRoles = roles.isEmpty();
    const QHash<int, QByteArray> names = m_model->roleNames();

    for(int row = topLeft.row(); row <= bottomRight.row(); row++) {
        if(updateAllRoles) {
            refreshItemAt(row);
            continue;
        }

        // optimized: only rewrite changed roles + handle position if in set
        bool positionChanged = false;
        const QModelIndex idx = indexForRow(row);
        QQuickItem* item = items().value(row, nullptr);
        if(item == nullptr) {
            continue;
        }

        for(int role : roles) {
            const QByteArray roleName = names.value(role);
            if(roleName.isEmpty()) {
                continue;
            }
            const QVariant value = m_model->data(idx, role);
            item->setProperty(roleName.constData(), value);
            if(m_positionRole >= 0 && role == m_positionRole) {
                positionChanged = true;
            }
        }

        if(positionChanged) {
            updateItemPosition(item, row);
        }
    }
}

void cwItem3DRepeater::onModelReset()
{
    const int count = m_model ? m_model->rowCount() : 0;
    resizeNumberOfItems(count);
    updateAllItemData();
}

bool cwItem3DRepeater::isFlatListModel(const QModelIndex& parent) const
{
    return !parent.isValid();
}

QModelIndex cwItem3DRepeater::indexForRow(int row) const
{
    if(!m_model) {
        return QModelIndex();
    }
    if(row < 0 || row >= m_model->rowCount()) {
        return QModelIndex();
    }
    return m_model->index(row, 0);
}

void cwItem3DRepeater::updateItemData(QQuickItem* item, int pointIndex)
{
    if(m_removingItems) {
        return;
    }

    if(!m_model) {
        return;
    }

    const QModelIndex idx = indexForRow(pointIndex);
    if(!idx.isValid()) {
        return;
    }

    const QHash<int, QByteArray> names = m_model->roleNames();
    for(auto it = names.cbegin(); it != names.cend(); ++it) {
        const int role = it.key();
        const QByteArray roleName = it.value();
        const QVariant value = m_model->data(idx, role);
        item->setProperty(roleName.constData(), value);
    }
}

void cwItem3DRepeater::updateItemPosition(QQuickItem* item, int pointIndex)
{
    if(item == nullptr) {
        return;
    }
    if(m_positionRole < 0 || !m_model) {
        return;
    }

    const QModelIndex idx = indexForRow(pointIndex);
    if(!idx.isValid()) {
        return;
    }

    const QVariant posVar = m_model->data(idx, m_positionRole);
    if(!posVar.isValid()) {
        return;
    }

    const QVector3D position = posVar.value<QVector3D>();
    item->setProperty("position3D", position);
}

/* ===== Hooks to attach/detach transform updater ===== */

void cwItem3DRepeater::onItemCreated(QQuickItem* item, int index)
{
    Q_UNUSED(index);
    if(item == nullptr) {
        return;
    }
    if(m_transformUpdater != nullptr) {
        m_transformUpdater->addPointItem(item);
    }
}

void cwItem3DRepeater::onItemAboutToBeDestroyed(QQuickItem* item, int index)
{
    Q_UNUSED(index);
    if(item == nullptr) {
        return;
    }
    if(m_transformUpdater != nullptr) {
        m_transformUpdater->removePointItem(item);
    }
}

QVariantMap cwItem3DRepeater::initialItemProperties(QQuickItem *item, int index) const
{
    QVariantMap out;

    if(item == nullptr) {
        return out;
    }
    if(m_model == nullptr) {
        return out;
    }

    const QModelIndex idx = indexForRow(index);
    if(!idx.isValid()) {
        return out;
    }

    const QHash<int, QByteArray> names = m_model->roleNames();

    auto hasWritable = [](QObject* obj, const QByteArray& name) -> bool {
        const QQmlProperty prop(obj, QString::fromUtf8(name));
        return prop.isValid() && prop.isWritable();
    };

    // Seed only properties that actually exist on the delegate and are writable.
    for(auto it = names.cbegin(); it != names.cend(); ++it) {
        const int role = it.key();
        const QByteArray roleName = it.value();

        if(!hasWritable(item, roleName)) {
            continue;
        }

        const QVariant value = m_model->data(idx, role);
        if(value.isValid()) {
            out.insert(QString::fromUtf8(roleName), value);
        }
    }

    // Common convenience: row index if present
    const auto indexPropertyName = QByteArrayLiteral("pointIndex");
    if(hasWritable(item, indexPropertyName)) {
        out.insert(indexPropertyName, index);
    }

    // Some delegates might name it `position3D` instead of the roleName.
    if(hasWritable(item, QByteArrayLiteral("position3D"))) {
        const QVariant posVal = m_model->data(idx, m_positionRole);
        if(posVal.isValid()) {
            out.insert(QStringLiteral("position3D"), posVal);
        }
    }

    return out;
}
