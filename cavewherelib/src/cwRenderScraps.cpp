#include "cwRenderScraps.h"
#include "cwScrap.h"
#include "cwRhiScraps.h"
#include "cwScene.h"

cwRenderScraps::cwRenderScraps(QObject *parent) :
    cwRenderObject(parent)
{
}

void cwRenderScraps::addScrapToUpdate(cwScrap *scrap)
{
    if(!scrap->triangulationData().isNull()) {

        auto triangleData = scrap->triangulationData();

        //This reduces the CPU memory, but this probably isn't the best place to do this
        //Probably need pipeline arch, with artifacts
        //scrap->setTriangulationData({}); //Clear the triangulation data

        PendingScrapCommand command = PendingScrapCommand(PendingScrapCommand::AddScrap,
                                                          scrap,
                                                          triangleData);

        addCommand(command);

        uint64_t scrapId = reinterpret_cast<uint64_t>(scrap);
        geometryItersecter()->addObject(cwGeometryItersecter::Object({this, scrapId},
                                                                     triangleData.scrapGeometry()));
    }
}

void cwRenderScraps::removeScrap(cwScrap *scrap)
{
    PendingScrapCommand command = PendingScrapCommand(PendingScrapCommand::RemoveScrap,
                                                      scrap,
                                                      cwTriangulatedData());

    addCommand(command);

    uint64_t scrapId = reinterpret_cast<uint64_t>(scrap);
    geometryItersecter()->removeObject(this, scrapId);
}

void cwRenderScraps::addScrapTextureToUpdate(cwScrap *scrap)
{
    PendingScrapCommand command = PendingScrapCommand(PendingScrapCommand::UpdateScrapTexture,
                                                      scrap,
                                                      scrap->triangulationData());

    addCommand(command);

}

cwRHIObject* cwRenderScraps::createRHIObject() {
    return new cwRhiScraps();
}

void cwRenderScraps::addCommand(const PendingScrapCommand &command)
{
    m_pendingChanges.append(command);
    update();
}
