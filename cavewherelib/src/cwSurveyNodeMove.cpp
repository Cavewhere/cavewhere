/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//"Move to…" on the survey tree: which subjects may move, where they may land,
//and the one undo command that carries a node or trip across a scope boundary.

//Our includes
#include "cwSurveyNode.h"
#include "cwCavingRegion.h"
#include "cwEquateModel.h"
#include "cwFixStationModel.h"
#include "cwStation.h"
#include "cwTrip.h"

//Std includes

namespace {

//The station names of \a trips, keyed canonically, each with the spelling its
//first trip writes it in. Scoped trips name their stations under a prefix, so
//only flat trips share the node's own namespace.
QHash<QString, QString> flatStationNames(const QList<cwTrip*>& trips, const cwTrip* excluded)
{
    QHash<QString, QString> names;
    for(const cwTrip* trip : trips) {
        if(trip == excluded || trip->isScoped()) {
            continue;
        }

        const QList<cwStation> stations = trip->stations();
        for(const cwStation& station : stations) {
            const QString name = station.name().trimmed();
            if(!name.isEmpty()) {
                names.insert(cwStation::canonicalKey(name), name);
            }
        }
    }
    return names;
}

//The part of \a name after "<label>.", empty when \a name lives outside that
//label's scope.
QString tailUnderLabel(const QString& name, const QString& label)
{
    if(label.isEmpty()) {
        return QString();
    }

    const QString prefix = cwStation::canonicalKey(label) + QStringLiteral(".");
    const QString trimmed = name.trimmed();
    if(!cwStation::canonicalKey(trimmed).startsWith(prefix)) {
        return QString();
    }
    return trimmed.mid(prefix.size());
}

bool holdsACave(const cwSurveyNode* node)
{
    bool found = false;
    node->walk([&found](const cwSurveyNode* descendant) {
        found = found || descendant->kind() == cwSurveyNode::Kind::Cave;
    });
    return found;
}

QString subjectName(const QObject* subject)
{
    if(const auto* node = qobject_cast<const cwSurveyNode*>(subject)) {
        return node->name();
    }
    if(const auto* trip = qobject_cast<const cwTrip*>(subject)) {
        return trip->name();
    }
    return QString();
}

}

bool cwSurveyNode::isMovable(const QObject* subject)
{
    if(const auto* node = qobject_cast<const cwSurveyNode*>(subject)) {
        return !node->isRoot()
                && node->isListedByParent()
                && node->sourceRoot() == nullptr
                && !node->externallyBacked();
    }

    if(const auto* trip = qobject_cast<const cwTrip*>(subject)) {
        const cwSurveyNode* parent = trip->parentNode();
        return InsertRemoveTrip::isListed(trip)
                && parent->sourceRoot() == nullptr
                && !trip->externallyBacked()
                && trip->externalCenterline().isEmpty()
                && !trip->isScoped();
    }

    return false;
}

QString cwSurveyNode::moveRefusal(QObject* subject) const
{
    if(!isMovable(subject)) {
        return tr("Survey data from an attached file stays where the file puts it");
    }

    if(sourceRoot() != nullptr || externallyBacked()) {
        return tr("An attached survey file is read-only");
    }

    const cwSurveyNode* subjectParent = nullptr;
    const cwCavingRegion* subjectRegion = nullptr;
    if(const auto* node = qobject_cast<const cwSurveyNode*>(subject)) {
        if(ancestorsOrSelf().contains(node)) {
            return tr("A node can't move into itself");
        }
        if(!takesCaves() && holdsACave(node)) {
            return tr("A cave can't hold a cave");
        }
        subjectParent = node->parentNode();
        subjectRegion = node->parentRegion();
    } else {
        const auto* trip = qobject_cast<const cwTrip*>(subject);
        if(takesCaves()) {
            return tr("Trips go in a cave or a section");
        }
        subjectParent = trip->parentNode();
        subjectRegion = subjectParent->parentRegion();
    }

    if(subjectParent == this) {
        return tr("Already here");
    }

    if(subjectRegion == nullptr || subjectRegion != parentRegion()) {
        return tr("Not in this project");
    }

    return QString();
}

