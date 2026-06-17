#include "cwBillboardTest.h"
#include "cwRenderBillboards.h"
#include "cwScene.h"

#include <QQuickWindow>
#include <QQmlComponent>
#include <QDebug>

namespace {

const char* kContentQml = R"QML(
import QtQuick
Rectangle {
    width: 180
    height: 56
    color: "#cc2f6e"
    radius: 8
    border.color: "white"
    border.width: 2
    Text {
        anchors.centerIn: parent
        text: "BILLBOARD"
        color: "white"
        font.pixelSize: 22
        font.bold: true
    }
}
)QML";

} // namespace

cwBillboardTest::cwBillboardTest(QQuickItem* parent)
    : QQuickItem(parent)
{
}

cwBillboardTest::~cwBillboardTest() = default;

void cwBillboardTest::setScene(cwScene* scene)
{
    if (m_scene == scene) {
        return;
    }
    m_scene = scene;
    emit sceneChanged();
    tryInitialize();
}

void cwBillboardTest::setWorldPosition(const QVector3D& position)
{
    if (m_worldPosition == position) {
        return;
    }
    m_worldPosition = position;
    updateBillboards();
    emit worldPositionChanged();
}

void cwBillboardTest::setScreenConstant(bool screenConstant)
{
    if (m_screenConstant == screenConstant) {
        return;
    }
    m_screenConstant = screenConstant;
    updateBillboards();
    emit screenConstantChanged();
}

void cwBillboardTest::componentComplete()
{
    QQuickItem::componentComplete();
    tryInitialize();
}

void cwBillboardTest::itemChange(ItemChange change, const ItemChangeData& value)
{
    if (change == ItemSceneChange) {
        tryInitialize();
    }
    QQuickItem::itemChange(change, value);
}

void cwBillboardTest::tryInitialize()
{
    if (m_initialized || !isComponentComplete()) {
        return;
    }

    QQuickWindow* quickWindow = window();
    QQmlEngine* engine = qmlEngine(this);
    if (!m_scene || !quickWindow || !engine) {
        return;
    }

    m_component = new QQmlComponent(engine, this);
    m_component->setData(kContentQml, QUrl());
    auto* content = qobject_cast<QQuickItem*>(m_component->create(qmlContext(this)));
    if (!content) {
        qWarning() << "cwBillboardTest: failed to create content item:"
                   << m_component->errorString();
        return;
    }
    content->setParent(this);
    m_content = content;

    m_renderObject = new cwRenderBillboards(this);
    m_renderObject->setWindow(quickWindow);
    m_renderObject->setScene(m_scene);
    updateBillboards();

    m_initialized = true;
}

void cwBillboardTest::updateBillboards()
{
    if (!m_renderObject || !m_content) {
        return;
    }

    cwRenderBillboards::Billboard billboard;
    billboard.content = m_content;
    billboard.worldPosition = m_worldPosition;
    billboard.sizeMode = m_screenConstant ? cwRenderBillboards::SizeMode::ScreenConstant
                                          : cwRenderBillboards::SizeMode::WorldSized;
    m_renderObject->setBillboards({billboard});
}
