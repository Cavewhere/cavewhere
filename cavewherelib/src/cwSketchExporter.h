/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHEXPORTER_H
#define CWSKETCHEXPORTER_H

//Qt includes
#include <QIODevice>
#include <QObject>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"

class QPaintDevice;
class cwAbstractSketchPainterPathModel;
class cwSketch;

class CAVEWHERE_LIB_EXPORT cwSketchExporter : public QObject
{
    Q_OBJECT

public:
    explicit cwSketchExporter(QObject *parent = nullptr);

    void setStrokeModel(cwAbstractSketchPainterPathModel *model) { m_strokeModel = model; }
    cwAbstractSketchPainterPathModel *strokeModel() const { return m_strokeModel; }

    // Optional: supplies the map scale. When unset, paintTo falls back to 1:250.
    void setSketch(cwSketch *sketch) { m_sketch = sketch; }

    bool exportPdf(const QString &path);
    bool exportSvg(const QString &path);

    // Stream overloads for testing (avoids filesystem churn in parallel CI).
    bool exportPdf(QIODevice *device);
    bool exportSvg(QIODevice *device);

private:
    void paintTo(QPaintDevice *device);

    cwAbstractSketchPainterPathModel *m_strokeModel = nullptr;
    cwSketch *m_sketch = nullptr;
};

#endif // CWSKETCHEXPORTER_H