cwSurveyNode::MovePlan cwSurveyNode::planMove(const QObject* subject) const
{
    MovePlan plan;

    if(const auto* node = qobject_cast<const cwSurveyNode*>(subject)) {
        plan.source = node->parentNode();
        plan.leavingLabel = plan.source->childScopeLabels().value(node->id());

        const QList<cwFixStation>& fixes = plan.source->fixStations()->fixStations();
        for(int row = 0; row < fixes.size(); ++row) {
            if(!tailUnderLabel(fixes.at(row).stationName(), plan.leavingLabel).isEmpty()) {
                plan.leavingFixRows.append(row);
            }
        }
        return plan;
    }

    const auto* trip = qobject_cast<const cwTrip*>(subject);
    plan.source = trip->parentNode();

    const QList<cwTrip*> moving({const_cast<cwTrip*>(trip)});
    const QHash<QString, QString> tripNames = flatStationNames(moving, nullptr);
    const QHash<QString, QString> leftBehind = flatStationNames(plan.source->trips(), trip);
    const QHash<QString, QString> here = flatStationNames(m_trips, nullptr);

    const cwCavingRegion* region = parentRegion();

    for(auto it = tripNames.cbegin(); it != tripNames.cend(); ++it) {
        if(here.contains(it.key())) {
            ++plan.joinedCount;
        }

        if(!leftBehind.contains(it.key())) {
            plan.leavingNames.insert(it.key());
            continue;
        }

        const cwStationHandle behind(cwStationHandle::NativeCave, plan.source->id(), it.value());
        const cwStationHandle moved(cwStationHandle::NativeCave, id(), it.value());
        if(region == nullptr || !region->isTied(behind, moved)) {
            plan.ties.append(cwEquate(QList<cwStationHandle>({behind, moved})));
        }
    }

    //A station already fixed here keeps that fix, so a second one is not
    //carried in to fight it.
    const QList<cwFixStation>& fixes = plan.source->fixStations()->fixStations();
    for(int row = 0; row < fixes.size(); ++row) {
        const QString name = fixes.at(row).stationName().trimmed();
        if(plan.leavingNames.contains(cwStation::canonicalKey(name))
                && m_fixStations->indexOf(name) < 0) {
            plan.leavingFixRows.append(row);
        }
    }

    return plan;
}

QString cwSurveyNode::moveConsequences(QObject* subject) const
{
    if(!moveRefusal(subject).isEmpty()) {
        return QString();
    }

    const MovePlan plan = planMove(subject);

    QStringList sentences;
    const auto appendCounted = [&sentences](qsizetype count, const QString& one, const QString& many) {
        if(count == 1) {
            sentences.append(one);
        } else if(count > 1) {
            sentences.append(many);
        }
    };

    appendCounted(plan.ties.size(),
                  tr("1 station shared with %1 becomes a tie.").arg(plan.source->name()),
                  tr("%1 stations shared with %2 become ties.").arg(plan.ties.size()).arg(plan.source->name()));
    appendCounted(plan.joinedCount,
                  tr("1 station joins the same-named station in %1.").arg(name()),
                  tr("%1 stations join same-named stations in %2.").arg(plan.joinedCount).arg(name()));
    appendCounted(plan.leavingFixRows.size(),
                  tr("1 fix station moves with it."),
                  tr("%1 fix stations move with it.").arg(plan.leavingFixRows.size()));

    if(sentences.isEmpty()) {
        return QString();
    }
    return tr("Move %1 to %2?").arg(subjectName(subject), name())
            + QStringLiteral(" ") + sentences.join(QStringLiteral(" "));
}

bool cwSurveyNode::moveHere(QObject* subject)
{
    if(!moveRefusal(subject).isEmpty()) {
        return false;
    }

    pushUndo(new MoveToCommand(subject, this, planMove(subject)));
    return true;
}

cwSurveyNode::MoveTripCommand::MoveTripCommand(cwTrip* trip, cwSurveyNode* newParent) :
    TripPtr(trip),
    OldParentPtr(trip->parentNode()),
    NewParentPtr(newParent),
    Remove(trip->parentNode(),
           trip->parentNode()->indexOf(trip),
           trip->parentNode()->indexOf(trip)),
    Insert(newParent, trip, newParent->tripCount()),
    OldName(trip->name())
{
    setText(QStringLiteral("Move %1").arg(trip->name()));
}

void cwSurveyNode::MoveTripCommand::renameWhileUnlisted(const cwSanitizedNameSet& siblingNames,
                                                        const QString& desiredName)
{
    TripPtr->Name = siblingNames.deduplicateName(desiredName);
    TripPtr->updateKeywordMetadata();
}

void cwSurveyNode::MoveTripCommand::redo()
{
    const QString previousName = TripPtr->name();

    Remove.redo();
    renameWhileUnlisted(NewParentPtr->tripNameSet(), previousName);
    Insert.redo();

    if(TripPtr->name() != previousName) {
        emit TripPtr->nameChanged();
        emit TripPtr->scopeChanged();
    }
}

void cwSurveyNode::MoveTripCommand::undo()
{
    const QString previousName = TripPtr->name();

    Insert.undo();
    renameWhileUnlisted(OldParentPtr->tripNameSet(), OldName);
    Remove.undo();

    if(TripPtr->name() != previousName) {
        emit TripPtr->nameChanged();
        emit TripPtr->scopeChanged();
    }
}

