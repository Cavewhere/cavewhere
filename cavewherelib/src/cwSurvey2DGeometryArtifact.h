// cwSurvey2DGeometryArtifact.h
#ifndef CWSURVEY2DGEOMETRYARTIFACT_H
#define CWSURVEY2DGEOMETRYARTIFACT_H

#include <QObject>
#include <QFuture>

//Our includes
#include "cwArtifact.h"
#include "cwSurvey2DGeometry.h"
#include "CaveWhereLibExport.h"

#include "Monad/Result.h"

class CAVEWHERE_LIB_EXPORT cwSurvey2DGeometryArtifact : public cwArtifact {
    Q_OBJECT
    Q_PROPERTY(QFuture<Monad::Result<cwSurvey2DGeometry>> geometryResult READ geometryResult WRITE setGeometryResult NOTIFY geometryResultChanged)

public:
    explicit cwSurvey2DGeometryArtifact(QObject* parent = nullptr);

    QFuture<Monad::Result<cwSurvey2DGeometry>> geometryResult() const;
    void setGeometryResult(const QFuture<Monad::Result<cwSurvey2DGeometry>>&);


signals:
    void geometryResultChanged();

private:
    QFuture<Monad::Result<cwSurvey2DGeometry>> m_geometryResult;

};

#endif // CWSURVEY2DGEOMETRYARTIFACT_H
