/**************************************************************************
**
**    Copyright (C) 2024 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBILLBOARDOVERLAYITEM_H
#define CWBILLBOARDOVERLAYITEM_H

// Shared base for QQuickItems that render their content as depth-occluded
// billboards in the scene's 3D pass (cwLeadView's lead markers, cwLabel3dView's
// station labels, any future overlay). It owns the wiring every such overlay
// needs identically: the scene property, fetching the scene-owned billboard
// layers (the shared item layer, and the sprite and text layers of its layer
// key, #538), rebinding those layers to the hosting window on a window change, and the add/update of one billboard through a handle.
// Each layer is fetched the first time an overlay asks for it, so a scene only
// carries the layers its overlays draw with. Subclasses supply only what
// differs: where their billboards live (repositionBillboards) and each
// billboard's content, world position, and depth bias (setBillboard and its
// sprite and text counterparts).

//Qt includes
#include <QQuickItem>
#include <QQmlEngine>
#include <QPointer>
#include <QQmlListProperty>
#include <QSizeF>
#include <QString>
#include <QVector3D>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwBillboardHandle.h"
#include "cwSpriteBillboardHandle.h"
#include "cwTextBillboardHandle.h"

class cwScene;
class cwCamera;

class CAVEWHERE_LIB_EXPORT cwBillboardOverlayItem : public QQuickItem
{
    Q_OBJECT
    // Not creatable from QML: it's the shared base for LeadView / Label3dView,
    // which inherit its scene and camera properties. QML_ANONYMOUS registers it
    // so those inherited properties participate explicitly in the QML type system.
    QML_ANONYMOUS

    Q_PROPERTY(cwScene* scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    // The atlas slots this overlay's sprite instances draw, handed to its keyed
    // sprite layer. Slot order is the atlas order. All slots share spriteSize
    // (slot 0's size), the size of each instance's tap target. A marker needing
    // its own size or live content belongs on cwRenderBillboards, see setBillboard.
    Q_PROPERTY(QQmlListProperty<QQuickItem> sprites READ sprites NOTIFY spritesChanged)
    Q_PROPERTY(QSizeF spriteSize READ spriteSize NOTIFY spriteSizeChanged)

public:
    explicit cwBillboardOverlayItem(QQuickItem* parent = nullptr);
    // @a layerKey picks the scene's sprite and text layers this overlay draws
    // through (cwScene::spriteBillboardLayer(key)); the empty key is the default pair.
    // A key belongs to one overlay at a time: the sprite layer holds one overlay's slots.
    explicit cwBillboardOverlayItem(const QString& layerKey, QQuickItem* parent = nullptr);
    ~cwBillboardOverlayItem() override;

    QString layerKey() const;

    QQmlListProperty<QQuickItem> sprites();
    QSizeF spriteSize() const;

    cwScene* scene() const;
    void setScene(cwScene* scene);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

signals:
    void sceneChanged();
    void cameraChanged();
    void spritesChanged();
    void spriteSizeChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void componentComplete() override;

    // Subclasses reproject their billboards onto the current camera here. Called
    // whenever the scene, hosting window, or visibility changes; subclasses also
    // wire their own camera-change signals to it.
    virtual void repositionBillboards() = 0;

    // The scene's shared item layer, or null until setBillboard() has fetched
    // it. Only overlays that draw QQuickItem content fetch it, so a scene whose
    // overlays draw only sprites and text has an empty item layer at most (the
    // text layer pulls it in for draw order) and no per-item renderer.
    cwRenderBillboards* renderBillboards() const { return m_renderBillboards; }

    // The scene's shared sprite layer, or null until ensureRenderSpriteBillboards()
    // has fetched it.
    cwRenderSpriteBillboards* renderSpriteBillboards() const { return m_renderSpriteBillboards; }

    // The scene's shared text layer, or null until ensureRenderTextBillboards()
    // has fetched it. Only overlays that draw text fetch it, so only scenes with
    // such an overlay create one.
    cwRenderTextBillboards* renderTextBillboards() const { return m_renderTextBillboards; }

    // Fetches the scene's sprite layer and binds it to the hosting window once
    // both are available; returns it, or null while either is missing. Cheap to
    // call every update pass.
    cwRenderSpriteBillboards* ensureRenderSpriteBillboards();

    // Fetches the scene's text layer and binds it to the hosting window once both
    // are available; returns it, or null while either is missing. Cheap to call
    // every update pass.
    cwRenderTextBillboards* ensureRenderTextBillboards();

    // Adds @a handle's billboard on the first call and updates it thereafter,
    // pushing it into the shared item layer, which is fetched here on first use.
    // ScreenConstant size. A no-op while the window or scene is missing, or when
    // content is null.
    void setBillboard(cwBillboardHandle& handle, QQuickItem* content,
                      const QVector3D& worldPosition, float depthBias);

    // Sprite counterpart of setBillboard: adds @a handle's instance to the shared
    // sprite layer, which is fetched here on first use, and updates it thereafter.
    // A no-op while the window or scene is missing. A hidden instance stays in
    // the layer and is left out of the draw. @a spriteIndex picks the layer's
    // atlas slot (cwRenderSpriteBillboards::setSprites).
    void setSpriteBillboard(cwSpriteBillboardHandle& handle, const QVector3D& worldPosition,
                            float depthBias, float opacity, bool visible, int spriteIndex = 0);

    // Text counterpart of setBillboard: adds @a handle's label to the shared text
    // layer on the first call and updates it thereafter. A no-op until the text
    // layer exists.
    void setTextBillboard(cwTextBillboardHandle& handle, const QString& text,
                          const QVector3D& worldPosition, float depthBias);

private:
    cwRenderBillboards* ensureRenderBillboards();

    static void appendSprite(QQmlListProperty<QQuickItem>* list, QQuickItem* sprite);
    static qsizetype spriteCount(QQmlListProperty<QQuickItem>* list);
    static QQuickItem* spriteAt(QQmlListProperty<QQuickItem>* list, qsizetype index);
    static void clearSprites(QQmlListProperty<QQuickItem>* list);
    void spritesEdited();

    // Hands the sprite slots to the keyed sprite layer once the scene and window
    // exist, taking them back from the layer of a previous scene first so that
    // layer drops its subscenes and textures of them. Returns early unless the
    // list was edited or the layer changed, so a destroyed sprite keeps its slot
    // in the layer. An overlay that has never listed a sprite fetches no layer.
    void pushSpritesToLayer();

    QString m_layerKey;
    QList<QPointer<QQuickItem>> m_sprites;
    QPointer<cwRenderSpriteBillboards> m_spriteLayer; //!< the layer m_sprites was last handed to
    bool m_spritesEdited = true; //!< m_sprites changed since it was last handed to m_spriteLayer
    QPointer<cwScene> m_scene;
    QPointer<cwCamera> m_camera;
    QPointer<cwRenderBillboards> m_renderBillboards; //!< shared, owned by m_scene
    QPointer<cwRenderSpriteBillboards> m_renderSpriteBillboards; //!< shared, owned by m_scene
    QPointer<cwRenderTextBillboards> m_renderTextBillboards; //!< shared, owned by m_scene
};

#endif // CWBILLBOARDOVERLAYITEM_H
