#include "cwSurvey2DGeometrySource.h"

cwSurvey2DGeometrySource::cwSurvey2DGeometrySource(QObject* parent)
    : QObject(parent)
{
}

QFuture<Monad::Result<cwSurvey2DGeometry>> cwSurvey2DGeometrySource::geometryResult() const {
    return m_geometryResult;
}

void cwSurvey2DGeometrySource::setGeometryResult(const QFuture<Monad::Result<cwSurvey2DGeometry>>& result)
{
    m_geometryResult = result;
    emit geometryResultChanged();
}
