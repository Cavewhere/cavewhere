#include "cwPlanScrapViewMatrix.h"

cwPlanScrapViewMatrix::cwPlanScrapViewMatrix(QObject* parent) :
    cwAbstractScrapViewMatrix(parent, new cwPlanScrapViewMatrix::Data())
{

}

const cwPlanScrapViewMatrix::Data *cwPlanScrapViewMatrix::data() const
{
    return static_cast<const cwPlanScrapViewMatrix::Data*>(cwAbstractScrapViewMatrix::data());
}

QMatrix4x4 cwPlanScrapViewMatrix::Data::matrix() const
{
    return QMatrix4x4();
}

cwPlanScrapViewMatrix::Data *cwPlanScrapViewMatrix::Data::clone() const
{
    return new cwPlanScrapViewMatrix::Data();
}

cwScrap::ScrapType cwPlanScrapViewMatrix::Data::type() const
{
    return cwScrap::Plan;
}
