#ifndef CWCAPTUREITEMVIEWBOUNDS_H
#define CWCAPTUREITEMVIEWBOUNDS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QObjectBindableProperty>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwCaptureViewport.h"
#include "cwQuickSceneView.h"

/**
 * This class binds the cwQuickSceneView and cwCaptureViewport to update the rect and position.
 *
 * This allows the viewport or the scene view to change and the geometry update correctly
 */

class CAVEWHERE_LIB_EXPORT cwCaptureItemViewBounds : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CaptureItemViewBounds)

    Q_PROPERTY(cwCaptureViewport* captureViewport READ captureViewport WRITE setCaptureViewport NOTIFY captureViewportChanged FINAL)
    Q_PROPERTY(cwQuickSceneView* quickSceneView READ quickSceneView WRITE setQuickSceneView NOTIFY quickSceneViewChanged FINAL)

    Q_PROPERTY(QRectF captureViewRect READ captureViewRect NOTIFY captureViewRectChanged BINDABLE bindableCaptureViewRect)

public:
    cwCaptureItemViewBounds(QObject* parent = nullptr);

    QRectF captureViewRect() const { return m_captureViewRect.value(); }
    QBindable<QRectF> bindableCaptureViewRect() { return &m_captureViewRect; }

    cwCaptureViewport *captureViewport() const;
    void setCaptureViewport(cwCaptureViewport *newCaptureViewport);

    cwQuickSceneView *quickSceneView() const;
    void setQuickSceneView(cwQuickSceneView *newQuickSceneView);

signals:
    void captureViewportChanged();
    void quickSceneViewChanged();
    void captureViewRectChanged();
    void captureViewPositionChanged();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwCaptureItemViewBounds, QRectF, m_captureViewRect, QRectF(), &cwCaptureItemViewBounds::captureViewRectChanged);

    QPointer<cwCaptureViewport> m_captureViewport;
    QPointer<cwQuickSceneView> m_quickSceneView;

    void updateBindings();
};

#endif // CWCAPTUREITEMVIEWBOUNDS_H
