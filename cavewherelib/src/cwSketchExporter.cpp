/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchExporter.h"
#include "cwSketchPainter.h"
#include "cwSketchDrawQPainter.h"
#include "cwSketchPathSource.h"
#include "cwScale.h"
#include "cwSketch.h"

//Qt includes
#include <QFile>
#include <QPaintDevice>
#include <QPainter>
#include <QPdfWriter>
#include <QSvgGenerator>

cwSketchExporter::cwSketchExporter(QObject *parent)
    : QObject(parent)
{
}

bool cwSketchExporter::exportPdf(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return exportPdf(&file);
}

bool cwSketchExporter::exportPdf(QIODevice *device)
{
    Q_ASSERT(device != nullptr);

    QPdfWriter writer(device);
    writer.setResolution(300);
    paintTo(&writer);
    return true;
}

bool cwSketchExporter::exportSvg(const QString &path)
{
    QSvgGenerator generator;
    generator.setFileName(path);
    paintTo(&generator);
    return true;
}

bool cwSketchExporter::exportSvg(QIODevice *device)
{
    Q_ASSERT(device != nullptr);

    QSvgGenerator generator;
    generator.setOutputDevice(device);
    paintTo(&generator);
    return true;
}

void cwSketchExporter::paintTo(QPaintDevice *device)
{
    Q_ASSERT(device != nullptr);

    QPainter painter(device);
    cwSketchDrawQPainter draw(&painter);

    const double paperScale = m_sketch ? m_sketch->mapScale()->scale()
                                       : 1.0 / 250.0;
    const int rawDpi = device->logicalDpiX();
    const double dpi = rawDpi > 0 ? double(rawDpi) : 96.0;

    cwSketchPainter::PaintContext ctx;
    ctx.worldToItem = cwSketchPainter::paperTransform(paperScale, dpi);
    ctx.mapScale    = paperScale;
    ctx.strokes     = m_strokeModel;
    ctx.linePlotTextScale = paperScale / cwSketchPainter::LinePlotReferenceMapScaleRatio;
    cwSketchPainter::paint(&draw, ctx);
}
