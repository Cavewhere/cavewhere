#include "cwPipelineManager.h"
#include "cwPipelineComponentFactory.h"

//Qt includes
#include <QDebug>

//QuickQanava
#include <QuickQanava.h>


cwPipelineManager::cwPipelineManager(QObject *parent)
    : QObject(parent),
    m_graph(nullptr),
    m_components(new cwPipelineComponentFactory(this))
{

    // m_graph->setParent(this);
    // m_graph->setObjectName("PipelineManagerGraph");
    // m_graph->setConnectorEnabled(true);
    // m_graph->setConnectorEdgeColor(QColor("violet"));
    // m_graph->setConnectorColor()
}

cwPipelineManager::~cwPipelineManager()
{
    // If the manager owns the objects, deletion could be handled here.
    // Otherwise, assume they will be deleted elsewhere.
}

void cwPipelineManager::addArtifact(cwArtifact *artifact)
{
    qDebug() << "Context object:" << QQmlEngine::contextForObject(this);
    if (artifact != nullptr && !m_artifacts.contains(artifact)) {
        m_artifacts.append(artifact);
        artifact->setParent(this);

        auto component = m_components->createComponent(artifact, qmlEngine(this));
        qDebug() << "Component:" << artifact << component;

        auto node = m_graph->insertNode(component);
        node->setLabel(artifact->name());
        node->getItem()->setProperty("artifact", QVariant::fromValue(artifact));

        node->getItem()->setX(10.0);
        node->getItem()->setY(m_artifacts.size() * (node->getItem()->height() + 10));

        m_nodeToArtifact.insert(node, artifact);

        qDebug() << "Added artifact:" << artifact;
    }
}

void cwPipelineManager::addRule(cwAbstractRule *rule)
{
    if (rule != nullptr && !m_rules.contains(rule)) {
        m_rules.append(rule);
        rule->setParent(this);

        auto component = m_components->createComponent(rule, qmlEngine(this));
        auto node = m_graph->insertNode(component);
        node->setLabel(rule->name());

        node->getItem()->setX(250.0);
        node->getItem()->setY(m_rules.size() * node->getItem()->height());

        m_nodeToRule.insert(node, rule);

        setupRulePorts(rule, node);

        qDebug() << "Added Rule:" << rule;
    }
}

void cwPipelineManager::removeArtifact(cwArtifact *artifact)
{
    m_artifacts.removeAll(artifact);
    // Optionally, disconnect signals related to this artifact.
}

void cwPipelineManager::removeRule(cwAbstractRule *rule)
{
    m_rules.removeAll(rule);
    // Optionally, disconnect signals related to this rule.
}

void cwPipelineManager::buildGraph()
{
    // // Iterate over all rules to create their ports.
    // for (cwAbstractRule *rule : std::as_const(m_rules)) {
    //     setupRulePorts(rule);
    // }

    // Additionally, iterate over artifacts and connect them to rule inputs
    // based on your application logic. This is pseudocode:
    //
    // for (cwArtifact *artifact : m_artifacts) {
    //     for (cwAbstractRule *rule : m_rules) {
    //         // if rule expects artifact data on one of its properties,
    //         // perform the connection.
    //     }
    // }
}

void cwPipelineManager::onPortConnected(QObject *source, QObject *dest)
{
    if (validateConnection(source, dest)) {
        // Establish the connection immediately.
        // For example, if the source is a cwArtifact and dest is a cwAbstractRule input,
        // you could connect the artifact's nameChanged signal to an appropriate slot on the rule.
        //
        // Note: Replace this with the actual signals/slots determined from the meta-properties.
        bool connected = QObject::connect(source, SIGNAL(nameChanged()),
                                          dest, SLOT(setName(QString)));
        if (!connected) {
            qDebug() << "Failed to connect" << source << "to" << dest;
        }
    } else {
        // If the connection is invalid, block or undo it.
        qDebug() << "Invalid connection between" << source << "and" << dest;
        // Optionally, trigger a disconnection:
        QObject::disconnect(source, nullptr, dest, nullptr);
    }
}

