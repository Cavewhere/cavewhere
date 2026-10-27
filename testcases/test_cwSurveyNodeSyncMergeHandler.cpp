/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch2 includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

//Our includes
#include "SurveyTreeSyncFixture.h"
#include "cwFixStationModel.h"

//Qt includes
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QPointer>

#include <algorithm>

using namespace SurveyTreeSyncFixture;

// ---------------------------------------------------------------------------
// A peer's edit of a depth-2 node's descriptor must land on the live node that
// carries that id, not on a node of the same name elsewhere in the tree.
// ---------------------------------------------------------------------------

TEST_CASE("A peer's depth-2 node rename lands on the live node with that id",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    auto clones = makeTwoClones(true);

    //--- Peer renames the depth-2 "Side Cave" and pushes ---
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerFolder = folderOf(peerRegion);
    cwCave* peerSideCave = childNamed(peerFolder, kSideCaveName);
    REQUIRE(peerSideCave != nullptr);
    peerSideCave->setName(kPeerSideCave);
    pushPeer(clones.get());

    //--- Author renames the sibling locally, so the sync is a real merge ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorFolder = folderOf(authorRegion);
    QPointer<cwCave> authorSideCave = childNamed(authorFolder, kSideCaveName);
    QPointer<cwCave> authorSibling = childNamed(authorFolder, kSiblingCaveName);
    REQUIRE(authorSideCave != nullptr);
    REQUIRE(authorSibling != nullptr);
    authorSibling->setName(kAuthorSibling);
    clones->authorProject()->waitSaveToFinish();
    REQUIRE(isProjectModified(clones->authorProject()));

    syncAuthor(clones.get());

    //The peer's rename landed on the same live object, still at depth two.
    REQUIRE(authorSideCave != nullptr);
    CHECK(authorSideCave->name() == kPeerSideCave);
    CHECK(authorSideCave->parentNode() == authorFolder);

    REQUIRE(authorSibling != nullptr);
    CHECK(authorSibling->name() == kAuthorSibling);

    const QDir dataRoot = clones->authorDataRoot();
    const QDir folderDir(dataRoot.absoluteFilePath(kFolderName));
    CHECK(QFileInfo::exists(folderDir.absoluteFilePath(
        QStringLiteral("sub/") + kPeerSideCave + QChar('/') + kPeerSideCave
        + QStringLiteral(".cwcave"))));
}

// ---------------------------------------------------------------------------
// Orphan cleanup after a depth-2 rename/rename conflict removes the losing
// node's own directory and nothing above it — a sibling's notes survive.
// ---------------------------------------------------------------------------

TEST_CASE("Depth-2 orphan cleanup removes only the losing node's directory",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    auto clones = makeTwoClones(true);

    //--- Peer renames the depth-2 "Side Cave" and pushes ---
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerFolder = folderOf(peerRegion);
    cwCave* peerSideCave = childNamed(peerFolder, kSideCaveName);
    REQUIRE(peerSideCave != nullptr);
    peerSideCave->setName(kPeerSideCave);
    pushPeer(clones.get());

    //--- Author renames the same node differently, then syncs: ours wins ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorFolder = folderOf(authorRegion);
    QPointer<cwCave> authorSideCave = childNamed(authorFolder, kSideCaveName);
    REQUIRE(authorSideCave != nullptr);
    authorSideCave->setName(kAuthorSideCave);
    clones->authorProject()->waitSaveToFinish();
    REQUIRE(isProjectModified(clones->authorProject()));

    syncAuthor(clones.get());

    REQUIRE(authorSideCave != nullptr);
    CHECK(authorSideCave->name() == kAuthorSideCave);

    const QDir dataRoot = clones->authorDataRoot();
    const QDir folderDir(dataRoot.absoluteFilePath(kFolderName));
    const QDir subDir(folderDir.absoluteFilePath(QStringLiteral("sub")));

    //The winner keeps its directory and descriptor.
    CHECK(QFileInfo::exists(subDir.absoluteFilePath(
        kAuthorSideCave + QChar('/') + kAuthorSideCave + QStringLiteral(".cwcave"))));

    //The loser's own directory is gone.
    CHECK_FALSE(QFileInfo::exists(subDir.absoluteFilePath(kPeerSideCave)));

    //Nothing above it was taken with it: the ancestor, the sibling node, its trip,
    //and its note image are all still on disk.
    CHECK(folderDir.exists());
    CHECK(QFileInfo::exists(folderDir.absoluteFilePath(kFolderName + QStringLiteral(".cwcave"))));
    const QDir siblingDir(subDir.absoluteFilePath(kSiblingCaveName));
    CHECK(siblingDir.exists());
    CHECK(QFileInfo::exists(
        siblingDir.absoluteFilePath(kSiblingCaveName + QStringLiteral(".cwcave"))));
    CHECK(noteImageFiles(siblingDir, kSiblingTripName).size() == 1);
}

// ---------------------------------------------------------------------------
// A peer moves a node's directory while the author edits an unrelated node. The
// moved directory is the only copy of that node's descriptor, so orphan cleanup
// must leave it alone even though it no longer sits where the live node's path
// says it does. A Move is not a save op until C3.3, so the peer's move is made
// by hand in the clone's working tree.
// ---------------------------------------------------------------------------

TEST_CASE("Orphan cleanup spares a moved node's only directory",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    auto clones = makeTwoClones(false);

    //--- Peer moves Kentucky field seasons/sub/Side Cave up to the data root ---
    const QDir peerDataRoot(clones->cloneRepository.directory().absoluteFilePath(kProjectName));
    const QString movedFrom =
        peerDataRoot.absoluteFilePath(kFolderName + QStringLiteral("/sub/") + kSideCaveName);
    const QString movedTo = peerDataRoot.absoluteFilePath(kSideCaveName);
    REQUIRE(QFileInfo::exists(movedFrom));
    REQUIRE(QDir().rename(movedFrom, movedTo));

    clones->cloneRepository.commitAll(QStringLiteral("Move Side Cave to the data root"), QString());
    auto pushFuture = clones->cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(pushFuture, 10000));
    INFO("Push error: " << pushFuture.result().errorMessage().toStdString());
    REQUIRE(!pushFuture.result().hasError());

    //--- Author fixes a station on an unrelated node, then syncs ---
    //A descriptor-only edit: no note or trip file changes, so no other handler falls
    //back to a full reload and the node handler's orphan cleanup really runs.
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorFolder = folderOf(authorRegion);
    cwCave* authorSibling = childNamed(authorFolder, kSiblingCaveName);
    REQUIRE(authorSibling != nullptr);

    cwFixStation fixStation;
    fixStation.setStationName(QStringLiteral("a1"));
    fixStation.setEasting(600000.0);
    fixStation.setNorthing(4430000.0);
    fixStation.setElevation(1655.0);
    authorSibling->fixStations()->appendFixStation(fixStation);
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    //The moved node's descriptor is still on disk. The live node's own directory holds
    //no descriptor, so the moved directory is the only copy and is never removed.
    const QDir dataRoot = clones->authorDataRoot();
    QStringList descriptors;
    QDirIterator files(dataRoot.absolutePath(),
                       QDir::Files | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString relativePath = dataRoot.relativeFilePath(files.next());
        if (relativePath.endsWith(QStringLiteral(".cwcave"))) {
            descriptors.append(relativePath);
        }
    }

    INFO("Descriptors under the data root: " << descriptors.join(QStringLiteral(", ")).toStdString());
    CHECK(descriptors.contains(kSideCaveName + QChar('/') + kSideCaveName
                               + QStringLiteral(".cwcave")));
}
