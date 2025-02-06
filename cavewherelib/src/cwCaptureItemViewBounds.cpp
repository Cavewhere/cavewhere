#include "cwCaptureItemViewBounds.h"

cwCaptureItemViewBounds::cwCaptureItemViewBounds(QObject *parent) {

}

cwCaptureViewport *cwCaptureItemViewBounds::captureViewport() const
{
    return m_captureViewport;
}

void cwCaptureItemViewBounds::setCaptureViewport(cwCaptureViewport *newCaptureViewport)
{
    if (m_captureViewport == newCaptureViewport)
        return;


    m_captureViewport = newCaptureViewport;
    updateBindings();
    emit captureViewportChanged();
}

cwQuickSceneView *cwCaptureItemViewBounds::quickSceneView() const
{
    return m_quickSceneView;
}

void cwCaptureItemViewBounds::setQuickSceneView(cwQuickSceneView *newQuickSceneView)
{
    if (m_quickSceneView == newQuickSceneView)
        return;


    m_quickSceneView = newQuickSceneView;
    updateBindings();
    emit quickSceneViewChanged();
}

void cwCaptureItemViewBounds::updateBindings()
{
    if(m_captureViewport && m_quickSceneView) {
        QBindable<QRectF> paperBoundingRect(m_captureViewport, "boundingBox");
        m_captureViewRect.setBinding([paperBoundingRect, this]() {
            //Since this calls bindable properties in m_quickSceneView,
            //this watches for changes in the view. This doesn't do the same
            //thing in qml, because this is c++ bindings
            return m_quickSceneView->toView(paperBoundingRect.value());
        });
    }
}
