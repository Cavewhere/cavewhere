#ifndef CWTRIANGULATELIDARTASK_H
#define CWTRIANGULATELIDARTASK_H

//Our includes
#include "cwTriangulateLiDARInData.h"
#include "cwRenderGLTF.h"
#include "cwRenderTexturedItems.h"
#include "cwProgressNode.h"

//Qt includes
#include <QFuture>

//Monad includes
#include <Monad/Monad.h>

class cwTriangulateLiDARTask
{
public:
    cwTriangulateLiDARTask() = delete;

    //`progressRoot` is the run's progress tree. Each note grows a node under it
    //while it works; a null root leaves the task untracked.
    static QFuture<Monad::Result<QVector<cwRenderTexturedItems::Item>>> triangulate(
        const QList<cwTriangulateLiDARInData>& liDARs,
        const cwProgressNodePtr& progressRoot = {});
    static QVector<cwRenderTexturedItems::Item> reserveRenderItems(const QVector<cw::gltf::MeshCPU>& meshes);
};

#endif // CWTRIANGULATELIDARTASK_H
