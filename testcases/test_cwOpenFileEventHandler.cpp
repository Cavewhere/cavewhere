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
