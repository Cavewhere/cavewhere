/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWNOTETRANFORMATION_H
#define CWNOTETRANFORMATION_H

//Qt includes
#include <QSharedData>
#include <QSharedDataPointer>
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QMatrix4x4>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
#include "cwAbstractNoteTransformation.h"
#include "cwImage.h"
class cwLength;
class cwImageResolution;
class cwScale;

/**
  This class is useful for allowing automatic station referencing.  This holds the page of note's
  scale (example 40:1 and rotation that rotates the page such that north is up, ie. parallel the y axis.

  The scale can be set throught the scaleNumerator : scaleDenominator.  When one of those change, the
  unitless scale will also change.
  */
class CAVEWHERE_LIB_EXPORT cwNoteTranformation : public cwAbstractNoteTransformation
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NoteTranformation)

public:
    cwNoteTranformation(QObject* parent = 0);

    Q_INVOKABLE double calculateNorth(QPointF noteP1, QPointF noteP2) const;
    Q_INVOKABLE double calculateScale(QPointF noteP1, QPointF noteP2,
                                      cwLength* length,
                                      QSize imageSize,
                                      cwImageResolution* resolution);
    Q_INVOKABLE double calculateScaleForRendered(QPointF noteP1, QPointF noteP2,
                                                 cwLength* length,
                                                 const cwImage& image,
                                                 const QSize& renderedSize,
                                                 cwImageResolution* resolution);

    QMatrix4x4 matrix() const override;
};



#endif // CWNOTETRANFORMATION_H
