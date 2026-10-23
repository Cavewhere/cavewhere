/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"

//Qt includes
#include <QSignalSpy>

//Std includes
#include <memory>

namespace {

cwError makeWarning(const QString& message, bool suppressed)
{
    cwError error(message, cwError::Warning);
    error.setSupressed(suppressed);
    return error;
}

}

TEST_CASE("suppressedWarningCount sums the suppressed warnings of every child", "[ErrorModelTest]")
{
    auto parent = std::make_unique<cwErrorModel>();
    auto child = std::make_unique<cwErrorModel>();
    auto grandChild = std::make_unique<cwErrorModel>();

    grandChild->setParentModel(child.get());
    child->setParentModel(parent.get());

    parent->errors()->append(makeWarning(QStringLiteral("parent accepted"), true));
    child->errors()->append(makeWarning(QStringLiteral("child accepted"), true));
    child->errors()->append(makeWarning(QStringLiteral("child open"), false));
    grandChild->errors()->append(makeWarning(QStringLiteral("grandchild open"), false));
    grandChild->errors()->append(cwError(QStringLiteral("grandchild fatal"), cwError::Fatal));

    CHECK(grandChild->suppressedWarningCount() == 0);
    CHECK(child->suppressedWarningCount() == 1);
    CHECK(parent->suppressedWarningCount() == 2);
    CHECK(parent->warningCount() == 2);
    CHECK(parent->fatalCount() == 1);

    QSignalSpy suppressedSpy(parent.get(), &cwErrorModel::suppressedWarningCountChanged);
    QSignalSpy warningSpy(parent.get(), &cwErrorModel::warningCountChanged);

    cwErrorListModel* grandChildErrors = grandChild->errors();
    grandChildErrors->setData(grandChildErrors->index(0),
                              true,
                              static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole));

    CHECK(suppressedSpy.size() == 1);
    CHECK(warningSpy.size() == 1);

    CHECK(grandChild->suppressedWarningCount() == 1);
    CHECK(parent->suppressedWarningCount() == 3);
    CHECK(parent->warningCount() == 1);
    CHECK(parent->fatalCount() == 1);
}
