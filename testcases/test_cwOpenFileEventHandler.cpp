/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

#include "TestHelper.h"

#include "cwErrorListModel.h"
#include "cwJobSettings.h"
#include "cwOpenFileEventHandler.h"
#include "cwRootData.h"

#include <QApplication>
#include <QFileOpenEvent>
#include <QSignalSpy>

#include <catch2/catch_test_macros.hpp>

namespace {
int fatalErrorCount(const cwErrorListModel* errorModel)
{
    if (errorModel == nullptr) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < errorModel->size(); ++i) {
        if (errorModel->at(i).type() == cwError::Fatal) {
            ++count;
        }
    }
    return count;
}
}

TEST_CASE("cwOpenFileEventHandler should load and convert legacy cw files", "[cwOpenFileEventHandler]")
{
    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    root->settings()->jobSettings()->setAutomaticUpdate(false);

    const QString legacyProjectPath = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

    cwOpenFileEventHandler openFileHandler(QApplication::instance());
    openFileHandler.setProject(root->project());
    QApplication::instance()->installEventFilter(&openFileHandler);

    QFileOpenEvent openEvent(legacyProjectPath);
    QApplication::sendEvent(QApplication::instance(), &openEvent);

    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    root->project()->waitSaveToFinish();

    QApplication::instance()->removeEventFilter(&openFileHandler);

    CHECK(openEvent.isAccepted());
    CHECK(fatalErrorCount(root->project()->errorModel()) == 0);
    REQUIRE(root->project()->cavingRegion()->caveCount() >= 1);
    CHECK(root->project()->filename() != legacyProjectPath);
    CHECK(root->project()->fileType() == cwProject::GitFileType);
}

TEST_CASE("cwOpenFileEventHandler cavewhere:// URL handling", "[FileOpen]")
{
    SECTION("cavewhere:// URL emits deepLinkReceived, not loadOrConvert")
    {
        auto root = std::make_unique<cwRootData>();
        root->settings()->jobSettings()->setAutomaticUpdate(false);

        cwOpenFileEventHandler openFileHandler(QApplication::instance());
        openFileHandler.setProject(root->project());
        QApplication::instance()->installEventFilter(&openFileHandler);

        QSignalSpy deepLinkSpy(&openFileHandler, &cwOpenFileEventHandler::deepLinkReceived);

        QFileOpenEvent openEvent(QUrl("cavewhere://open?repo=https://github.com/user/repo"));
        QApplication::sendEvent(QApplication::instance(), &openEvent);

        QApplication::instance()->removeEventFilter(&openFileHandler);

        REQUIRE(deepLinkSpy.count() == 1);
        CHECK(deepLinkSpy.first().first().toUrl().scheme() == QStringLiteral("cavewhere"));
        CHECK(openEvent.isAccepted());
        // No cave data should have been loaded
        CHECK(root->project()->cavingRegion()->caveCount() == 0);
    }

    SECTION("Regular file path still calls loadOrConvert")
    {
        auto root = std::make_unique<cwRootData>();
        root->settings()->jobSettings()->setAutomaticUpdate(false);

        const QString legacyProjectPath = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

        cwOpenFileEventHandler openFileHandler(QApplication::instance());
        openFileHandler.setProject(root->project());
        QApplication::instance()->installEventFilter(&openFileHandler);

        QSignalSpy deepLinkSpy(&openFileHandler, &cwOpenFileEventHandler::deepLinkReceived);

        QFileOpenEvent openEvent(legacyProjectPath);
        QApplication::sendEvent(QApplication::instance(), &openEvent);

        root->project()->waitLoadToFinish();
        root->futureManagerModel()->waitForFinished();
        root->project()->waitSaveToFinish();

        QApplication::instance()->removeEventFilter(&openFileHandler);

        CHECK(deepLinkSpy.count() == 0);
        CHECK(openEvent.isAccepted());
        REQUIRE(root->project()->cavingRegion()->caveCount() >= 1);
    }
}
