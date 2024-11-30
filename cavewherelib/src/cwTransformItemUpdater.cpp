#include "cwTransformItemUpdater.h"
#include "cwPositioner3D.h"
#include <QDebug>

cwTransformItemUpdater::cwTransformItemUpdater(QObject *parent)
    : QObject(parent)
{
    m_matrixNotifier = m_transformMatrix.addNotifier([this]() {
        update();
    });
}

void cwTransformItemUpdater::setWatchTransformItem(QQuickItem *item)
{
    if (m_watchTransformItem != nullptr) {
        disconnect(m_watchTransformItem, nullptr, this, nullptr);
    }

    m_watchTransformItem = item;

    if (m_watchTransformItem != nullptr) {
        m_transformMatrix.setBinding([this](){
            QMatrix4x4 matrix;
            matrix.translate(m_watchTransformItem->x(), m_watchTransformItem->y());
            matrix.scale(m_targetScale, m_targetScale, 1.0);
            matrix.rotate(m_targetRotation, 0, 0, 1); // Assuming rotation around Z-axis

            // qDebug() << "Scale:" << m_targetScale;
            // qDebug() << "Matrix1:" << matrix;

            return matrix; // Use the bindable property
        });

        connect(m_watchTransformItem, &QQuickItem::scaleChanged, this, [this]() { m_targetScale.setValue(m_watchTransformItem->scale()); });
        connect(m_watchTransformItem, &QQuickItem::rotationChanged, this, [this]() { m_targetRotation.setValue(m_watchTransformItem->rotation()); });

        m_targetScale.setValue(m_watchTransformItem->scale());
        m_targetRotation.setValue(m_watchTransformItem->rotation());

        emit watchTransformItemChanged();
    } else {
        //Removes the binding
        m_transformMatrix = QMatrix4x4();
    }
}

void cwTransformItemUpdater::setTargetItem(QQuickItem* item)
{
    if(m_targetItem != item) {
        m_targetItem = item;
        emit targetItemChanged();
        update();
    }
}

QQuickItem* cwTransformItemUpdater::targetItem() const
{
    return m_targetItem;
}

bool cwTransformItemUpdater::enabled() const
{
    return m_enabled;
}

void cwTransformItemUpdater::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        update();
        emit enabledChanged();
    }
}

void cwTransformItemUpdater::addChildItem(cwPositioner3D* item)
{
    if (item == nullptr) return;

    if (m_childItems.contains(item)) {
        qDebug() << "Adding item twice in cwTransformItemUpdater";
        return;
    }

    m_childItems.insert(item);
    connect(item, &QObject::destroyed, this, [this, item]() {
        removeChildItem(item);
    });
    connect(item, &cwPositioner3D::position3DChanged, this, [this, item]() {
        updateChildItem(item);
    });

    updateChildItem(item);
}

void cwTransformItemUpdater::removeChildItem(cwPositioner3D* item)
{
    if (item == nullptr) return;

    if (!m_childItems.contains(item)) {
        qDebug() << item << " isn't in the cwTransformItemUpdater, can't remove it";
        return;
    }

    disconnect(item, nullptr, this, nullptr);
    m_childItems.remove(item);
}

void cwTransformItemUpdater::update()
{
    if (m_enabled) {
        for(QQuickItem* item : std::as_const(m_childItems)) {
            updateChildItem(item);
        }
    }
}


void cwTransformItemUpdater::updateChildItem(QQuickItem* item)
{
    if (item == nullptr) return;

    // Assume the item has a property "position3D" which is its position relative to TargetItem
    QVector3D position3D = item->property("position3D").value<QVector3D>();

    QPointF transformedPos = item->parentItem()->mapFromItem(m_targetItem, position3D.x(), position3D.y());

    // qDebug() << "TransformPos:" << transformedPos << m_transformMatrix.value().inverted().map(position3D);

    // QVector3D transformedPos = m_transformMatrix.value().map(position3D);
    item->setPosition(QPointF(transformedPos.x(), transformedPos.y()));
}

QPointF cwTransformItemUpdater::mapFromItemToViewport(QPointF itemPoint) const
{
    QVector3D itemPoint3D(itemPoint);
    QVector3D viewportPoint = m_transformMatrix.value().map(itemPoint3D);
    return viewportPoint.toPointF();
}

QPointF cwTransformItemUpdater::mapToItemFromViewport(QPointF viewportPoint) const
{
    QVector3D viewportPoint3D(viewportPoint);
    QVector3D itemPoint = m_transformMatrix.value().inverted().map(viewportPoint3D);
    return itemPoint.toPointF();
}
