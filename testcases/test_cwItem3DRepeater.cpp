// File: test_cwItem3DRepeater.cpp

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace Catch;

// Qt
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QAbstractListModel>
#include <QVector3D>
#include <QUrl>
#include <QCoreApplication>

// Under test
#include "cwItem3DRepeater.h"



// ----------------------------
// Helper model for the tests
// ----------------------------
class TestListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        PositionRole = Qt::UserRole + 1,
        DisplayNameRole,
        LengthMetersRole,
        AltPositionRole
    };

    struct Entry {
        QVector3D position3D;
        QString displayName;
        double lengthMeters = 0.0;
        QVector3D altPosition3D;
        bool displayNameValid = true; // when false, data() returns invalid for DisplayNameRole
    };

    explicit TestListModel(QObject* parent = nullptr) : QAbstractListModel(parent) { }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        if(parent.isValid()) {
            return 0;
        }
        return m_entries.size();
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if(!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
            return {};
        }
        const Entry& e = m_entries.at(index.row());
        switch(role) {
        case PositionRole:      return QVariant::fromValue(e.position3D);
        case DisplayNameRole:   return e.displayNameValid ? QVariant::fromValue(e.displayName) : QVariant();
        case LengthMetersRole:  return e.lengthMeters;
        case AltPositionRole:   return QVariant::fromValue(e.altPosition3D);
        default: return {};
        }
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role) override {
        if(!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
            return false;
        }
        Entry& e = m_entries[index.row()];
        bool changed = false;
        switch(role) {
        case PositionRole: {
            const QVector3D v = value.value<QVector3D>();
            if(e.position3D != v) { e.position3D = v; changed = true; }
            break;
        }
        case DisplayNameRole: {
            const QString v = value.toString();
            if(e.displayName != v) { e.displayName = v; changed = true; }
            e.displayNameValid = true;
            break;
        }
        case LengthMetersRole: {
            const double v = value.toDouble();
            if(e.lengthMeters != v) { e.lengthMeters = v; changed = true; }
            break;
        }
        case AltPositionRole: {
            const QVector3D v = value.value<QVector3D>();
            if(e.altPosition3D != v) { e.altPosition3D = v; changed = true; }
            break;
        }
        default:
            break;
        }

        if(changed) {
            emit dataChanged(index, index, QList<int>() << role);
        }
        return changed;
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if(!index.isValid()) {
            return Qt::NoItemFlags;
        }
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    QHash<int, QByteArray> roleNames() const override {
        QHash<int, QByteArray> names;
        names[PositionRole]     = QByteArrayLiteral("position3D");
        names[DisplayNameRole]  = QByteArrayLiteral("displayName");
        names[LengthMetersRole] = QByteArrayLiteral("lengthMeters");
        names[AltPositionRole]  = QByteArrayLiteral("");
        return names;
    }

    // Test helpers
    void resetWith(const QVector<Entry>& entries) {
        beginResetModel();
        m_entries = entries;
        endResetModel();
    }

    void insertRowAt(int row, const Entry& e) {
        if(row < 0) { row = 0; }
        if(row > m_entries.size()) { row = m_entries.size(); }
        beginInsertRows(QModelIndex(), row, row);
        m_entries.insert(row, e);
        endInsertRows();
    }

    void removeRowAt(int row) {
        if(row < 0 || row >= m_entries.size()) {
            return;
        }
        beginRemoveRows(QModelIndex(), row, row);
        m_entries.removeAt(row);
        endRemoveRows();
    }

    void invalidateDisplayNameAt(int row) {
        if(row < 0 || row >= m_entries.size()) {
            return;
        }
        m_entries[row].displayNameValid = false;
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, QList<int>() << DisplayNameRole);
    }

private:
    QVector<Entry> m_entries;

    // Private data at end
};

// ----------------------------
// Delegate QML as data: URL
// ----------------------------
static QUrl makeDelegateUrl() {
    const QString qml = QString::fromUtf8(
        "import QtQuick 2.15\n"
        "Item {\n"
        "  id: root\n"
        "  required property vector3d position3D\n"
        "  required property string displayName\n"
        "  required property real lengthMeters\n"
        "  property int row\n"
        "}\n"
        );
    const QByteArray encoded = QUrl::toPercentEncoding(qml);
    return QUrl(QStringLiteral("data:text/plain,") + QString::fromLatin1(encoded));
}

