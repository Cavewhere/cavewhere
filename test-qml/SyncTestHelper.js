.pragma library

function isArrayLike(value) {
    return value !== null
           && value !== undefined
           && typeof value !== "string"
           && typeof value.length === "number"
           && value.length >= 0
           && Math.floor(value.length) === value.length
}

function toArray(value) {
    let result = []
    for (let i = 0; i < value.length; ++i) {
        result.push(value[i])
    }
    return result
}

function deepEqual(lhs, rhs) {
    if (lhs === rhs) {
        return true
    }

    if (lhs === null || rhs === null || lhs === undefined || rhs === undefined) {
        return lhs === rhs
    }

    if (typeof lhs !== typeof rhs) {
        return false
    }

    if (isArrayLike(lhs) || isArrayLike(rhs)) {
        if (!isArrayLike(lhs) || !isArrayLike(rhs) || lhs.length !== rhs.length) {
            return false
        }
        lhs = toArray(lhs)
        rhs = toArray(rhs)
        for (let i = 0; i < lhs.length; ++i) {
            if (!deepEqual(lhs[i], rhs[i])) {
                return false
            }
        }
        return true
    }

    if (typeof lhs === "object") {
        let lhsKeys = Object.keys(lhs).sort()
        let rhsKeys = Object.keys(rhs).sort()
        if (!deepEqual(lhsKeys, rhsKeys)) {
            return false
        }
        for (let i = 0; i < lhsKeys.length; ++i) {
            let key = lhsKeys[i]
            if (!deepEqual(lhs[key], rhs[key])) {
                return false
            }
        }
        return true
    }

    return false
}

function tryVerifyWithDiagnostics(testCase, predicate, timeoutMs, label, onPending) {
    let startMs = Date.now()
    let attempts = 0
    while (Date.now() - startMs < timeoutMs) {
        if (predicate()) {
            return
        }
        attempts += 1
        if (onPending !== undefined && onPending !== null && attempts % 20 === 0) {
            onPending(attempts, Date.now() - startMs)
        }
        testCase.wait(50)
    }
    if (onPending !== undefined && onPending !== null) {
        onPending(attempts, Date.now() - startMs)
    }
    testCase.verify(false, label + " timed out after " + timeoutMs + "ms")
}

function openTripPage(testCase, rootData, caveName, tripName) {
    rootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + caveName + "/Trip=" + tripName
    tryVerifyWithDiagnostics(testCase, () => {
        return rootData.pageView.currentPageItem !== null
               && rootData.pageView.currentPageItem.objectName === "tripPage"
    }, 10000, "openTripPage")
}

function verifyStillOnTripPage(testCase, rootData, tripPageAddress) {
    tryVerifyWithDiagnostics(testCase, () => {
        if (rootData.pageSelectionModel.currentPageAddress !== tripPageAddress) {
            rootData.pageSelectionModel.currentPageAddress = tripPageAddress
        }
        return rootData.pageView.currentPageItem !== null
               && rootData.pageSelectionModel.currentPageAddress === tripPageAddress
               && rootData.pageView.currentPageItem.objectName === "tripPage"
    }, 10000, "verifyStillOnTripPage")
}

function loadFixtureAndOpenFirstTrip(testCase, rootData, testHelper) {
    let fixture = testHelper.createLocalSyncFixtureWithLfsServer()
    testCase.compare(fixture.errorMessage, "")
    testCase.verify(fixture.projectFilePath !== "")
    testCase.verify(fixture.remoteRepoPath !== "")
    testCase.verify(fixture.lfsEndpoint !== "")
    testCase.compare(testHelper.fileExists(testHelper.toLocalUrl(fixture.projectFilePath)), true)

    testHelper.loadProjectFromPath(rootData.project, fixture.projectFilePath)
    testCase.tryVerify(() => {
        return rootData.region.caveCount > 0
    }, 10000)

    let cave = null
    let trip = null
    for (let i = 0; i < rootData.region.caveCount; ++i) {
        let candidateCave = rootData.region.cave(i)
        if (candidateCave !== null && candidateCave.rowCount() > 0) {
            cave = candidateCave
            trip = candidateCave.trip(0)
            break
        }
    }

    testCase.verify(cave !== null)
    testCase.verify(trip !== null)

    let caveName = String(cave.name)
    let tripName = String(trip.name)
    testCase.verify(caveName.length > 0)
    testCase.verify(tripName.length > 0)

    openTripPage(testCase, rootData, caveName, tripName)

    return {
        tripPageAddress: "Source/Data/Cave=" + caveName + "/Trip=" + tripName
    }
}

function waitForProjectSyncToFinish(testCase, rootData) {
    tryVerifyWithDiagnostics(testCase, () => {
        return rootData.project.syncInProgress === false
    }, 3000, "waitForProjectSyncToFinish")
}

