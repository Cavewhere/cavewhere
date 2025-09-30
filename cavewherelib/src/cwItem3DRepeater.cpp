/**************************************************************************
**
**    Copyright (C) 2025 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwItem3DRepeater.h"
#include "cwTransformUpdater.h"
#include "cwSelectionManager.h"
#include "cwCamera.h"
#include "cwDebug.h"

// Qt includes
#include <QQmlContext>
#include <QQmlProperty>
#include <QVariantMap>
#include <QPointer>

cwItem3DRepeater::cwItem3DRepeater(QQuickItem* parent) :
    QQuickItem(parent),
    m_transformUpdater(new cwTransformUpdater(this)),
    m_selectionManager(new cwSelectionManager(this))
{
    connect(m_transformUpdater, &cwTransformUpdater::cameraChanged, this, &cwItem3DRepeater::cameraChanged);
    connect(this, &cwItem3DRepeater::visibleChanged, this, [this](){
        if(m_transformUpdater) {
            m_transformUpdater->setEnabled(isVisible());
        }
    });
}

cwItem3DRepeater::~cwItem3DRepeater()
{
    clearAll();
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
        disconnect(m_model, &QAbstractItemModel::rowsInserted, this, &cwItem3DRepeater::onRowsInserted);
        disconnect(m_model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &cwItem3DRepeater::onRowsRemoved);
        disconnect(m_model, &QAbstractItemModel::dataChanged, this, &cwItem3DRepeater::onDataChanged);
        disconnect(m_model, &QAbstractItemModel::modelReset, this, &cwItem3DRepeater::onModelReset);
    }

    m_model = model;

    if(m_model) {
        connect(m_model, &QAbstractItemModel::rowsInserted, this, &cwItem3DRepeater::onRowsInserted);
        connect(m_model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &cwItem3DRepeater::onRowsRemoved);
        connect(m_model, &QAbstractItemModel::dataChanged, this, &cwItem3DRepeater::onDataChanged);
        connect(m_model, &QAbstractItemModel::modelReset, this, &cwItem3DRepeater::onModelReset);
    }

    rebuildAll();
    emit modelChanged();
}

QUrl cwItem3DRepeater::qmlSource() const
{
    return m_qmlSource;
}

void cwItem3DRepeater::setQmlSource(const QUrl& source)
{
    if(m_qmlSource == source) {
        return;
    }

    m_qmlSource = source;
    m_itemComponent = nullptr;
    rebuildAll();
    emit qmlSourceChanged();
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

    for(int row = 0; row < m_items.size(); row++) {
        updateItemPosition(m_items[row], indexForRow(row));
    }
    emit positionRoleChanged();
}

cwCamera* cwItem3DRepeater::camera() const
{
    if(!m_transformUpdater) {
        return nullptr;
    }
    return m_transformUpdater->camera();
}

void cwItem3DRepeater::setCamera(cwCamera* camera)
{
    if(m_transformUpdater) {
        m_transformUpdater->setCamera(camera);
    }
    emit cameraChanged();
}

void cwItem3DRepeater::selectRow(int row)
{
    if(row < 0 || row >= m_items.size()) {
        return;
    }
    QQuickItem* item = m_items[row];
    if(item && m_selectionManager) {
        m_selectionManager->setSelectedItem(item);
    }
}

void cwItem3DRepeater::componentComplete()
{
    QQuickItem::componentComplete();
    ensureInfrastructure();
    rebuildAll();
}

void cwItem3DRepeater::onRowsInserted(const QModelIndex& parent, int first, int last)
{
    if(!isFlatListModel(parent)) {
        return;
    }
    spawnRows(first, last);
}

void cwItem3DRepeater::onRowsRemoved(const QModelIndex& parent, int first, int last)
{
    if(!isFlatListModel(parent)) {
        return;
    }
    if(first < 0 || last < first) {
        return;
    }

    for(int row = last; row >= first; row--) {
        destroyItemAtRow(row);
        m_items.removeAt(row);
    }
}

void cwItem3DRepeater::onDataChanged(const QModelIndex& topLeft,
                                     const QModelIndex& bottomRight,
                                     const QList<int>& roles)
{
    if(!topLeft.isValid() || !bottomRight.isValid()) {
        return;
    }
    if(topLeft.parent().isValid() || bottomRight.parent().isValid()) {
        return; // flat list only
    }

    const QHash<int, QByteArray> names = m_model->roleNames();
    const bool updateAllRoles = roles.isEmpty();
    const bool hasPositionRole = (m_positionRole >= 0);

    for(int row = topLeft.row(); row <= bottomRight.row(); row++) {
        QQuickItem* item = m_items[row];
        if(item == nullptr) {
            continue;
        }

        const QModelIndex index = indexForRow(row); // single call per row

        if(updateAllRoles) {
            for(auto it = names.cbegin(); it != names.cend(); ++it) {
                item->setProperty(it.value().constData(),
                                  m_model->data(index, it.key()));
            }
            if(hasPositionRole) {
                updateItemPosition(item, index);
            }
            continue;
        }

        bool positionChanged = false;
        for(int role : roles) {
            const QByteArray roleName = names.value(role);
            if(roleName.isEmpty()) {
                continue;
            }
            item->setProperty(roleName.constData(),
                              m_model->data(index, role));
            if(hasPositionRole && role == m_positionRole) {
                positionChanged = true;
            }
        }
        if(positionChanged) {
            updateItemPosition(item, index);
        }
    }
}

void cwItem3DRepeater::onModelReset()
{
    rebuildAll();
}

void cwItem3DRepeater::ensureInfrastructure()
{
    if(!m_transformUpdater) {
        m_transformUpdater = new cwTransformUpdater(this);
    }
    if(!m_selectionManager) {
        m_selectionManager = new cwSelectionManager(this);
    }

    if(m_transformUpdater) {
        m_transformUpdater->setEnabled(isVisible());
    }
}

void cwItem3DRepeater::ensureComponent()
{
    if(m_itemComponent) {
        return;
    }
    if(!isComponentComplete()) {
        return;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    if(context == nullptr) {
        qDebug() << "Context is nullptr, did you forget to set the context? THIS IS A BUG" << LOCATION;
        return;
    }

    if(!m_qmlSource.isEmpty()) {
        m_itemComponent = new QQmlComponent(context->engine(), m_qmlSource, this);
        if(m_itemComponent->isError()) {
            qDebug() << "Item component errors:" << m_itemComponent->errorString();
        }
    } else {
        qDebug() << "qmlSource is empty; cannot create row items." << LOCATION;
    }
}

void cwItem3DRepeater::clearAll()
{
    for(auto& entry : m_items) {
        if(entry) {
            if(m_transformUpdater) {
                m_transformUpdater->removePointItem(entry);
            }
            entry->setParent(nullptr);
            entry->setParentItem(nullptr);
            entry->deleteLater();
        }
    }
    m_items.clear();
}

void cwItem3DRepeater::rebuildAll()
{
    clearAll();

    if(!m_model || !isComponentComplete()) {
        return;
    }

    ensureComponent();

    const int rowCount = m_model->rowCount();
    if(rowCount <= 0) {
        return;
    }

    m_items.resize(rowCount);
    spawnRows(0, rowCount - 1);
}

bool cwItem3DRepeater::isFlatListModel(const QModelIndex& parent) const
{
    return !parent.isValid();
}

QQuickItem* cwItem3DRepeater::createItem(const QModelIndex& index)
{
    ensureComponent();
    if(!m_itemComponent) {
        return nullptr;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    if(context == nullptr) {
        qWarning() << "Null QQmlContext" << LOCATION;
        return nullptr;
    }

    QObject* obj = m_itemComponent->beginCreate(context);
    QQuickItem* item = qobject_cast<QQuickItem*>(obj);
    if(item == nullptr) {
        if(obj) { obj->deleteLater(); }
        qWarning() << "Delegate is not a QQuickItem" << LOCATION;
        return nullptr;
    }

    // helper that only writes if the property exists and is writable
    auto writeIfExists = [item](const char* name, const QVariant& v) -> bool {
        QQmlProperty prop(item, QString::fromLatin1(name));
        if(prop.isValid() && prop.isWritable()) {
            return prop.write(v);
        }
        return false;
    };

    auto hasWritableProperty = [](QObject* obj, const QByteArray& name) {
        QQmlProperty prop(obj, QString::fromUtf8(name));
        return prop.isValid() && prop.isWritable();
    };

    // set all roles that exist on the delegate (and are writable)
    const QHash<int, QByteArray> names = m_model->roleNames();
    for(auto it = names.cbegin(); it != names.cend(); ++it) {
        const int role = it.key();
        const QByteArray roleName = it.value();

        if(!hasWritableProperty(item, roleName)) {
            continue; // skip unknown or read-only properties (avoids data() call)
        }

        const QVariant variant = m_model->data(index, role);
        if(variant.isValid()) {
            QQmlProperty(item, QString::fromUtf8(roleName)).write(variant);
        }
    }

    // if(m_positionRole >= 0) {
    //     const QByteArray posName = names.value(m_positionRole);
    //     if(posName.isEmpty() || !hasWritableProperty(item, posName)) {
    //         obj->deleteLater();
    //         qWarning() << "Delegate missing required property for position role" << LOCATION;
    //         return nullptr;
    //     }
    //     const QVariant posVal = m_model->data(index, m_positionRole);
    //     if(!posVal.isValid()) {
    //         obj->deleteLater();
    //         qWarning() << "Model missing valid data for required position role" << LOCATION;
    //         return nullptr;
    //     }
    //     // If not already written above (it was, but harmless to re-write):
    //     QQmlProperty(item, QString::fromUtf8(posName)).write(posVal);
    // }

    // optional convenience
    writeIfExists("row", index.row());

    // complete construction (required props must be satisfied by now)
    m_itemComponent->completeCreate();

    // parent + transform hookup
    item->setParentItem(this);
    item->setParent(this);
    if(m_transformUpdater) {
        m_transformUpdater->addPointItem(item);
    }

    return item;
}

void cwItem3DRepeater::destroyItemAtRow(int row)
{
    if(row < 0 || row >= m_items.size()) {
        return;
    }
    QQuickItem* item = m_items[row];
    if(item) {
        if(m_transformUpdater) {
            m_transformUpdater->removePointItem(item);
        }
        item->setParentItem(nullptr);
        item->setParent(nullptr);
        item->deleteLater();
        qDebug() << "Delete later" << item;
        m_items[row] = nullptr;
    }
}

void cwItem3DRepeater::spawnRows(int first, int last)
{
    if(first > last) {
        return;
    }

    if(last >= m_items.size()) {
        m_items.resize(last + 1);
    }

    for (int row = first; row <= last; row++) {
        const QModelIndex idx = indexForRow(row);

        QQuickItem* item = createItem(idx);
        m_items[row] = item;

        if (item == nullptr) {
            continue;
        }

        // Optional: if the position can change, keep the fast path:
        updateItemPosition(item, idx);
    }
}

void cwItem3DRepeater::updateItemFromIndex(int row)
{
    if(!m_model) {
        return;
    }
    const QModelIndex idx = indexForRow(row);
    if(!idx.isValid()) {
        return;
    }

    QQuickItem* item = m_items[row];
    if(item == nullptr) {
        return;
    }

    exposeRolesToItem(item, idx);
}

void cwItem3DRepeater::updateItemPosition(QQuickItem* item, const QModelIndex& index)
{
    if(item == nullptr) {
        return;
    }
    if(m_positionRole < 0) {
        return;
    }
    const QVariant posVar = m_model->data(index, m_positionRole);
    if(!posVar.isValid()) {
        return;
    }

    const QVector3D position = posVar.value<QVector3D>();
    item->setProperty("position3D", position);
}

void cwItem3DRepeater::exposeRolesToItem(QQuickItem* item, const QModelIndex& index)
{
    if(item == nullptr || !m_model) {
        return;
    }

    const QHash<int, QByteArray> names = m_model->roleNames();
    for(auto it = names.constBegin(); it != names.constEnd(); ++it) {
        const int role = it.key();
        const QByteArray name = it.value();
        const QVariant value = m_model->data(index, role);
        item->setProperty(name.constData(), value);
    }
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
