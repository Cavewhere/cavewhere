#ifndef CWKEYWORDVISIBILITY_H
#define CWKEYWORDVISIBILITY_H

//Qt includes
#include <QAbstractItemModel>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>

//Our includes
#include "cwGlobals.h"
class cwKeywordItemModel;
class cwRenderObject;

class CAVEWHERE_LIB_EXPORT cwKeywordVisibility : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KeywordVisibility)

    Q_PROPERTY(QAbstractItemModel* visibleModel READ visibleModel WRITE setVisibleModel NOTIFY visibleModelChanged)
    Q_PROPERTY(QAbstractItemModel* hideModel READ hideModel WRITE setHideModel NOTIFY hideModelChanged)

public:
    explicit cwKeywordVisibility(QObject *parent = nullptr);

    QAbstractItemModel* visibleModel() const;
    void setVisibleModel(QAbstractItemModel* visibleModel);

    QAbstractItemModel* hideModel() const;
    void setHideModel(QAbstractItemModel* hideModel);


signals:
    void visibleModelChanged();
    void hideModelChanged();

private:
    QPointer<QAbstractItemModel> mVisibleModel; //!<
    QPointer<QAbstractItemModel> mHideModel; //!<

    // The hide model's objects, rebuilt on the first lookup after any change
    // to it, so a batch of k lookups costs O(hidden + k) instead of O(hidden * k).
    mutable QSet<QObject*> mHiddenObjects;
    mutable bool mHiddenObjectsDirty = true;

    void connectModel(QAbstractItemModel* model);
    void connectHideModelCache();
    void markHiddenObjectsDirty();
    void resolveVisibility(QObject* object) const;
    bool isHidden(QObject* object) const;

};

inline QAbstractItemModel* cwKeywordVisibility::visibleModel() const {
    return mVisibleModel;
}

inline QAbstractItemModel* cwKeywordVisibility::hideModel() const {
    return mHideModel;
}

#endif // CWKEYWORDVISIBILITY_H
