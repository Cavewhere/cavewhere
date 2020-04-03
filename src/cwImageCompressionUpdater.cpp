//Our includes
#include "cwImageCompressionUpdater.h"
#include "cwRegionTreeModel.h"
#include "cwOpenGLSettings.h"
#include "cwScrap.h"
#include "cwImageDatabase.h"
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwNote.h"
#include "cwAddImageTask.h"

//Aysnc future
#include "asyncfuture.h"

cwImageCompressionUpdater::cwImageCompressionUpdater(QObject *parent) : QObject(parent)
{
    Q_ASSERT(cwOpenGLSettings::instance());

    connect(cwOpenGLSettings::instance(), &cwOpenGLSettings::useDXT1CompressionChanged,
            this, &cwImageCompressionUpdater::updateAllImages);
}

cwRegionTreeModel* cwImageCompressionUpdater::regionTreeModel() const {
    return RegionTreeModel;
}

void cwImageCompressionUpdater::setRegionTreeModel(cwRegionTreeModel* regionTreeModel) {
    if(RegionTreeModel != regionTreeModel) {

        if(RegionTreeModel) {
            disconnect(RegionTreeModel, nullptr, this, nullptr);
        }

        RegionTreeModel = regionTreeModel;

        if(RegionTreeModel) {
            connect(RegionTreeModel, &cwRegionTreeModel::modelReset,
                    this, &cwImageCompressionUpdater::updateAllImages);
            connect(RegionTreeModel, &cwRegionTreeModel::rowsInserted,
                    this, [this](const QModelIndex& parent, int begin, int end)
            {
                QList<cwScrap*> scraps = RegionTreeModel->objects<cwScrap*>(parent,
                                                                            begin,
                                                                            end,
                                                                            &cwRegionTreeModel::scrap);
                recompressScraps(scraps);

                QList<cwNote*> notes = RegionTreeModel->objects<cwNote*>(parent,
                                                                         begin,
                                                                         end,
                                                                         &cwRegionTreeModel::note);
                recompressNotes(notes);
            });
        }

        emit regionTreeModelChanged();
    }

    updateAllImages();
}

void cwImageCompressionUpdater::setFutureToken(const cwFutureManagerToken &token)
{
    FutureToken = token;
}

cwFutureManagerToken cwImageCompressionUpdater::futureToken() const
{
    return FutureToken;
}

void cwImageCompressionUpdater::updateAllImages()
{
    recompressScraps(RegionTreeModel->all<cwScrap*>(QModelIndex(),
                                                    &cwRegionTreeModel::scrap));
    recompressNotes(RegionTreeModel->all<cwNote*>(QModelIndex(),
                                                  &cwRegionTreeModel::note));
}

bool cwImageCompressionUpdater::isSetup() const
{
    if(regionTreeModel()) {
        if(regionTreeModel()->cavingRegion()) {
            if(regionTreeModel()->cavingRegion()->parentProject()) {
                return true;
            }
        }
    }
    return false;
}

QString cwImageCompressionUpdater::filename() const
{
    return regionTreeModel()
            ->cavingRegion()
            ->parentProject()
            ->filename();
}

void cwImageCompressionUpdater::recompressNotes(QList<cwNote *> notes)
{
    bool usingCompression = cwOpenGLSettings::instance()->useDXT1Compression();

    if(isSetup() && !notes.isEmpty()) {
        QString filename = this->filename();
        cwImageDatabase database(filename);

        auto end = std::partition(notes.begin(), notes.end(),
                                  [&database, usingCompression](cwNote* note)
        {
            return !database.mipmapsValid(note->image(), usingCompression);
        });

        QList<QFuture<void>> noteFutures;
        noteFutures.reserve(std::distance(notes.begin(), end));

        std::transform(notes.begin(), end, std::back_inserter(noteFutures),
                       [filename](cwNote* note)
        {
            cwAddImageTask addImage;
            addImage.setRegenerateMipmapsOn(note->image());
            addImage.setDatabaseFilename(filename);
            addImage.setFormatType(cwTextureUploadTask::format());
            auto imageFuture = addImage.images();

            auto finalFuture = AsyncFuture::observe(imageFuture)
                    .context(note, [note, imageFuture]()
            {
                if(imageFuture.resultCount() == 1) {
                    auto trackedImage = imageFuture.result();
                    note->setImage(trackedImage->take());
                }
            }).future();

            return finalFuture;
        });

        auto combine = AsyncFuture::combine() << noteFutures;
        FutureToken.addJob({combine.future(), "Compressing"});
    }
}

void cwImageCompressionUpdater::recompressScraps(const QList<cwScrap *>& scraps)
{
    bool usingCompression = cwOpenGLSettings::instance()->useDXT1Compression();
    if(isSetup() && !scraps.isEmpty()) {
        cwImageDatabase database(filename());
        for(auto scrap : scraps) {
            //See if scrap
            auto image = scrap->triangulationData().croppedImage();
            if(!database.mipmapsValid(image, usingCompression)) {
                scrap->updateImage();
            }
        }
    }
}

