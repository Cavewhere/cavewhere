#include "cwAbstractNoteTransformation.h"
#include "cwScale.h"
#include "cwLength.h"
#include "cwImageResolution.h"
#include "cwMath.h"

// Qt
#include <QVector2D>
#include <QLineF>
#include <QtGlobal>

// Std
#include <cmath>

cwAbstractNoteTransformation::cwAbstractNoteTransformation(QObject* parent)
    : QObject(parent)
    , Scale(new cwScale(this))
{
    connect(Scale, &cwScale::scaleChanged, this, &cwAbstractNoteTransformation::scaleChanged);
}

cwLength* cwAbstractNoteTransformation::scaleDenominator() const { return Scale->scaleDenominator(); }
cwLength* cwAbstractNoteTransformation::scaleNumerator() const   { return Scale->scaleNumerator(); }


void cwAbstractNoteTransformation::setScale(double s) { Scale->setScale(s); }
double cwAbstractNoteTransformation::scale() const     { return Scale->scale(); }

void cwAbstractNoteTransformation::setData(const cwNoteTransformationData& d)
{
    setNorthUp(d.north);
    Scale->setData(d.scale);
}

cwNoteTransformationData cwAbstractNoteTransformation::data() const
{
    return { m_northUp, Scale->data() };
}

double cwAbstractNoteTransformation::wrapDegrees360(double degrees) const
{
    return cwWrapDegrees360(degrees);
}
