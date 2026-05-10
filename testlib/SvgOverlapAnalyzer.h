/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef SVGOVERLAPANALYZER_H
#define SVGOVERLAPANALYZER_H

// Qt includes
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QRectF>
#include <QString>
#include <QUrl>

// Our includes
#include "CaveWhereTestLibExport.h"

class CAVEWHERE_TESTLIB_EXPORT SvgPassageOverlap
{
    Q_GADGET
    QML_NAMED_ELEMENT(svgPassageOverlap)
    Q_PROPERTY(QString text MEMBER text)
    Q_PROPERTY(QRectF rect MEMBER rect)
    Q_PROPERTY(int overlapPixels MEMBER overlapPixels)
    Q_PROPERTY(int totalPixels MEMBER totalPixels)
    Q_PROPERTY(double overlapPercent MEMBER overlapPercent)

public:
    QString text;
    QRectF  rect;
    int     overlapPixels = 0;
    int     totalPixels   = 0;
    double  overlapPercent = 0.0;
};
Q_DECLARE_METATYPE(SvgPassageOverlap)

class CAVEWHERE_TESTLIB_EXPORT SvgTextCollision
{
    Q_GADGET
    QML_NAMED_ELEMENT(svgTextCollision)
    Q_PROPERTY(QString textA MEMBER textA)
    Q_PROPERTY(QString textB MEMBER textB)
    Q_PROPERTY(QRectF rectA MEMBER rectA)
    Q_PROPERTY(QRectF rectB MEMBER rectB)
    Q_PROPERTY(double overlapArea MEMBER overlapArea)
    Q_PROPERTY(double overlapPercent MEMBER overlapPercent)

public:
    QString textA;
    QString textB;
    QRectF  rectA;
    QRectF  rectB;
    double  overlapArea    = 0.0;
    double  overlapPercent = 0.0;
};
Q_DECLARE_METATYPE(SvgTextCollision)

class CAVEWHERE_TESTLIB_EXPORT SvgOverlapAnalyzer : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SvgOverlap)
    QML_SINGLETON

public:
    explicit SvgOverlapAnalyzer(QObject* parent = nullptr);

    Q_INVOKABLE QList<SvgPassageOverlap> passageOverlaps(const QUrl& svgUrl) const;
    Q_INVOKABLE QList<SvgTextCollision>  textCollisions(const QUrl& svgUrl) const;
};

#endif // SVGOVERLAPANALYZER_H
