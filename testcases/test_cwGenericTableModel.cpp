//Catch includes
#include "catch.hpp"

//Our includes
#include "cwGenericTableModel.h"
#include "SpyChecker.h"

//Qt includes
#include <QVector3D>
#include <QSignalSpy>
#include <QDebug>

class QVector3DModel : public cwGenericTableModel<QVector3D> {
public:
    enum Roles {
        UnitRole = Qt::UserRole
    };

    QVector3DModel(QObject* parent = nullptr) :
        cwGenericTableModel<QVector3D>(parent)
    {
        auto lengthChanged = [this](int row) {
            auto legthColumnIndex = column(LengthColumn);
            auto currentIndex = index(row, legthColumnIndex);
            emit dataChanged(currentIndex, currentIndex, {Qt::DisplayRole});
        };

        auto setNumber = [this, lengthChanged](const QModelIndex& index, const QVariant& value, auto&& setFunc) {
            bool okay;
            double num = value.toDouble(&okay);
            if(okay) {
                setFunc(num);
                emit dataChanged(index, index, {Qt::DisplayRole});
                lengthChanged(index.row());
            }
            return okay;
        };

        auto x = addColumn("x");
        x->registerDataRole(Qt::DisplayRole,
                            [](const QModelIndex& index, const QVector3D& row)
        {
            Q_UNUSED(index);
            return row.x();
        });
        x->registerDataRole(UnitRole,
                            [](const QModelIndex& index, const QVector3D& row)
        {
            Q_UNUSED(row);
            Q_UNUSED(index);
            return "ft";
        });

        x->registerSetDataRole(Qt::DisplayRole,
                               [setNumber](const QModelIndex& index,
                               QVector3D& row,
                               const QVariant& value)
        {
            Q_UNUSED(index);
            return setNumber(index, value, [&row](double value) { row.setX(value); });
        });

        auto y = addColumn("y");
        y->registerDataRole(Qt::DisplayRole,
                            [](const QModelIndex& index, const QVector3D& row)
        {
            Q_UNUSED(index);
            return row.y();
        });

        y->registerSetDataRole(Qt::DisplayRole,
                               [setNumber](const QModelIndex& index, QVector3D& row, const QVariant& value)
        {
            Q_UNUSED(index);
            return setNumber(index, value, [&row](double value) { row.setY(value); });
        });

        auto z = addColumn("z");
        z->registerDataRole(Qt::DisplayRole,
                            [](const QModelIndex& index, const QVector3D& row)
        {
            Q_UNUSED(index);
            return row.z();
        });

        z->registerSetDataRole(Qt::DisplayRole,
                               [setNumber](const QModelIndex& index, QVector3D& row, const QVariant& value)
        {
            Q_UNUSED(index);
            return setNumber(index, value, [&row](double value) { row.setZ(value); });
        });

        LengthColumn = addColumn("Length");
        LengthColumn->registerDataRole(Qt::DisplayRole,
                            [](const QModelIndex& index, const QVector3D& row)
        {
            Q_UNUSED(index);
            return row.length();
        });
    }

private:
    Column* LengthColumn;

};

