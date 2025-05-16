// cwAbstractRule.h
#ifndef CWABSTRACTRULE_H
#define CWABSTRACTRULE_H

#include <QObject>
#include <QList>
#include <QObjectBindableProperty>

//Our includes
// #include "cwArtifact.h"

/**
 * @brief The cwAbstractRule class is the base class for all pipeline rules
 *
 * Rules process input artifacts and produce output artifacts.
 * When input artifacts change, the rule processes them automatically.
 */
class cwAbstractRule : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged BINDABLE bindableEnabled)
    Q_PROPERTY(bool valid READ isValid WRITE setValid NOTIFY validChanged BINDABLE bindableValid)

public:
    explicit cwAbstractRule(QObject *parent = nullptr);
    virtual ~cwAbstractRule();

    QString name() const;
    void setName(const QString &name);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    QBindable<bool> bindableEnabled() { return QBindable<bool>(&m_enabled); }

    bool isValid() const { return m_valid; }
    void setValid(bool valid) { m_valid = valid; }
    QBindable<bool> bindableValid() { return QBindable<bool>(&m_valid); }

    QList<QMetaProperty> sourceProperties() const { return properties(sourcesNames()); }
    QList<QMetaProperty> outputProperties() const { return properties(outputsNames()); }

protected:
    // Instead of abstract add/remove functions, you can use properties for sources/outputs
    // Derived classes will override these functions to provide property names
    virtual QList<QByteArray> sourcesNames() const = 0;
    virtual QList<QByteArray> outputsNames() const = 0;

signals:
    void nameChanged(const QString &name);
    void enabledChanged(bool enabled);
    void validChanged(bool valid);
    // void executionError(const QString &errorMessage);

private:
    QString m_name;
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwAbstractRule, bool, m_enabled, true, &cwAbstractRule::enabledChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwAbstractRule, bool, m_valid, false, &cwAbstractRule::validChanged)

    QList<QMetaProperty> properties(const QList<QByteArray>& propertyNames) const;
};

#endif
