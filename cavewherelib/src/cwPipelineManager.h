#ifndef CWPIPELINEMANAGER_H
#define CWPIPELINEMANAGER_H

//Qt includes
#include <QObject>
#include <QList>
#include <QMetaProperty>
#include <QQmlEngine>

//Our includes
#include "cwArtifact.h"
#include "cwAbstractRule.h"

//QuickQanava
#include <QuickQanava.h>

/**
 * @brief The cwPipelineManager class manages cwArtifact and cwAbstractRule objects,
 * creating QuickQanava nodes with ports based on rule properties, and wiring
 * connections as the graph updates.
 */
class cwPipelineManager : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(PipelineManager)

    // Q_PROPERTY(qan::Graph* graph READ graph CONSTANT)
    Q_PROPERTY(qan::Graph* graph READ graph WRITE setGraph NOTIFY graphChanged FINAL REQUIRED)

public:
    explicit cwPipelineManager(QObject *parent = nullptr);
    ~cwPipelineManager();

    // Add and remove pipeline elements. Ownership is optional; objects may be created elsewhere.
    Q_INVOKABLE void addArtifact(cwArtifact *artifact);
    Q_INVOKABLE void addRule(cwAbstractRule *rule);

    void removeArtifact(cwArtifact *artifact);
    void removeRule(cwAbstractRule *rule);

    // Rebuild the graph from the current set of artifacts and rules.
    void buildGraph();

    qan::Graph *graph() const { return m_graph; }

    void setGraph(qan::Graph *newGraph);

signals:
    // Optionally, you could emit signals when the graph is updated.

    void graphChanged();

public slots:
    // Slots that handle QuickQanava updates. For example, when a user connects two ports.
    void onPortConnected(QObject *source, QObject *dest);
    void onPortDisconnected(QObject *source, QObject *dest);

private:
    struct Port {
        cwAbstractRule* rule;
        QMetaProperty property;
    };

    // Validates a proposed connection between source and destination objects.
    bool validateConnection(QObject *source, QObject *dest) const;

    // Creates input and output ports for a given rule based on its Q_PROPERTY lists.
    void setupRulePorts(cwAbstractRule *rule, qan::Node *node);

    qan::Graph* m_graph;

    QList<cwArtifact*> m_artifacts;
    QList<cwAbstractRule*> m_rules;

    QHash<qan::Node*, cwArtifact*> m_nodeToArtifact;
    QHash<qan::Node*, cwAbstractRule*> m_nodeToRule;
    QHash<qan::PortItem*, Port> m_ports;

};

#endif // CWPIPELINEMANAGER_H
