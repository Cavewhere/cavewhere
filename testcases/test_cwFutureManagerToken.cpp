//Catch includes
#include <catch2/catch.hpp>

//Our includes
#include "cwFutureManagerToken.h"
#include "cwFutureManagerModel.h"
#include "cwAsyncFuture.h"

//AsyncFuture
#include "asyncfuture.h"

//Qt includes
#include <QtConcurrent>

TEST_CASE("cwFutureManagerToken should be thread safe", "[cwFutureManagerToken]") {

    auto model = std::make_unique<cwFutureManagerModel>();

    CHECK(model->rowCount() == 0);

    SECTION("Add job in main thread") {
        cwFutureManagerToken token(model.get());

        auto def = AsyncFuture::deferred<int>();
        token.addJob({def.future(), "Current Thread"});

        REQUIRE(model->rowCount() == 1);
        CHECK(model->index(0).data(cwFutureManagerModel::NameRole).toString().toStdString() == "Current Thread");

        def.complete(1);

        QCoreApplication::processEvents();

        CHECK(model->rowCount() == 0);
    }

    SECTION("The token from the model should work") {
        cwFutureManagerToken token = model->token();

        auto def = AsyncFuture::deferred<int>();
        token.addJob({def.future(), "Current Thread"});

        REQUIRE(model->rowCount() == 1);
        CHECK(model->index(0).data(cwFutureManagerModel::NameRole).toString().toStdString() == "Current Thread");

        def.complete(1);

        QCoreApplication::processEvents();

        CHECK(model->rowCount() == 0);
    }

    SECTION("Add job another thread") {
        cwFutureManagerToken token(model.get());
        auto def = AsyncFuture::deferred<int>();
        auto future = QtConcurrent::run([def, token](){
            auto localToken = token;
            localToken.addJob({def.future(), "Other Thread"});
            QThread::msleep(50);
        });

        cwAsyncFuture::waitForFinished(future);

        REQUIRE(model->rowCount() == 1);
        CHECK(model->index(0).data(cwFutureManagerModel::NameRole).toString().toStdString() == "Other Thread");

        def.complete(1);

        QCoreApplication::processEvents();

        CHECK(model->rowCount() == 0);
    }

    SECTION("Adding to a null token does nothing") {
        cwFutureManagerModel token;

        auto def = AsyncFuture::deferred<int>();
        token.addJob({def.future(), "Current Thread"});

        CHECK(model->rowCount() == 0);
    }

    SECTION("isValid should work correctly") {

        cwFutureManagerModel model;
        CHECK(model.token().isValid() == true);

        cwFutureManagerToken t;
        CHECK(t.isValid() == false);
    }
}
