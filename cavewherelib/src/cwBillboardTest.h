#ifndef CWBILLBOARDTEST_H
#define CWBILLBOARDTEST_H

// =====================================================================
// PROTOTYPE / RESEARCH SPIKE for issues #535 (leads) and #536 (station
// labels). Throwaway code: it is the live, on-screen exercise of the real
// billboard path — it builds one QML content item and hands it to
// cwRenderBillboards, which renders it as a depth-occluded billboard inside
// the cwRhiScene 3D pass. Deleted in Phase 5 once cwLabel3dView /
// LeadPoint.qml drive cwRenderBillboards directly.
// =====================================================================

//Qt includes
#include <QQuickItem>
#include <QQmlEngine>
#include <QPointer>
#include <QVector3D>

class cwScene;
class cwRenderBillboards;
class QQmlComponent;

// QML element placed in the 3D view. Builds a throwaway QML content item and
// registers it with a cwRenderBillboards as a single billboard slot. The
// screenConstant property toggles between the two SizeMode behaviors so both
// can be verified on screen.
class cwBillboardTest : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BillboardTest)
    Q_PROPERTY(cwScene* scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_PROPERTY(QVector3D worldPosition READ worldPosition WRITE setWorldPosition NOTIFY worldPositionChanged)
    Q_PROPERTY(bool screenConstant READ screenConstant WRITE setScreenConstant NOTIFY screenConstantChanged)

public:
    explicit cwBillboardTest(QQuickItem* parent = nullptr);
    ~cwBillboardTest() override;

    cwScene* scene() const;
    void setScene(cwScene* scene);

    QVector3D worldPosition() const;
    void setWorldPosition(const QVector3D& position);

    bool screenConstant() const;
    void setScreenConstant(bool screenConstant);

signals:
    void sceneChanged();
    void worldPositionChanged();
    void screenConstantChanged();

protected:
    void componentComplete() override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    void tryInitialize();
    void updateBillboards();

    QPointer<cwScene> m_scene;
    QVector3D m_worldPosition;
    bool m_screenConstant = true;

    QPointer<QQuickItem> m_content;
    QQmlComponent* m_component = nullptr;
    cwRenderBillboards* m_renderObject = nullptr;
    bool m_initialized = false;
};

inline cwScene* cwBillboardTest::scene() const { return m_scene; }
inline QVector3D cwBillboardTest::worldPosition() const { return m_worldPosition; }
inline bool cwBillboardTest::screenConstant() const { return m_screenConstant; }

#endif // CWBILLBOARDTEST_H
