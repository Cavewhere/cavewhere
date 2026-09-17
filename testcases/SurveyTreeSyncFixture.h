/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#pragma once

//Catch2 includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "LoadProjectHelper.h"
#include "SurveyTreeTestHelper.h"
#include "TestHelper.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwErrorListModel.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "GitRepository.h"
#include "asyncfuture.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrl>

//libgit2
#include "git2.h"

#include <memory>

//! The two-clone git fixture the survey-tree sync cases share: a depth-2 project saved,
//! pushed to a bare remote, and cloned for a peer.
namespace SurveyTreeSyncFixture {

inline const QString kProjectName = QStringLiteral("NodeTreeProject");
inline const QString kFolderName = QStringLiteral("Kentucky field seasons");
inline const QString kSideCaveName = QStringLiteral("Side Cave");
inline const QString kSiblingCaveName = QStringLiteral("Sibling Cave");
inline const QString kSiblingTripName = QStringLiteral("Sibling survey");
inline const QString kAuthorSideCave = QStringLiteral("Author Side Cave");
inline const QString kPeerSideCave = QStringLiteral("Peer Side Cave");
inline const QString kAuthorSibling = QStringLiteral("Author Sibling");
inline const QString kSectionName = QStringLiteral("Upper level");
inline const QString kSectionTripName = QStringLiteral("Dome climb");
inline const QString kAuthorSection = QStringLiteral("Author Upper level");
inline const QString kPeerSection = QStringLiteral("Peer Upper level");

//! The depth-2 project both sides of every case below start from:
//!
//!   Kentucky field seasons          (Folder)
//!     nodes/Side Cave               (Cave)
//!     nodes/Sibling Cave            (Cave, one trip carrying a note image)
//!
//! With a section asked for, Side Cave gains one more level:
//!
//!     nodes/Side Cave/nodes/Upper level   (Folder, one trip carrying a note image)
struct TreeFixture {
    cwCave* folder = nullptr;
    cwCave* sideCave = nullptr;
    cwCave* siblingCave = nullptr;
    cwCave* section = nullptr;
};

using SurveyTreeTestHelper::addNode;

//! \a withSection adds the depth-3 node the depth-N sync cases need.
inline TreeFixture buildTree(cwRootData* rootData, bool withSection = false)
{
    auto* region = rootData->project()->cavingRegion();
    region->setName(kProjectName);

    TreeFixture fixture;
    fixture.folder = addNode(region, nullptr, cwSurveyNode::Kind::Folder, kFolderName);
    fixture.sideCave = addNode(region, fixture.folder, cwSurveyNode::Kind::Cave, kSideCaveName);
    fixture.siblingCave = addNode(region, fixture.folder, cwSurveyNode::Kind::Cave, kSiblingCaveName);

    fixture.siblingCave->addTrip();
    cwTrip* siblingTrip = fixture.siblingCave->trip(0);
    REQUIRE(siblingTrip != nullptr);
    siblingTrip->setName(kSiblingTripName);

    const QString noteImagePath =
        copyToTempFolder(testcasesDatasetPath("test_cwAddImageTask/supportedImage.png"));
    siblingTrip->notes()->addFromFiles({QUrl::fromLocalFile(noteImagePath)});
    rootData->futureManagerModel()->waitForFinished();
    REQUIRE(siblingTrip->notes()->rowCount() == 1);

    if (withSection) {
        fixture.section = addNode(region, fixture.sideCave, cwSurveyNode::Kind::Folder, kSectionName);

        fixture.section->addTrip();
        cwTrip* sectionTrip = fixture.section->trip(0);
        REQUIRE(sectionTrip != nullptr);
        sectionTrip->setName(kSectionTripName);

        const QString sectionImagePath =
            copyToTempFolder(testcasesDatasetPath("test_cwAddImageTask/supportedImage.png"));
        sectionTrip->notes()->addFromFiles({QUrl::fromLocalFile(sectionImagePath)});
        rootData->futureManagerModel()->waitForFinished();
        REQUIRE(sectionTrip->notes()->rowCount() == 1);
    }

    return fixture;
}

//! Author and peer, each with their own working copy of the same pushed project.
struct TwoClones {
    //The temporary directories are declared first so they outlive both projects: members
    //are destroyed in reverse order, and a project tearing down against a deleted working
    //tree flushes saves into directories that are already gone.
    QTemporaryDir authorProjectDir;
    QTemporaryDir remoteRoot;
    QTemporaryDir cloneDir;
    std::unique_ptr<cwRootData> authorRootData;
    std::unique_ptr<cwRootData> peerRootData;
    QQuickGit::GitRepository cloneRepository;
    QString projectPath;

