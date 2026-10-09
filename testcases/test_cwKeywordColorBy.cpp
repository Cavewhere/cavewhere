//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwColorBy.h"
#include "cwColorByUniform.h"
#include "cwKeywordColorBy.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwVisibilityProxy.h"

//Qt includes
#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>

//Std includes
#include <memory>
#include <vector>

namespace {

constexpr float kNothingApplied = -1.0f;
constexpr float kOtherSlot = static_cast<float>(cwKeywordColorBy::kOtherSlot);

// cwColorByUniform::params layout
constexpr int kParamMode = 0;
constexpr int kParamSlotCount = 2;
constexpr int kParamValueSource = 3;

// Records what the base class pushes, as a render-object proxy would.
class RecordingProxy : public cwVisibilityProxy
{
public:
    using cwVisibilityProxy::cwVisibilityProxy;

    float lastApplied = kNothingApplied;

protected:
    void applyVisible(bool visible) override { Q_UNUSED(visible); }
    void applyColorValue(float value) override { lastApplied = value; }
};

// A keyword item model whose items each wrap a RecordingProxy. Declared
// before anything that observes the model, so the items die first.
class Fixture
{
public:
    RecordingProxy* add(const QString& caveName)
    {
        return add(caveName.isEmpty() ? QStringList() : QStringList({caveName}));
    }

    RecordingProxy* add(const QStringList& caveNames)
    {
        auto item = std::make_unique<cwKeywordItem>();
        for (const QString& caveName : caveNames) {
            item->keywordModel()->add(cwKeyword(cwKeywordModel::CaveKey, caveName));
        }
        auto* proxy = new RecordingProxy(item.get());
        item->setObject(proxy);
        model.addItem(item.get());
        items.push_back(std::move(item));
        return proxy;
    }

    cwKeywordItemModel model;
    std::vector<std::unique_ptr<cwKeywordItem>> items;
};

// Points cwColorBy at its own QSettings store, unique to this process so
// parallel runs never collide, and clears it on both ends.
class ScopedSettings
{
public:
    ScopedSettings() :
        m_previousApplicationName(QCoreApplication::applicationName())
    {
        QCoreApplication::setApplicationName(
            QStringLiteral("cavewhere-test-keywordColorBy-%1").arg(QCoreApplication::applicationPid()));
        QSettings().clear();
    }

    ~ScopedSettings()
    {
        QSettings().clear();
        QCoreApplication::setApplicationName(m_previousApplicationName);
    }

private:
    QString m_previousApplicationName;
};

// cwKeywordColorBy coalesces model signals onto the next event-loop turn.
void settle()
{
    QCoreApplication::processEvents();
}

} // namespace

TEST_CASE("cwKeywordColorBy gives each keyword value its sorted slot", "[ColorBy]")
{
    Fixture fixture;
    RecordingProxy* firstA = fixture.add(QStringLiteral("A"));
    RecordingProxy* b = fixture.add(QStringLiteral("B"));
    RecordingProxy* secondA = fixture.add(QStringLiteral("A"));
    RecordingProxy* noCave = fixture.add(QString());

    cwKeywordColorBy colorBy;
    colorBy.setKeywordItemModel(&fixture.model);
    colorBy.setKey(cwKeywordModel::CaveKey);
    settle();

    CHECK(firstA->colorValue() == 0.0f);
    CHECK(b->colorValue() == 1.0f);
    CHECK(b->lastApplied == 1.0f);
    CHECK(secondA->colorValue() == 0.0f);
    CHECK(noCave->colorValue() == kOtherSlot);
    CHECK(noCave->lastApplied == kOtherSlot);

    CHECK(colorBy.slotCount() == 2);
    CHECK(colorBy.values() == QStringList({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("Others")}));

    SECTION("A value that sorts in between shifts the later slots") {
        RecordingProxy* between = fixture.add(QStringLiteral("AB"));
        settle();
        CHECK(between->colorValue() == 1.0f);
        CHECK(b->colorValue() == 2.0f);
        CHECK(firstA->colorValue() == 0.0f);
        CHECK(noCave->colorValue() == kOtherSlot);
        CHECK(colorBy.slotCount() == 3);
    }

    SECTION("An empty key assigns nothing and clears the legend") {
        colorBy.setKey(QString());
        settle();
        CHECK(colorBy.slotCount() == 0);
        CHECK(colorBy.values().isEmpty());
        CHECK(b->colorValue() == 1.0f);
    }
}