function runProjectSyncRoundTrip(testCase, rootData, testHelper, options) {
    const verifyEditedValueTimeoutMs = options.verifyEditedValueTimeoutMs !== undefined
                                     ? options.verifyEditedValueTimeoutMs
                                     : 3000
    const verifyBaselineAfterCheckoutTimeoutMs = options.verifyBaselineAfterCheckoutTimeoutMs !== undefined
                                               ? options.verifyBaselineAfterCheckoutTimeoutMs
                                               : 3000
    const verifyResyncedValueTimeoutMs = options.verifyResyncedValueTimeoutMs !== undefined
                                       ? options.verifyResyncedValueTimeoutMs
                                       : 3000
    const verifyBaselineUi = options.verifyBaselineUi !== false
    const verifyEditedUi = options.verifyEditedUi !== false
    const verifyCheckoutUi = options.verifyCheckoutUi !== false
    const verifyResyncUi = options.verifyResyncUi !== false

    function verifyStateEquals(getter, expectedValue, timeoutMs, label) {
        tryVerifyWithDiagnostics(testCase, () => {
            return deepEqual(getter(), expectedValue)
        }, timeoutMs, label)
    }

    if (options.prepare !== undefined && options.prepare !== null) {
        options.prepare("baseline")
    }

    let baselineValue = options.getter()
    let baselineUiValue = options.uiExpectedFromValue !== undefined && options.uiExpectedFromValue !== null
                        ? options.uiExpectedFromValue(baselineValue)
                        : baselineValue
    if (options.requireNonEmptyBaseline !== false) {
        if (typeof baselineValue === "string") {
            testCase.verify(baselineValue.length > 0)
        } else {
            testCase.verify(baselineValue !== null && baselineValue !== undefined)
        }
    }

    if (options.uiGetter !== undefined && options.uiGetter !== null && verifyBaselineUi) {
        if (options.prepare !== undefined && options.prepare !== null) {
            options.prepare("baseline-ui")
        }
        verifyStateEquals(options.uiGetter, baselineUiValue, verifyEditedValueTimeoutMs, "verify baseline UI value")
    }

    let commitA = testHelper.projectHeadCommitOid(rootData.project)
    testCase.verify(commitA !== "")

    let syncedValue = options.nextValue(baselineValue)
    let syncedUiValue = options.uiExpectedFromValue !== undefined && options.uiExpectedFromValue !== null
                      ? options.uiExpectedFromValue(syncedValue)
                      : syncedValue

    if (options.prepare !== undefined && options.prepare !== null) {
        options.prepare("before-set")
    }
    options.setter(syncedValue)

    if (options.prepare !== undefined && options.prepare !== null) {
        options.prepare("after-set")
    }
    verifyStateEquals(options.getter, syncedValue, verifyEditedValueTimeoutMs, "verify value after local setter")

    if (options.uiGetter !== undefined && options.uiGetter !== null && verifyEditedUi) {
        if (options.prepare !== undefined && options.prepare !== null) {
            options.prepare("after-set-ui")
        }
        verifyStateEquals(options.uiGetter, syncedUiValue, verifyEditedValueTimeoutMs, "verify edited UI value")
    }

    testCase.verify(rootData.project.sync())
    waitForProjectSyncToFinish(testCase, rootData)

    let commitB = testHelper.projectHeadCommitOid(rootData.project)
    testCase.verify(commitB !== "")
    testCase.verify(commitB !== commitA)

    let checkoutError = testHelper.checkoutProjectRef(rootData.project, commitA, true)
    testCase.compare(checkoutError, "")

    if (options.restorePage !== undefined && options.restorePage !== null) {
        options.restorePage("after-checkout")
    } else {
        verifyStillOnTripPage(testCase, rootData, options.tripPageAddress)
    }

    if (options.prepare !== undefined && options.prepare !== null) {
        options.prepare("after-checkout")
    }
    verifyStateEquals(options.getter, baselineValue, verifyBaselineAfterCheckoutTimeoutMs, "verify baseline after checkout")

    if (options.uiGetter !== undefined && options.uiGetter !== null && verifyCheckoutUi) {
        if (options.prepare !== undefined && options.prepare !== null) {
            options.prepare("after-checkout-ui")
        }
        verifyStateEquals(options.uiGetter, baselineUiValue, verifyBaselineAfterCheckoutTimeoutMs, "verify baseline UI after checkout")
    }

    testCase.verify(rootData.project.sync())
    waitForProjectSyncToFinish(testCase, rootData)

    let commitC = testHelper.projectHeadCommitOid(rootData.project)
    testCase.verify(commitC !== "")
    testCase.verify(commitB === commitC)

    if (options.restorePage !== undefined && options.restorePage !== null) {
        options.restorePage("after-resync")
    } else {
        verifyStillOnTripPage(testCase, rootData, options.tripPageAddress)
    }

    if (options.prepare !== undefined && options.prepare !== null) {
        options.prepare("after-resync")
    }
    verifyStateEquals(options.getter, syncedValue, verifyResyncedValueTimeoutMs, "verify synced value after second sync")

    if (options.uiGetter !== undefined && options.uiGetter !== null && verifyResyncUi) {
        if (options.prepare !== undefined && options.prepare !== null) {
            options.prepare("after-resync-ui")
        }
        verifyStateEquals(options.uiGetter, syncedUiValue, verifyResyncedValueTimeoutMs, "verify synced UI value after second sync")
    }
}