TEST_CASE("Generic table model should work correctly", "[cwGenericTableModel]") {

    QVector3DModel pointModel;

    QSignalSpy dataChangedSpy(&pointModel, &QVector3DModel::dataChanged);
    QSignalSpy rowInsertedSpy(&pointModel, &QVector3DModel::rowsInserted);
    QSignalSpy rowRemovedSpy(&pointModel, &QVector3DModel::rowsRemoved);
    QSignalSpy rowMovedSpy(&pointModel, &QVector3DModel::rowsMoved);
    QSignalSpy columnInsertedSpy(&pointModel, &QVector3DModel::columnsInserted);
    QSignalSpy columnRemovedSpy(&pointModel, &QVector3DModel::columnsRemoved);
    QSignalSpy columnMoveSpy(&pointModel, &QVector3DModel::columnsMoved);
    QSignalSpy resetSpy(&pointModel, &QVector3DModel::modelReset);

    dataChangedSpy.setObjectName("dataChangeSpy");
    rowInsertedSpy.setObjectName("rowInsertedSpy");
    rowRemovedSpy.setObjectName("rowRemovedSpy");
    rowMovedSpy.setObjectName("rowMovedSpy");
    columnInsertedSpy.setObjectName("columnInsertedSpy");
    columnRemovedSpy.setObjectName("columnRemovedSpy");
    columnMoveSpy.setObjectName("columnMoveSpy");
    resetSpy.setObjectName("resetSpy");

    SpyChecker spyChecker;
    spyChecker.insert(&rowInsertedSpy, 0);
    spyChecker.insert(&rowRemovedSpy, 0);
    spyChecker.insert(&rowMovedSpy, 0);
    spyChecker.insert(&columnInsertedSpy, 0);
    spyChecker.insert(&columnRemovedSpy, 0);
    spyChecker.insert(&columnMoveSpy, 0);
    spyChecker.insert(&resetSpy, 0);

    CHECK(pointModel.rowCount() == 0);
    CHECK(pointModel.columnCount() == 4);

    CHECK(pointModel.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString().toStdString() == "x");
    CHECK(pointModel.headerData(1).toString().toStdString() == "y");
    CHECK(pointModel.headerData(2).toString().toStdString() == "z");
    CHECK(pointModel.headerData(3).toString().toStdString() == "Length");
    CHECK(pointModel.headerData(-1, Qt::Horizontal, Qt::DisplayRole).isNull());
    CHECK(pointModel.headerData(4, Qt::Horizontal, Qt::DisplayRole).isNull());
    CHECK(pointModel.headerData(0, Qt::Vertical, Qt::DisplayRole).isNull());
    CHECK(pointModel.headerData(0, Qt::Horizontal, 1000).isNull());

    QVector3D v1(1.0, 2.0, 3.0);
    pointModel.append(v1);

    spyChecker[&rowInsertedSpy] = 1;
    spyChecker.requireSpies();
    CHECK(rowInsertedSpy.at(0).at(1).toInt() == 0);
    CHECK(rowInsertedSpy.at(0).at(2).toInt() == 0);

    CHECK(pointModel.rowCount() == 1);

    CHECK(pointModel.data(pointModel.index(0, 0)).toDouble() == v1.x());
    CHECK(pointModel.data(pointModel.index(0, 0), QVector3DModel::UnitRole).toString().toStdString() == "ft");
    CHECK(pointModel.data(pointModel.index(0, 1)).toDouble() == v1.y());
    CHECK(pointModel.data(pointModel.index(0, 2)).toDouble() == v1.z());
    CHECK(pointModel.data(pointModel.index(0, 3)).toDouble() == v1.length());

    spyChecker.checkSpies();
    spyChecker.clearSpyCounts();

    SECTION("Add a list of vector3d") {
        QList<QVector3D> points;
        for(int i = 1; i <= 5; i++) {
            points.append(QVector3D(i * 1.0, i * 10.0, i * 100.0));
        }
        pointModel.append(points);

        spyChecker[&rowInsertedSpy] = 1;
        spyChecker.requireSpies();
        CHECK(rowInsertedSpy.at(0).at(1).toInt() == 1);
        CHECK(rowInsertedSpy.at(0).at(2).toInt() == 5);

        points.prepend(v1);

        auto checkRows = [&]() {
            CHECK(pointModel.rowCount() == points.size());
            for(int i = 0; i < points.size(); i++) {
                auto point = points.at(i);

                CHECK(pointModel.data(pointModel.index(i, 0)).toDouble() == point.x());
                CHECK(pointModel.data(pointModel.index(i, 0), QVector3DModel::UnitRole).toString().toStdString() == "ft");
                CHECK(pointModel.data(pointModel.index(i, 1)).toDouble() == point.y());
                CHECK(pointModel.data(pointModel.index(i, 2)).toDouble() == point.z());
                CHECK(pointModel.data(pointModel.index(i, 3)).toDouble() == point.length());
            }
            spyChecker.checkSpies();
        };

        checkRows();
        spyChecker.clearSpyCounts();

        SECTION("Append empty list") {
            pointModel.append(QList<QVector3D>());

            spyChecker.checkSpies();
            checkRows();
        }

        SECTION("Removing rows should work correctly") {

            pointModel.remove(1);

            spyChecker[&rowRemovedSpy] = 1;
            spyChecker.requireSpies();
            CHECK(rowRemovedSpy.at(0).at(1).toInt() == 1);
            CHECK(rowRemovedSpy.at(0).at(2).toInt() == 1);

            pointModel.remove(4);

            spyChecker[&rowRemovedSpy] = 2;
            spyChecker.requireSpies();
            CHECK(rowRemovedSpy.at(1).at(1).toInt() == 4);
            CHECK(rowRemovedSpy.at(1).at(2).toInt() == 4);

            points.removeAt(1);
            points.removeAt(4);

            checkRows();
        }

        SECTION("Setting row should work correctly") {
            auto newRow = QVector3D(6.3, 2.3, 1.1);
            pointModel.setRow(1, newRow);

            spyChecker[&dataChangedSpy] = 1;
            spyChecker.requireSpies();
            CHECK(dataChangedSpy.at(0).at(0).value<QModelIndex>().row() == 1);
            CHECK(dataChangedSpy.at(0).at(0).value<QModelIndex>().column() == 0);
            CHECK(dataChangedSpy.at(0).at(1).value<QModelIndex>().row() == 1);
            CHECK(dataChangedSpy.at(0).at(1).value<QModelIndex>().column() == 3); //4 total columns, 3 is last index
            CHECK(dataChangedSpy.at(0).at(2).value<QVector<int>>().isEmpty());

            points[1] = newRow;

            checkRows();
        }

        SECTION("setData should work correctly") {

            SECTION("X") {
                auto column = pointModel.column("x");
                CHECK(column == 0);

                QModelIndex index = pointModel.index(5, column);
                REQUIRE(index.isValid());
                CHECK(pointModel.setData(index, 5.1));

                spyChecker[&dataChangedSpy] = 2;
                spyChecker.requireSpies();

                CHECK(dataChangedSpy.at(0).at(0).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(0).at(0).value<QModelIndex>().column() == column);
                CHECK(dataChangedSpy.at(0).at(1).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(0).at(1).value<QModelIndex>().column() == column);
                CHECK(dataChangedSpy.at(0).at(2).value<QVector<int>>() == QVector<int>({Qt::DisplayRole}));

                CHECK(dataChangedSpy.at(1).at(0).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(1).at(0).value<QModelIndex>().column() == pointModel.column("Length"));
                CHECK(dataChangedSpy.at(1).at(1).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(1).at(1).value<QModelIndex>().column() == pointModel.column("Length"));
                CHECK(dataChangedSpy.at(1).at(2).value<QVector<int>>() == QVector<int>({Qt::DisplayRole}));

                points[5].setX(5.1);

                checkRows();
            }

            SECTION("Y") {
                auto column = pointModel.column("y");
                CHECK(column == 1);

                QModelIndex index = pointModel.index(5, column);
                CHECK(pointModel.setData(index, 6.1));

                spyChecker[&dataChangedSpy] = 2;
                spyChecker.requireSpies();

                CHECK(dataChangedSpy.at(0).at(0).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(0).at(0).value<QModelIndex>().column() == column);
                CHECK(dataChangedSpy.at(0).at(1).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(0).at(1).value<QModelIndex>().column() == column);
                CHECK(dataChangedSpy.at(0).at(2).value<QVector<int>>() == QVector<int>({Qt::DisplayRole}));

                CHECK(dataChangedSpy.at(1).at(0).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(1).at(0).value<QModelIndex>().column() == pointModel.column("Length"));
                CHECK(dataChangedSpy.at(1).at(1).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(1).at(1).value<QModelIndex>().column() == pointModel.column("Length"));
                CHECK(dataChangedSpy.at(1).at(2).value<QVector<int>>() == QVector<int>({Qt::DisplayRole}));

                points[5].setY(6.1);

                checkRows();
            }

            SECTION("Z") {
                auto column = pointModel.column("z");
                CHECK(column == 2);

                QModelIndex index = pointModel.index(5, column);
                CHECK(pointModel.setData(index, 7.1));

                spyChecker[&dataChangedSpy] = 2;
                spyChecker.requireSpies();

                CHECK(dataChangedSpy.at(0).at(0).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(0).at(0).value<QModelIndex>().column() == column);
                CHECK(dataChangedSpy.at(0).at(1).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(0).at(1).value<QModelIndex>().column() == column);
                CHECK(dataChangedSpy.at(0).at(2).value<QVector<int>>() == QVector<int>({Qt::DisplayRole}));

                CHECK(dataChangedSpy.at(1).at(0).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(1).at(0).value<QModelIndex>().column() == pointModel.column("Length"));
                CHECK(dataChangedSpy.at(1).at(1).value<QModelIndex>().row() == 5);
                CHECK(dataChangedSpy.at(1).at(1).value<QModelIndex>().column() == pointModel.column("Length"));
                CHECK(dataChangedSpy.at(1).at(2).value<QVector<int>>() == QVector<int>({Qt::DisplayRole}));

                points[5].setZ(7.1);

                checkRows();
            }

            SECTION("Try to set the unsettable") {
                auto column = pointModel.column("Length");
                CHECK(column == 3);

                QModelIndex index = pointModel.index(5, column);
                CHECK(pointModel.setData(index, 10.0) == false);
                spyChecker[&dataChangedSpy] = 0;

                index = pointModel.index(5, 0);
                CHECK(pointModel.setData(index, "m", QVector3DModel::UnitRole) == false);
                spyChecker[&dataChangedSpy] = 0;
            }
        }
    }
}
