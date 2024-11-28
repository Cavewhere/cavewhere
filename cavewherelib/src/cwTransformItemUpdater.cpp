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

void cwTransformItemUpdater::setTargetItem(QQuickItem* item)
{
    if (m_targetItem != nullptr) {
        disconnect(m_targetItem, nullptr, this, nullptr);
    }

    m_targetItem = item;

    if (m_targetItem != nullptr) {
        m_transformMatrix.setBinding([this](){
            QMatrix4x4 matrix;
            matrix.translate(m_targetItem->x(), m_targetItem->y());
            matrix.rotate(m_targetRotation, 0, 0, 1); // Assuming rotation around Z-axis
            matrix.scale(m_targetScale);

            return matrix; // Use the bindable property
        });

        connect(m_targetItem, &QQuickItem::scaleChanged, this, [this]() { m_targetScale.setValue(m_targetItem->scale()); });
        connect(m_targetItem, &QQuickItem::rotationChanged, this, [this]() { m_targetRotation.setValue(m_targetItem->rotation()); });

        m_targetScale.setValue(m_targetItem->scale());
        m_targetRotation.setValue(m_targetItem->rotation());

        emit targetItemChanged();
    } else {
        //Removes the binding
        m_transformMatrix = QMatrix4x4();
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
    QVector3D transformedPos = m_transformMatrix.value().map(position3D);
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
