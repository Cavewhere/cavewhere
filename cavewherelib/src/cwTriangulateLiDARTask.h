#ifndef CWTRIANGULATELIDARTASK_H
#define CWTRIANGULATELIDARTASK_H

//Our includes
#include "cwTriangulateLiDARInData.h"
#include "cwRenderGLTF.h"
#include "cwRenderTexturedItems.h"
#include "cwProgressNode.h"

//Qt includes
#include <QFuture>
#include <QList>

//Monad includes
#include <Monad/Monad.h>

class cwTriangulateLiDARTask
{
public:
    cwTriangulateLiDARTask() = delete;

    //One future per input, in input order. `progressRoot` is the run's progress
    //tree: each note grows a node under it while it works, and a null root
    //leaves the task untracked.
    static QList<QFuture<Monad::Result<QVector<cwRenderTexturedItems::Item>>>> triangulate(
        const QList<cwTriangulateLiDARInData>& liDARs,
        const cwProgressNodePtr& progressRoot = {});
    static QVector<cwRenderTexturedItems::Item> reserveRenderItems(const QVector<cw::gltf::MeshCPU>& meshes);
};

#endif // CWTRIANGULATELIDARTASK_H