// ----------------------------
// Root QML that hosts repeater
// ----------------------------
static QObject* makeRootWithRepeater(QQmlEngine& engine) {
    // Register the type for QML construction
    qmlRegisterType<cwItem3DRepeater>("Test", 1, 0, "Item3DRepeater");

    const QString rootQml = QString::fromUtf8(
        "import QtQuick 2.15\n"
        "import Test 1.0\n"
        "Item {\n"
        "  width: 400; height: 300\n"
        "  Item3DRepeater { id: rep; objectName: \"rep\"; anchors.fill: parent }\n"
        "}\n"
        );

    QQmlComponent comp(&engine);
    comp.setData(rootQml.toUtf8(), QUrl());
    QObject* root = comp.create();
    REQUIRE(root != nullptr);
    return root;
}

static cwItem3DRepeater* findRepeater(QObject* root) {
    auto* repObj = root->findChild<QObject*>(QStringLiteral("rep"));
    REQUIRE(repObj != nullptr);
    auto* rep = qobject_cast<cwItem3DRepeater*>(repObj);
    REQUIRE(rep != nullptr);
    return rep;
}

static QVector<QQuickItem*> repeaterChildren(cwItem3DRepeater* rep) {
    // Only direct QQuickItem children should be the delegates
    QVector<QQuickItem*> items;
    const auto children = rep->childItems();
    for(QQuickItem* it : children) {
        items.push_back(it);
    }
    return items;
}

// ----------------------------
// Tests
// ----------------------------

TEST_CASE("Item3DRepeater creates items per row with all role properties", "[cwItem3DRepeater]") {
    QQmlEngine engine;
    QObject* root = makeRootWithRepeater(engine);
    auto* rep = findRepeater(root);

    rep->setQmlSource(makeDelegateUrl());

    TestListModel model;
    model.resetWith({
        { QVector3D(1,2,3), QStringLiteral("A"), 10.0, QVector3D(9,9,9), true },
        { QVector3D(4,5,6), QStringLiteral("B"), 20.0, QVector3D(8,8,8), true }
    });

    rep->setModel(&model);
    rep->setPositionRole(TestListModel::PositionRole);

    const auto items = repeaterChildren(rep);
    REQUIRE(items.size() == 2);

    // Validate properties for both items (order should match insertion)
    {
        QQuickItem* item0 = items[0];
        REQUIRE(item0->property("displayName").toString() == QStringLiteral("A"));
        REQUIRE(item0->property("lengthMeters").toDouble() == Approx(10.0));
        const QVector3D p0 = item0->property("position3D").value<QVector3D>();
        REQUIRE(p0 == QVector3D(1,2,3));
    }
    {
        QQuickItem* item1 = items[1];
        REQUIRE(item1->property("displayName").toString() == QStringLiteral("B"));
        REQUIRE(item1->property("lengthMeters").toDouble() == Approx(20.0));
        const QVector3D p1 = item1->property("position3D").value<QVector3D>();
        REQUIRE(p1 == QVector3D(4,5,6));
    }

    delete root;
}

TEST_CASE("Item3DRepeater updates changed roles on dataChanged", "[cwItem3DRepeater]") {
    QQmlEngine engine;
    QObject* root = makeRootWithRepeater(engine);
    auto* rep = findRepeater(root);

    rep->setQmlSource(makeDelegateUrl());

    TestListModel model;
    model.resetWith({
        { QVector3D(0,0,0), QStringLiteral("X"), 5.0, QVector3D(1,1,1), true },
        { QVector3D(1,2,3), QStringLiteral("Y"), 6.0, QVector3D(2,2,2), true }
    });

    rep->setModel(&model);
    rep->setPositionRole(TestListModel::PositionRole);

    auto items = repeaterChildren(rep);
    REQUIRE(items.size() == 2);

    // Change display name of row 1
    model.setData(model.index(1,0), QStringLiteral("Y2"), TestListModel::DisplayNameRole);
    REQUIRE(items[1]->property("displayName").toString() == QStringLiteral("Y2"));

    // Change position3D of row 0 (should update position3D on the item)
    const QVector3D newPos(7,7,7);
    model.setData(model.index(0,0), QVariant::fromValue(newPos), TestListModel::PositionRole);
    const QVector3D p0 = items[0]->property("position3D").value<QVector3D>();
    REQUIRE(p0 == newPos);

    delete root;
}