void cwPipelineManager::onPortDisconnected(QObject *source, QObject *dest)
{
    // Handle disconnection if necessary.
    QObject::disconnect(source, nullptr, dest, nullptr);
}

bool cwPipelineManager::validateConnection(QObject *source, QObject *dest) const
{
    // Example validation:
    // If connecting an artifact to a rule input, ensure that types are compatible.
    cwArtifact *artifact = qobject_cast<cwArtifact*>(source);
    cwAbstractRule *rule = qobject_cast<cwAbstractRule*>(dest);
    if (artifact != nullptr && rule != nullptr) {
        // Implement your logic here.
        // For instance, you might check if the cwAbstractRule's input property type
        // can accept the cwArtifact data type.
        return true;
    }
    // You can add further validation for rule-to-rule connections.
    return false;
}

void cwPipelineManager::setupRulePorts(cwAbstractRule *rule, qan::Node* node)
{
    if (rule != nullptr) {

        auto insertPort = [node, this, rule](const QMetaProperty& property,
                                             qan::NodeItem::Dock dockType,
                                             qan::PortItem::Type portType)
        {
            auto port = m_graph->insertPort(node,
                                            dockType,
                                            portType,
                                            property.name()
                                            );
            qDebug() << "Port:" << port;
            m_ports.insert(port, {
                                     rule,
                                     property
                                 });
        };

        // Create input ports based on rule's sourceProperties.
        QList<QMetaProperty> inputs = rule->sourceProperties();
        for (const QMetaProperty &prop : std::as_const(inputs)) {
            insertPort(prop,
                       qan::NodeItem::Dock::Left,
                       qan::PortItem::Type::In);
        }

        // Create output ports based on rule's outputProperties.
        QList<QMetaProperty> outputs = rule->outputProperties();
        for (const QMetaProperty &prop : std::as_const(outputs)) {
            insertPort(prop,
                       qan::NodeItem::Dock::Right,
                       qan::PortItem::Type::Out
                       );
        }
    }
}

void cwPipelineManager::setGraph(qan::Graph *newGraph)
{
    if (m_graph == newGraph) {
        return;
    }
    m_graph = newGraph;

    if(m_graph) {
        m_graph->setObjectName("PipelineManagerGraph");
        m_graph->setConnectorEnabled(true);
        m_graph->setConnectorEdgeColor(QColor("black"));
        m_graph->setConnectorColor("lightblue");
        m_graph->setConnectorCreateDefaultEdge(false);

        connect(m_graph, &qan::Graph::connectorRequestEdgeCreation,
                this, [this](qan::Node* src,
                   QObject* dst,
                   qan::PortItem* srcPortItem,
                   qan::PortItem* dstPortItem)
                {
                    qDebug() << "Src:" << src << "Dst:" << dst << "srcPortItem:" << srcPortItem << "dstPortItem" << dstPortItem;
                    auto destNode = qobject_cast<qan::Node*>(dst);
                    if(destNode) {
                        if(m_nodeToArtifact.contains(src) && m_ports.contains(dstPortItem)) {
                            Q_ASSERT(src != nullptr);
                            Q_ASSERT(dst != nullptr);
                            Q_ASSERT(dstPortItem != nullptr);

                           //Source is an artifact
                            cwArtifact* artifact = m_nodeToArtifact.value(src);

                            //Get the port of the destination
                            const auto& port = m_ports.value(dstPortItem);

                            //Make sure we can make the connection
                            //Since we're just setting the whole cwArtifact object as a property to the rule
                            //We don't need to watch for changes.
                            bool success = port.property.write(port.rule, QVariant::fromValue(artifact));

                            if(success) {
                                auto edge = m_graph->insertEdge(src, dst);
                                m_graph->bindEdgeDestination(edge, dstPortItem);
                            }
                        }
                    }
                });
    }

    emit graphChanged();
}