cwSurveyNode::MoveToCommand::MoveToCommand(QObject* subject,
                                           cwSurveyNode* destination,
                                           const MovePlan& plan) :
    SubjectPtr(subject),
    SourcePtr(plan.source),
    DestinationPtr(destination),
    Region(destination->parentRegion()),
    Plan(plan)
{
    if(auto* node = qobject_cast<cwSurveyNode*>(subject)) {
        Move = std::make_unique<MoveNodeCommand>(node, destination, destination->childNodeCount());
    } else {
        Move = std::make_unique<MoveTripCommand>(qobject_cast<cwTrip*>(subject), destination);
    }

    setText(QStringLiteral("Move %1 to %2").arg(subjectName(subject), destination->name()));
}

void cwSurveyNode::MoveToCommand::finishPlan()
{
    const auto* movedNode = qobject_cast<const cwSurveyNode*>(SubjectPtr);
    const QString arrivingLabel = movedNode != nullptr
            ? DestinationPtr->childScopeLabels().value(movedNode->id())
            : QString();

    //The name a leaving station takes on the destination's side, empty when
    //\a name stays behind.
    const auto arrivingName = [&](const QString& name) -> QString {
        if(movedNode != nullptr) {
            const QString tail = tailUnderLabel(name, Plan.leavingLabel);
            return tail.isEmpty() ? QString() : arrivingLabel + QStringLiteral(".") + tail;
        }
        return Plan.leavingNames.contains(cwStation::canonicalKey(name.trimmed())) ? name : QString();
    };

    if(Region != nullptr) {
        const QList<cwEquate>& equates = Region->equates()->equates();
        for(const cwEquate& before : equates) {
            QList<cwStationHandle> stations = before.stations();
            bool rekeyed = false;
            for(cwStationHandle& handle : stations) {
                if(handle.scope() != cwStationHandle::NativeCave
                        || handle.containerId() != SourcePtr->id()) {
                    continue;
                }

                const QString arriving = arrivingName(handle.tail());
                if(!arriving.isEmpty()) {
                    handle.setContainerId(DestinationPtr->id());
                    handle.setTail(arriving);
                    rekeyed = true;
                }
            }

            if(rekeyed) {
                EquateEdits.append({before, cwEquate(stations)});
            }
        }
    }

    const QList<cwFixStation>& fixes = SourcePtr->fixStations()->fixStations();
    for(int row : std::as_const(Plan.leavingFixRows)) {
        const cwFixStation& before = fixes.at(row);
        cwFixStation after = before;
        after.setStationName(arrivingName(before.stationName()).trimmed());
        FixEdits.append({row, before, after});
    }

    Planned = true;
}

void cwSurveyNode::MoveToCommand::redo()
{
    Move->redo();

    if(!Planned) {
        finishPlan();
    }

    if(Region != nullptr) {
        cwEquateModel* equates = Region->equates();
        //Found by value, like undo: the tables can change between undo and redo.
        for(const EquateEdit& edit : std::as_const(EquateEdits)) {
            equates->replaceAt(equates->equates().indexOf(edit.before), edit.after);
        }
        for(const cwEquate& tie : std::as_const(Plan.ties)) {
            equates->appendEquate(tie);
        }
    }

    cwFixStationModel* sourceFixes = SourcePtr->fixStations();
    for(const FixEdit& edit : std::as_const(FixEdits)) {
        sourceFixes->removeAt(sourceFixes->indexOf(edit.before.id()));
    }
    for(const FixEdit& edit : std::as_const(FixEdits)) {
        DestinationPtr->fixStations()->appendFixStation(edit.after);
    }
}

void cwSurveyNode::MoveToCommand::undo()
{
    cwFixStationModel* destinationFixes = DestinationPtr->fixStations();
    for(auto it = FixEdits.crbegin(); it != FixEdits.crend(); ++it) {
        destinationFixes->removeAt(destinationFixes->indexOf(it->after.id()));
    }
    for(const FixEdit& edit : std::as_const(FixEdits)) {
        SourcePtr->fixStations()->insertFixStation(edit.sourceRow, edit.before);
    }

    //Found by value rather than by row: a tie made by hand after the move is
    //not on the undo stack, and it can sit where the row numbers point.
    if(Region != nullptr) {
        cwEquateModel* equates = Region->equates();
        for(auto it = Plan.ties.crbegin(); it != Plan.ties.crend(); ++it) {
            equates->removeAt(equates->equates().lastIndexOf(*it));
        }
        for(const EquateEdit& edit : std::as_const(EquateEdits)) {
            equates->replaceAt(equates->equates().indexOf(edit.after), edit.before);
        }
    }

    Move->undo();
}