TEST_CASE("cwColorBy keyword mode builds a categorical per-element block", "[ColorBy]")
{
    ScopedSettings settings;
    Fixture fixture;
    fixture.add(QStringLiteral("A"));
    fixture.add(QStringLiteral("B"));
    fixture.add(QString());

    cwColorBy colorBy;
    colorBy.setKeywordItemModel(&fixture.model);
    colorBy.setKeywordKey(cwKeywordModel::CaveKey);
    CHECK(colorBy.uniform().params[kParamMode] == static_cast<float>(cwColorByUniform::Off));
    CHECK(colorBy.legendValues().isEmpty());

    colorBy.setSource(cwColorBy::Source::Keyword);
    settle();
    const cwColorByUniform uniform = colorBy.uniform();
    CHECK(uniform.params[kParamMode] == static_cast<float>(cwColorByUniform::Categorical));
    CHECK(uniform.params[kParamSlotCount] == 2.0f);
    CHECK(uniform.params[kParamValueSource] == static_cast<float>(cwColorByUniform::PerElement));
    CHECK(colorBy.legendValues() == QStringList({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("Others")}));
    CHECK(colorBy.legendSlot(1) == 1);
    CHECK(colorBy.legendSlot(2) == cwKeywordColorBy::kOtherSlot);
    CHECK(colorBy.slotColor(0) == QColor(QStringLiteral("#E69F00")));
    CHECK(colorBy.slotColor(cwKeywordColorBy::kOtherSlot) == QColor(QStringLiteral("#999999")));

    // The baked ramp is what the shader indexes by slot.
    CHECK(QColor::fromRgbF(uniform.ramp[0][0], uniform.ramp[0][1], uniform.ramp[0][2]) == colorBy.slotColor(0));

    colorBy.setSource(cwColorBy::Source::None);
    settle();
    CHECK(colorBy.uniform().params[kParamMode] == static_cast<float>(cwColorByUniform::Off));
    CHECK(colorBy.legendValues().isEmpty());
}

TEST_CASE("cwKeywordColorBy puts an item with two values for the key in Others", "[ColorBy]")
{
    Fixture fixture;
    RecordingProxy* a = fixture.add(QStringLiteral("A"));
    RecordingProxy* both = fixture.add(QStringList({QStringLiteral("A"), QStringLiteral("B")}));
    RecordingProxy* b = fixture.add(QStringLiteral("B"));

    cwKeywordColorBy colorBy;
    colorBy.setKeywordItemModel(&fixture.model);
    colorBy.setKey(cwKeywordModel::CaveKey);
    settle();

    CHECK(a->colorValue() == 0.0f);
    CHECK(b->colorValue() == 1.0f);
    CHECK(both->colorValue() == kOtherSlot);
    CHECK(both->lastApplied == kOtherSlot);

    // No item lacks the key, yet the two-valued item is drawn as "Others".
    CHECK(colorBy.slotCount() == 2);
    CHECK(colorBy.values() == QStringList({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("Others")}));
}

TEST_CASE("cwKeywordColorBy assigns a burst of inserts in one pass", "[ColorBy]")
{
    Fixture fixture;
    cwKeywordColorBy colorBy;
    colorBy.setKeywordItemModel(&fixture.model);
    colorBy.setKey(cwKeywordModel::CaveKey);
    settle();

    QSignalSpy slotsSpy(&colorBy, &cwKeywordColorBy::slotsChanged);
    RecordingProxy* c = fixture.add(QStringLiteral("C"));
    RecordingProxy* b = fixture.add(QStringLiteral("B"));
    RecordingProxy* a = fixture.add(QStringLiteral("A"));

    // Nothing is pushed until the event loop turns, so no item passes
    // through a transient slot.
    CHECK(c->lastApplied == kNothingApplied);
    CHECK(slotsSpy.isEmpty());

    settle();
    CHECK(slotsSpy.size() == 1);
    CHECK(a->colorValue() == 0.0f);
    CHECK(b->lastApplied == 1.0f);
    CHECK(c->lastApplied == 2.0f);
}
