//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTransformUpdater.h"
#include "cwCamera.h"

//Qt includes
#include <QQuickItem>
#include <QVector3D>

namespace {

    constexpr int kViewportSize = 1000;

    //One model unit sideways - moves the projection well past a pixel without
    //pushing the point off screen
    constexpr qreal kPanDistance = 1.0;

    /**
     * Minimal stand-in for a station delegate. cwTransformUpdater reads
     * "position3D" off the item, connects to position3DChanged(), and writes
     * "inFrustum" (false once the point leaves the view frustum). Real delegates
     * compose inFrustum into their own visible binding; this one just records it.
     */
    class TestPositionItem : public QQuickItem
    {
        Q_OBJECT
        Q_PROPERTY(QVector3D position3D READ position3D WRITE setPosition3D NOTIFY position3DChanged)
        Q_PROPERTY(bool inFrustum READ inFrustum WRITE setInFrustum NOTIFY inFrustumChanged)

    public:
        explicit TestPositionItem(const QVector3D& position3D = QVector3D())
            : m_position3D(position3D)
        {
        }

        QVector3D position3D() const { return m_position3D; }

        void setPosition3D(const QVector3D& position3D)
        {
            if(m_position3D == position3D) {
                return;
            }
            m_position3D = position3D;
            emit position3DChanged();
        }

        bool inFrustum() const { return m_inFrustum; }

        void setInFrustum(bool inFrustum)
        {
            if(m_inFrustum == inFrustum) {
                return;
            }
            m_inFrustum = inFrustum;
            emit inFrustumChanged();
        }

    signals:
        void position3DChanged();
        void inFrustumChanged();

    private:
        QVector3D m_position3D;
        bool m_inFrustum = true;
    };

    /**
     * A delegate that never declares `inFrustum` - the updater has to tolerate it
     * rather than crash, and warn that the point won't be hidden.
     */
    class TestPositionItemNoInFrustum : public QQuickItem
    {
        Q_OBJECT
        Q_PROPERTY(QVector3D position3D READ position3D CONSTANT)

    public:
        explicit TestPositionItemNoInFrustum(const QVector3D& position3D)
            : m_position3D(position3D)
        {
        }

        QVector3D position3D() const { return m_position3D; }

    private:
        QVector3D m_position3D;
    };

    /**
     * QQuickItem::isVisible() is the *effective* visibility, which is false for
     * any item without a parent. Real delegates are parented into the view, so
     * the tests need a visible root for the visibility checks to mean anything.
     */
    class TestItemRoot : public QQuickItem
    {
    public:
        TestPositionItem* addPoint(const QVector3D& position3D)
        {
            auto item = new TestPositionItem(position3D);
            item->setParentItem(this);
            return item;
        }
    };

    cwCamera* makeCenteredCamera(QObject* parent)
    {
        auto camera = new cwCamera(parent);

        cwProjection projection;
        projection.setPerspective(55, 1.0, 1.0, 10000);

        camera->setProjection(projection);
        camera->setViewport(QRect(0, 0, kViewportSize, kViewportSize));
        camera->setViewMatrix(QMatrix4x4()); //Default view looking down the -z axis

        return camera;
    }
}

TEST_CASE("cwTransformUpdater reports points inside the frustum as inFrustum", "[cwTransformUpdater]") {
    QObject parent;
    cwTransformUpdater updater;
    updater.setCamera(makeCenteredCamera(&parent));

    TestItemRoot root;
    TestPositionItem* inFront = root.addPoint(QVector3D(0, 0, -10)); //Down the -z axis, dead center
    updater.addPointItem(inFront);

    CHECK(inFront->inFrustum() == true);
}

TEST_CASE("cwTransformUpdater reports points behind the camera as not inFrustum", "[cwTransformUpdater]") {
    QObject parent;
    cwTransformUpdater updater;
    updater.setCamera(makeCenteredCamera(&parent));

    TestItemRoot root;
    TestPositionItem* behind = root.addPoint(QVector3D(0, 0, 10)); //Behind the camera
    updater.addPointItem(behind);

    CHECK(behind->inFrustum() == false);
}

TEST_CASE("cwTransformUpdater never writes a point's visible", "[cwTransformUpdater]") {
    QObject parent;
    cwCamera* camera = makeCenteredCamera(&parent);

    cwTransformUpdater updater;
    updater.setCamera(camera);

    TestItemRoot root;
    TestPositionItem* behind = root.addPoint(QVector3D(0, 0, 10)); //Behind the camera
    updater.addPointItem(behind);

    //The item's owner is the only writer of visible. The updater used to set it
    //here, which raced whatever binding the owner had installed - a C++ setter
    //doesn't remove a QML binding, so the binding won on its next evaluation and
    //the cull was silently defeated.
    REQUIRE(behind->inFrustum() == false);
    CHECK(behind->isVisible() == true);

    //Still true after a camera move, which re-runs every point
    QMatrix4x4 panned;
    panned.translate(kPanDistance, 0.0, 0.0);
    camera->setViewMatrix(panned);
    CHECK(behind->isVisible() == true);
}

TEST_CASE("cwTransformUpdater still positions a point that has no inFrustum property", "[cwTransformUpdater]") {
    QObject parent;
    cwTransformUpdater updater;
    updater.setCamera(makeCenteredCamera(&parent));

    TestItemRoot root;
    //Off to one side so the projection lands somewhere other than the origin
    TestPositionItemNoInFrustum item(QVector3D(1, 0, -10));
    item.setParentItem(&root);

    updater.addPointItem(&item); //Warns, but must not crash or skip positioning

    CHECK(item.position() != QPointF(0, 0));
}

TEST_CASE("cwTransformUpdater stops following the camera while disabled", "[cwTransformUpdater]") {
    QObject parent;
    cwCamera* camera = makeCenteredCamera(&parent);

    cwTransformUpdater updater;
    updater.setCamera(camera);

    TestItemRoot root;
    TestPositionItem* inFront = root.addPoint(QVector3D(0, 0, -10));
    updater.addPointItem(inFront);

    REQUIRE(inFront->inFrustum() == true);
    const QPointF originalPosition = inFront->position();

    //cwItem3DRepeater turns this off while the points are hidden, so panning the
    //camera behind a hidden note reprojects nothing.
    updater.setEnabled(false);

    QMatrix4x4 panned;
    panned.translate(kPanDistance, 0.0, 0.0);
    camera->setViewMatrix(panned);
    CHECK(inFront->position() == originalPosition);

    //Re-enabling has to resync everything the camera moved past
    updater.setEnabled(true);
    CHECK(inFront->position() != originalPosition);
}

TEST_CASE("cwTransformUpdater tolerates points added without a camera", "[cwTransformUpdater]") {
    cwTransformUpdater updater; //No camera set

    TestItemRoot root;
    TestPositionItem* item = root.addPoint(QVector3D(0, 0, -10));
    updater.addPointItem(item);

    //Nothing to project against yet, so the point keeps its declared position
    CHECK(item->position() == QPointF(0, 0));

    updater.setCamera(makeCenteredCamera(&updater));
    CHECK(item->inFrustum() == true);
}

#include "test_cwTransformUpdater.moc"
