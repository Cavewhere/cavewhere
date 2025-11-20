#include "cwSurvey2DGeometryArtifact.h"


cwSurvey2DGeometryArtifact::cwSurvey2DGeometryArtifact(QObject* parent)
    : cwArtifact(parent) {
}

QFuture<Monad::Result<cwSurvey2DGeometry>> cwSurvey2DGeometryArtifact::geometryResult() const {
    return m_geometryResult;
}

void cwSurvey2DGeometryArtifact::setGeometryResult(const QFuture<Monad::Result<cwSurvey2DGeometry>>& result)
{
    m_geometryResult = result;
    emit geometryResultChanged();
}
