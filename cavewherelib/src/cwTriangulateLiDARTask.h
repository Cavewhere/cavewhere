#ifndef CWTRIANGULATELIDARTASK_H
#define CWTRIANGULATELIDARTASK_H

//Our includes
#include "cwTriangulateLiDARInData.h"
#include "cwRenderGLTF.h"
#include "cwRenderTexturedItems.h"

//Qt includes
#include <QFuture>

//Monad includes
#include <Monad/Monad.h>

class cwTriangulateLiDARTask
{
public:
    cwTriangulateLiDARTask() = delete;

    static QFuture<Monad::Result<QVector<cwRenderTexturedItems::Item>>> triangulate(const QList<cwTriangulateLiDARInData>& liDARs);
    static QVector<cwRenderTexturedItems::Item> reserveRenderItems(const QVector<cw::gltf::MeshCPU>& meshes);
};

#endif // CWTRIANGULATELIDARTASK_H