    cwProject* authorProject() const { return authorRootData->project(); }
    cwProject* peerProject() const { return peerRootData->project(); }

    //! <repoRoot>/<dataRoot> of the author's working copy.
    QDir authorDataRoot() const
    {
        const QDir repoRoot = QFileInfo(authorProject()->filename()).absoluteDir();
        return QDir(repoRoot.absoluteFilePath(kProjectName));
    }
};

//! Saves the fixture project, pushes it to a bare remote, and clones it for the peer.
//! \a loadPeerProject opens the clone in a second cwProject; a case that edits the clone's
//! working tree by hand leaves it closed.
inline std::unique_ptr<TwoClones> makeTwoClones(bool loadPeerProject, bool withSection = false)
{
    auto clones = std::make_unique<TwoClones>();
    REQUIRE(clones->authorProjectDir.isValid());
    REQUIRE(clones->remoteRoot.isValid());
    REQUIRE(clones->cloneDir.isValid());

    clones->authorRootData = std::make_unique<cwRootData>();
    clones->authorRootData->account()->setName(QStringLiteral("Author User"));
    clones->authorRootData->account()->setEmail(QStringLiteral("author@example.com"));

    buildTree(clones->authorRootData.get(), withSection);

    auto* authorProject = clones->authorProject();
    clones->projectPath = QDir(clones->authorProjectDir.path())
                              .filePath(kProjectName + QStringLiteral(".cwproj"));
    REQUIRE(authorProject->saveAs(clones->projectPath));
    authorProject->waitSaveToFinish();

    const QString remoteRepoPath = QDir(clones->remoteRoot.path()).filePath(QStringLiteral("remote.git"));
    REQUIRE(initBareRepo(remoteRepoPath) == GIT_OK);
    REQUIRE(authorProject->repository()
                ->addRemote(QStringLiteral("origin"), QUrl::fromLocalFile(remoteRepoPath))
                .isEmpty());

    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    clones->authorRootData->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();
    CHECK(authorProject->errorModel()->count() == 0);

    const QString clonePath = QDir(clones->cloneDir.path()).filePath(QStringLiteral("peer-clone"));
    clones->cloneRepository.setDirectory(QDir(clonePath));
    clones->cloneRepository.setAccount(clones->authorRootData->account());

    auto cloneFuture = clones->cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error: " << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    if (loadPeerProject) {
        clones->peerRootData = std::make_unique<cwRootData>();
        clones->peerRootData->account()->setName(QStringLiteral("Peer User"));
        clones->peerRootData->account()->setEmail(QStringLiteral("peer@example.com"));

        auto* peerProject = clones->peerProject();
        const QString peerProjectPath =
            QDir(clonePath).filePath(QFileInfo(clones->projectPath).fileName());
        REQUIRE(QFileInfo::exists(peerProjectPath));
        peerProject->loadFile(peerProjectPath);
        peerProject->waitLoadToFinish();
        peerProject->waitSaveToFinish();

        REQUIRE(peerProject->repository() != nullptr);
        peerProject->repository()->setAccount(clones->peerRootData->account());
    }

    return clones;
}

inline void pushPeer(TwoClones* clones)
{
    auto* peerProject = clones->peerProject();
    peerProject->waitSaveToFinish();
    REQUIRE(isProjectModified(peerProject));

    peerProject->errorModel()->clear();
    REQUIRE(peerProject->sync());
    clones->peerRootData->futureManagerModel()->waitForFinished();
    peerProject->waitSaveToFinish();
    CHECK(peerProject->errorModel()->count() == 0);
}

inline void syncAuthor(TwoClones* clones)
{
    auto* authorProject = clones->authorProject();
    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    clones->authorRootData->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();
    CHECK(authorProject->errorModel()->count() == 0);
}

inline cwCave* childNamed(const cwSurveyNode* parent, const QString& name)
{
    const QList<cwSurveyNode*> children = parent->childNodes();
    for (cwSurveyNode* child : children) {
        if (child->name() == name) {
            return qobject_cast<cwCave*>(child);
        }
    }
    return nullptr;
}

//! The "Kentucky field seasons" folder every case starts from.
inline cwCave* folderOf(const cwCavingRegion* region)
{
    cwCave* folder = childNamed(region->rootNode(), kFolderName);
    REQUIRE(folder != nullptr);
    return folder;
}

//! The image files sitting in \a trip's notes directory on disk.
inline QStringList noteImageFiles(const QDir& nodeDir, const QString& tripName)
{
    const QDir notesDir(nodeDir.absoluteFilePath(
        QStringLiteral("trips/") + tripName + QStringLiteral("/notes")));
    return notesDir.entryList({QStringLiteral("*.png")}, QDir::Files);
}

} // namespace SurveyTreeSyncFixture
