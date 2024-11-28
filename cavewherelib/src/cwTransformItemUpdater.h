#ifndef CWTRANSFORMITEMUPDATER_H
#define CWTRANSFORMITEMUPDATER_H

//Qt includes
#include "cwPositioner3D.h"
#include <QObject>
#include <QMatrix4x4>
#include <QQuickItem>
#include <QSet>
#include <QQmlEngine>

class cwTransformItemUpdater : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TransformItemUpdater)
    Q_PROPERTY(QQuickItem* targetItem READ targetItem WRITE setTargetItem NOTIFY targetItemChanged)
    Q_PROPERTY(QMatrix4x4 matrix READ matrix BINDABLE bindableMatrix)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit cwTransformItemUpdater(QObject *parent = nullptr);

    void setTargetItem(QQuickItem* item);
    QQuickItem* targetItem() const;

    QMatrix4x4 matrix() const { return m_transformMatrix; }
    QBindable<QMatrix4x4> bindableMatrix() { return &m_transformMatrix; }

    bool enabled() const;
    void setEnabled(bool enabled);

    void addChildItem(cwPositioner3D* item);
    void removeChildItem(cwPositioner3D* item);

    Q_INVOKABLE QPointF mapFromItemToViewport(QPointF itemPoint) const;
    Q_INVOKABLE QPointF mapToItemFromViewport(QPointF viewportPoint) const;

signals:
    void targetItemChanged();
    void enabledChanged();

public slots:
    void update();


private:
    Q_OBJECT_BINDABLE_PROPERTY(cwTransformItemUpdater, QMatrix4x4, m_transformMatrix)
    QProperty<double> m_targetScale;
    QProperty<double> m_targetRotation;



    QPropertyNotifier m_matrixNotifier;

    QQuickItem* m_targetItem = nullptr;
    bool m_enabled = true;

    QSet<QQuickItem*> m_childItems;

    void updateChildItem(QQuickItem* item);
};

#endif // CWTRANSFORMITEMUPDATER_H
