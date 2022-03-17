//Catch includes
#include "catch.hpp"

//Our includes
#include "cwKeywordFilterModel.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwKeywordItem.h"
#include "SpyChecker.h"
#include "TestHelper.h"

TEST_CASE("cwKeywordFilterModel should filter and sort cwKeywordItemModel rows correctly", "[cwKeywordFilterModel]") {

    auto model = std::make_unique<cwKeywordItemModel>();

    //Generate the items for the model
    auto populateModel = [](cwKeywordItemModel* model) {
        const auto numItems = 10;
        for(int i = 0; i < numItems; i++) {
            auto name = [i](const QString& label) {
                return label + QString::number(i);
            };

            auto item = new cwKeywordItem();
            item->setObjectName(name("item"));
            item->keywordModel()->setObjectName(name("keywordModel"));
            auto entity = new QObject(model);
            entity->setObjectName(name("entity"));

            item->keywordModel()->add({"name", name("name")});
            item->keywordModel()->add({"date", name("date")});

            item->setObject(entity);
            model->addItem(item);
        }
    };

    populateModel(model.get());

    auto filter = std::make_unique<cwKeywordFilterModel>();
    filter->setSourceModel(model.get());

    auto spyChecker = SpyChecker::makeChecker(filter.get());

    CHECK(filter->rowCount() == 0);

    auto insertedSpy = spyChecker.findSpy(&cwKeywordFilterModel::rowsInserted);
    auto aboutToBeInsertedspy = spyChecker.findSpy(&cwKeywordFilterModel::rowsAboutToBeInserted);
    auto removedSpy = spyChecker.findSpy(&cwKeywordFilterModel::rowsRemoved);
    auto aboutToBeRemovedSpy = spyChecker.findSpy(&cwKeywordFilterModel::rowsAboutToBeRemoved);
    auto resetSpy = spyChecker.findSpy(&cwKeywordFilterModel::modelReset);
    auto aboutToBeResetSpy = spyChecker.findSpy(&cwKeywordFilterModel::modelAboutToBeReset);
    auto sourceModelChangedSpy = spyChecker.findSpy(&cwKeywordFilterModel::sourceModelChanged);

    SECTION("Add single row to be filtered") {
        auto sourceIndex = model->index(3, 0, QModelIndex());
        filter->insert(sourceIndex);

        CHECK(filter->rowCount() == 1);

        CHECK(filter->mapToSource(filter->index(0)) == sourceIndex);

        spyChecker[insertedSpy]++;
        spyChecker[aboutToBeInsertedspy]++;
        spyChecker.checkSpies();
    }

    SECTION("Add multiple rows and make sure they're sorted") {

        QVector<int> indexes = {0, 9, 4, 7};
        for(auto i : indexes) {
            auto sourceIndex = model->index(i, 0, QModelIndex());
            filter->insert(sourceIndex);
            spyChecker[insertedSpy]++;
            spyChecker[aboutToBeInsertedspy]++;
        }

        auto checkIndexesWithModel = [&filter, &indexes](cwKeywordItemModel* model) {
            auto sourceIndexes = [&indexes, model]() {
                QVector<QPersistentModelIndex> sourceIndexes;
                for(int i : indexes) {
                    sourceIndexes.append(model->index(i, 0, QModelIndex()));
                }

                std::sort(sourceIndexes.begin(), sourceIndexes.end(), [](const QModelIndex& left, const QModelIndex& right)
                {
                    auto obj = [](const QModelIndex& index) {
                        return index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                    };

                    return obj(left) < obj(right);
                });
                return sourceIndexes;
            }();

            REQUIRE(sourceIndexes.size() == filter->rowCount());

            for(int i = 0; i < filter->rowCount(); i++) {
                auto filterIndex = filter->index(i);
                INFO("Filter to Source:" << filter->mapToSource(filterIndex) << " source:" << sourceIndexes.at(i));
                CHECK(filter->mapToSource(filterIndex) == sourceIndexes.at(i));
                CHECK(filter->mapFromSource(sourceIndexes.at(i)) == filterIndex);
            }
        };

        auto checkIndexes = [checkIndexesWithModel, &model](){
            checkIndexesWithModel(model.get());
        };

        checkIndexes();
        spyChecker.checkSpies();

        SECTION("Add existing index") {
            filter->insert(model->index(9, 0, QModelIndex()));

            //Should be rejected
            checkIndexes();
            spyChecker.checkSpies();
        }

        SECTION("Add bad index") {
            filter->insert(QModelIndex());

            //Should be rejected
            checkIndexes();
            spyChecker.checkSpies();
        }

        SECTION("Remove a rows") {
            filter->remove(model->index(indexes.at(2), 0, QModelIndex()));
            indexes.removeAt(2);

            spyChecker[removedSpy]++;
            spyChecker[aboutToBeRemovedSpy]++;

            checkIndexes();
            spyChecker.checkSpies();

            SECTION("Remove Front") {
                filter->remove(model->index(indexes.first(), 0, QModelIndex()));
                indexes.removeFirst();

                spyChecker[removedSpy]++;
                spyChecker[aboutToBeRemovedSpy]++;

                checkIndexes();
                spyChecker.checkSpies();
            }

            SECTION("Remove last") {
                filter->remove(model->index(indexes.last(), 0, QModelIndex()));
                indexes.removeLast();

                spyChecker[removedSpy]++;
                spyChecker[aboutToBeRemovedSpy]++;

                checkIndexes();
                spyChecker.checkSpies();
            }

        }

        SECTION("Remove row from source model") {
            indexes = {0, 8, 6};
            auto item = model->item(4);
            model->removeItem(item);

            spyChecker[aboutToBeRemovedSpy]++;
            spyChecker[removedSpy]++;

            checkIndexes();
            spyChecker.checkSpies();
        }

        SECTION("Delete item") {
            indexes = {0, 8, 6};
            auto item = model->item(4);
            delete item;

            spyChecker[aboutToBeRemovedSpy]++;
            spyChecker[removedSpy]++;

            checkIndexes();
            spyChecker.checkSpies();
        }

        SECTION("Filter should handle object pointer changes") {
            auto item = model->item(4);
            auto newObject = new QObject(model.get());
            newObject->setObjectName("newObject");
            item->setObject(newObject);

            spyChecker[aboutToBeRemovedSpy]++;
            spyChecker[removedSpy]++;
            spyChecker[insertedSpy]++;
            spyChecker[aboutToBeInsertedspy]++;

            checkIndexes();
            spyChecker.checkSpies();
        }

        SECTION("Filter should handle sourceModel change") {
            auto model2 = std::make_unique<cwKeywordItemModel>();
            populateModel(model2.get());

            filter->setSourceModel(model2.get());

            CHECK(filter->sourceModel() == model2.get());

            indexes = {};

            checkIndexesWithModel(model2.get());

            spyChecker[aboutToBeRemovedSpy]++;
            spyChecker[removedSpy]++;
            spyChecker[sourceModelChangedSpy]++;

            spyChecker.checkSpies();
        }
    }
}