TEST_CASE("Item3DRepeater responds to rowsInserted and rowsRemoved", "[cwItem3DRepeater]") {
    QQmlEngine engine;
    QObject* root = makeRootWithRepeater(engine);
    auto* rep = findRepeater(root);
    rep->setQmlSource(makeDelegateUrl());

    TestListModel model;
    model.resetWith({
        { QVector3D(1,0,0), QStringLiteral("A"), 1.0, QVector3D(0,0,0), true }
    });
    rep->setModel(&model);
    rep->setPositionRole(TestListModel::PositionRole);

    auto items = repeaterChildren(rep);
    REQUIRE(items.size() == 1);

    // Insert at row 1
    model.insertRowAt(1, { QVector3D(2,0,0), QStringLiteral("B"), 2.0, QVector3D(0,0,0), true });
    items = repeaterChildren(rep);
    REQUIRE(items.size() == 2);

    // Remove row 0
    model.removeRowAt(0);
    QCoreApplication::processEvents(); // process deleteLaters
    items = repeaterChildren(rep);
    REQUIRE(items.size() == 1);

    // Remaining item should be "B"
    REQUIRE(items[0]->property("displayName").toString() == QStringLiteral("B"));

    delete root;
}

TEST_CASE("Item3DRepeater rebuilds on modelReset", "[cwItem3DRepeater]") {
    QQmlEngine engine;
    QObject* root = makeRootWithRepeater(engine);
    auto* rep = findRepeater(root);
    rep->setQmlSource(makeDelegateUrl());

    TestListModel model;
    model.resetWith({
        { QVector3D(1,2,3), QStringLiteral("A"), 1.0, QVector3D(0,0,0), true },
        { QVector3D(4,5,6), QStringLiteral("B"), 2.0, QVector3D(0,0,0), true }
    });
    rep->setModel(&model);
    rep->setPositionRole(TestListModel::PositionRole);

    auto items = repeaterChildren(rep);
    REQUIRE(items.size() == 2);

    // Reset with a single row
    model.resetWith({
        { QVector3D(9,9,9), QStringLiteral("C"), 3.0, QVector3D(0,0,0), true }
    });

    items = repeaterChildren(rep);
    REQUIRE(items.size() == 1);
    REQUIRE(items[0]->property("displayName").toString() == QStringLiteral("C"));

    delete root;
}

TEST_CASE("Item3DRepeater updates position when positionRole changes", "[cwItem3DRepeater]") {
    QQmlEngine engine;
    QObject* root = makeRootWithRepeater(engine);
    auto* rep = findRepeater(root);
    rep->setQmlSource(makeDelegateUrl());

    TestListModel model;
    model.resetWith({
        { QVector3D(1,2,3), QStringLiteral("A"), 1.0, QVector3D(30,20,10), true },
        { QVector3D(4,5,6), QStringLiteral("B"), 2.0, QVector3D(60,50,40), true }
    });
    rep->setModel(&model);
    rep->setPositionRole(TestListModel::PositionRole);

    auto items = repeaterChildren(rep);
    REQUIRE(items.size() == 2);
    REQUIRE(items[0]->property("position3D").value<QVector3D>() == QVector3D(1,2,3));
    REQUIRE(items[1]->property("position3D").value<QVector3D>() == QVector3D(4,5,6));

    // Switch to alt position role
    rep->setPositionRole(TestListModel::AltPositionRole);
    REQUIRE(items[0]->property("position3D").value<QVector3D>() == QVector3D(30,20,10));
    REQUIRE(items[1]->property("position3D").value<QVector3D>() == QVector3D(60,50,40));

    delete root;
}


#include "test_cwItem3DRepeater.moc"
