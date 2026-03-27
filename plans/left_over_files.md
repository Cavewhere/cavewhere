---
Testing plan: cave/trip rename/rename conflict

What the current code does (and doesn't do)

The cwCaveSyncMergeHandler applies a three-way merge that picks the winning cave name — but never scans the
dataRoot for orphaned peer cave directories and never calls enqueueConflictingProjectCleanup or anything
equivalent. After "ours wins", the peer's renamed cave directory (PeerCave/PeerCave.cwcave, PeerCave/trips/…)
remains on disk indefinitely.

The same gap exists in cwTripSyncMergeHandler for trip renames.

Scenario to test: cave rename/rename conflict

base:  cave "Conflict Cave"
ours:  cave renamed → "Author Cave"
peer:  cave renamed → "Peer Cave" (pushed)
sync:  ours wins, peer's cave directory must be cleaned up

Setup sequence:
1. Create a project with one cave "Conflict Cave", save, push to bare remote
2. Peer clones, renames the cave to "Peer Cave", pushes
3. Locally rename the cave to "Author Cave", sync

Assertions:
- cavingRegion()->cave(0)->name() == "Author Cave" (ours wins)
- QFileInfo::exists("<dataRoot>/Author Cave/Author Cave.cwcave") is true
- QFileInfo::exists("<dataRoot>/Peer Cave/Peer Cave.cwcave") is false — currently FAILS
- QFileInfo::exists("<dataRoot>/Peer Cave/") is false — currently FAILS
- The conflict-cleanup commit must not spuriously modify Author Cave.cwcave or any trip descriptors

Scenario to test: trip rename/rename conflict

Same structure but rename a trip instead of a cave. Verify the losing trip directory (<dataRoot>/<cave>/trips/Peer
 Trip/) is deleted.

What the fix would look like

cwCaveSyncMergeHandler needs to scan the current cave's parent directory for any cave descriptor whose ID does not
 match the winning cave's UUID, and enqueue their deletion — mirroring what cwCavingRegionSyncMergeHandler does
with enqueueConflictingProjectCleanup.

---
Now the test addition — checking the peer's data root directory is fully removed:
