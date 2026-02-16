#pragma once

#include <QObject>
#include <QHash>
#include <QQmlEngine>

#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwRemoteBindingStore : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteBindingStore)
    QML_UNCREATABLE("Access via RootData.remoteBindingStore")

public:
    explicit cwRemoteBindingStore(QObject* parent = nullptr);

    void bindRemoteToAccount(const QString& remoteUrl, const QString& accountId);
    QString accountIdForRemote(const QString& remoteUrl) const;
    void removeBindingsForAccount(const QString& accountId);
    void clear();

    static QString canonicalRemoteKey(const QString& remoteUrl);

private:
    void bindRemoteKeyToAccount(const QString& remoteKey, const QString& accountId);
    QString accountIdForRemoteKey(const QString& remoteKey) const;
    void loadFromSettings();
    void saveToSettings() const;

    QHash<QString, QString> m_remoteKeyToAccountId;
};
