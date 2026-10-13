/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CAPTURELAYERINSPECTOR_H
#define CAPTURELAYERINSPECTOR_H

// Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

// Our includes
#include "CaveWhereTestLibExport.h"
#include "cwCaptureViewport.h"

class CAVEWHERE_TESTLIB_EXPORT CaptureLayerInspector : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CaptureLayerInspector)
    QML_SINGLETON

public:
    explicit CaptureLayerInspector(QObject* parent = nullptr);

    // Leads items in the layer's preview group: -1 if there is no preview,
    // otherwise how many exist and how many of those are visible.
    Q_INVOKABLE QVariantMap previewLeads(cwCaptureViewport* viewport) const;

    // Same shape as previewLeads(), for the preview's centerline items.
    Q_INVOKABLE QVariantMap previewCenterlines(cwCaptureViewport* viewport) const;

    // True once the preview's label placement has finished: the run reveals
    // the centerline item only after its worker is done.
    Q_INVOKABLE bool previewPlaced(cwCaptureViewport* viewport) const;
};

#endif // CAPTURELAYERINSPECTOR_H
