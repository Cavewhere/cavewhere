#ifndef CWBILLBOARDTEST_H
#define CWBILLBOARDTEST_H

// =====================================================================
// PROTOTYPE / RESEARCH SPIKE for issues #535 (leads) and #536 (station
// labels). Throwaway code: it proves that a live QML Item can be rendered
// as a depth-occluded billboard *inside* the cwRhiScene 3D pass, reusing
// Qt Quick's own scene-graph renderer (QSGRenderer in RenderMode3D) via
// private API — the same mechanism QtQuick3D uses for 2D-in-3D content.
//
// Not for merge. The shipping design will encapsulate every private-API
// call behind a small public CaveWhere surface (see the plan).
// =====================================================================

//Qt includes
#include <QQuickItem>
#include <QQmlEngine>
#include <QPointer>
#include <QVector3D>

//Our includes
#include "cwRenderObject.h"

//Std includes
#include <memory>

class cwScene;
class cwRHIObject;
class cwQuickItemSubscene;
class QQmlComponent;

// GUI-side render object handed to cwScene. Carries the referenced subscene and
// the world position to the render thread; spawns the matching cwRHIObject
// backend.
class cwBillboardTestRender : public cwRenderObject
{
    Q_OBJECT
public:
    explicit cwBillboardTestRender(QObject* parent = nullptr);

    cwQuickItemSubscene* subscene = nullptr;
    QVector3D worldPosition;

protected:
    cwRHIObject* createRHIObject() override;
};

// QML element placed in the 3D view. Builds a throwaway QML content item,
// references it so the scene graph materializes its node tree (refFromEffectItem),
// and registers a cwBillboardTestRender with the scene.
class cwBillboardTest : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BillboardTest)
    Q_PROPERTY(cwScene* scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_PROPERTY(QVector3D worldPosition READ worldPosition WRITE setWorldPosition NOTIFY worldPositionChanged)

public:
    explicit cwBillboardTest(QQuickItem* parent = nullptr);
    ~cwBillboardTest() override;

    cwScene* scene() const;
    void setScene(cwScene* scene);

    QVector3D worldPosition() const;
    void setWorldPosition(const QVector3D& position);

signals:
    void sceneChanged();
    void worldPositionChanged();

protected:
    void componentComplete() override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    void tryInitialize();

    QPointer<cwScene> m_scene;
    QVector3D m_worldPosition;

    QPointer<QQuickItem> m_content;
    QQmlComponent* m_component = nullptr;
    std::unique_ptr<cwQuickItemSubscene> m_subscene;
    cwBillboardTestRender* m_renderObject = nullptr;
    bool m_initialized = false;
};

inline cwScene* cwBillboardTest::scene() const { return m_scene; }
inline QVector3D cwBillboardTest::worldPosition() const { return m_worldPosition; }

#endif // CWBILLBOARDTEST_H
