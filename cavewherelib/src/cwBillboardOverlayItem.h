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
// needs identically: the scene property, fetching the scene-owned shared
// billboard layer (so all overlays sort back-to-front together, #538), rebinding
// that layer to the hosting window on a window change, and the add/update of one
// billboard through a cwBillboardHandle. Subclasses supply only what differs:
// where their billboards live (repositionBillboards) and each billboard's
// content, world position, and depth bias (setBillboard).

//Qt includes
#include <QQuickItem>
#include <QQmlEngine>
#include <QPointer>
#include <QVector3D>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwBillboardHandle.h"
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

public:
    explicit cwBillboardOverlayItem(QQuickItem* parent = nullptr);

    cwScene* scene() const;
    void setScene(cwScene* scene);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

signals:
    void sceneChanged();
    void cameraChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData& value) override;

    // Subclasses reproject their billboards onto the current camera here. Called
    // whenever the scene, hosting window, or visibility changes; subclasses also
    // wire their own camera-change signals to it.
    virtual void repositionBillboards() = 0;

    // The shared layer, or null until both a window and a scene are available.
    cwRenderBillboards* renderBillboards() const { return m_renderBillboards; }

    // The scene's shared text layer, or null until ensureRenderTextBillboards()
    // has fetched it. Only overlays that draw text fetch it, so only scenes with
    // such an overlay create one.
    cwRenderTextBillboards* renderTextBillboards() const { return m_renderTextBillboards; }

    // Fetches the scene's text layer and binds it to the hosting window once both
    // are available; returns it, or null while either is missing. Cheap to call
    // every update pass.
    cwRenderTextBillboards* ensureRenderTextBillboards();

    // Requests a frame of the 3D pass (a billboard's content faded or toggled).
    // A no-op until the shared layer exists.
    void requestBillboardRender();

    // Adds @a handle's billboard on the first call and updates it thereafter,
    // pushing it into the shared layer. ScreenConstant size (the only mode either
    // current overlay uses). A no-op when the layer or content is missing.
    void setBillboard(cwBillboardHandle& handle, QQuickItem* content,
                      const QVector3D& worldPosition, float depthBias);

    // Text counterpart of setBillboard: adds @a handle's label to the shared text
    // layer on the first call and updates it thereafter. A no-op until the text
    // layer exists.
    void setTextBillboard(cwTextBillboardHandle& handle, const QString& text,
                          const QVector3D& worldPosition, float depthBias);

private:
    void ensureRenderBillboards();

    QPointer<cwScene> m_scene;
    QPointer<cwCamera> m_camera;
    QPointer<cwRenderBillboards> m_renderBillboards; //!< shared, owned by m_scene
    QPointer<cwRenderTextBillboards> m_renderTextBillboards; //!< shared, owned by m_scene
};

#endif // CWBILLBOARDOVERLAYITEM_H
