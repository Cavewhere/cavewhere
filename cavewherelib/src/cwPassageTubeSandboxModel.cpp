/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeSandboxModel.h"

//Qt includes
#include <QClipboard>
#include <QGuiApplication>
#include <QLocale>

//Std includes
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

using Cases = cwPassageTubeCases;
using Section = Cases::Section;
using ArmLeg = Cases::ArmLeg;
using ArmSpec = Cases::ArmSpec;
using JunctionCase = Cases::JunctionCase;

constexpr double kFullTurnDegrees = 360.0;
constexpr double kHalfTurnDegrees = 180.0;
constexpr double kVerticalDegrees = 90.0;
// addArm tries plan angles in these steps and takes the first at least
// kFreePlanDegrees from every passage leaving J.
constexpr double kPlanStepDegrees = 90.0;
constexpr double kFreePlanDegrees = 45.0;
const Section kSandboxThroughSection {1.0, 1.0, 1.5, 1.0};

bool isVertical(double profileDegrees)
{
    return std::abs(profileDegrees) >= kVerticalDegrees;
}

double angleBetween(double a, double b)
{
    const double difference = std::fmod(std::abs(a - b), kFullTurnDegrees);
    return std::min(difference, kFullTurnDegrees - difference);
}

// The plan angles of the passages leaving J, measured like
// ArmSpec::planDegrees; a vertical leg leaves along none.
QVector<double> occupiedPlans(const JunctionCase& junctionCase)
{
    QVector<double> occupied;
    if (!isVertical(junctionCase.throughProfileDegrees)) {
        occupied.append(0.0);
    }
    if (!isVertical(junctionCase.throughProfileDegrees + junctionCase.throughBendProfileDegrees)) {
        occupied.append(kHalfTurnDegrees + junctionCase.throughBendDegrees);
    }
    for (const ArmSpec& arm : junctionCase.arms) {
        if (!isVertical(arm.profileDegrees)) {
            occupied.append(arm.planDegrees);
        }
    }
    return occupied;
}

// The plan angle in (-180, 180], the range of the panel's Plan slider.
double signedPlan(double plan)
{
    const double turned = std::fmod(plan, kFullTurnDegrees);
    return turned > kHalfTurnDegrees ? turned - kFullTurnDegrees : turned;
}

double nextFreePlan(const JunctionCase& junctionCase)
{
    const QVector<double> occupied = occupiedPlans(junctionCase);
    for (double plan = kPlanStepDegrees; plan <= kFullTurnDegrees; plan += kPlanStepDegrees) {
        const bool isFree = std::all_of(occupied.cbegin(), occupied.cend(), [plan](double other) {
            return angleBetween(plan, other) >= kFreePlanDegrees;
        });
        if (isFree) {
            return signedPlan(plan);
        }
    }
    return kPlanStepDegrees;
}

QList<double> wallList(const Section& section)
{
    return {section.left, section.right, section.up, section.down};
}

double* wallOf(Section& section, cwPassageTubeSandboxModel::Wall wall)
{
    switch (wall) {
    case cwPassageTubeSandboxModel::Left:
        return &section.left;
    case cwPassageTubeSandboxModel::Right:
        return &section.right;
    case cwPassageTubeSandboxModel::Up:
        return &section.up;
    case cwPassageTubeSandboxModel::Down:
        return &section.down;
    }
    return nullptr;
}

Section* sectionOf(JunctionCase& junctionCase, int arm, cwPassageTubeSandboxModel::Reading reading)
{
    switch (reading) {
    case cwPassageTubeSandboxModel::ThroughStart:
        return &junctionCase.throughSection;
    case cwPassageTubeSandboxModel::ThroughJunction:
        return &junctionCase.throughAtJunction;
    case cwPassageTubeSandboxModel::ThroughEnd:
        return &junctionCase.throughPastJunction;
    case cwPassageTubeSandboxModel::ArmFar:
    case cwPassageTubeSandboxModel::ArmJunction:
        if (arm < 0 || arm >= junctionCase.arms.size()) {
            return nullptr;
        }
        return reading == cwPassageTubeSandboxModel::ArmFar ? &junctionCase.arms[arm].at : &junctionCase.arms[arm].atJunction;
    }
    return nullptr;
}

// Writing the initializer.

QString numberText(double value)
{
    return QString::number(value, 'g', QLocale::FloatingPointShortest);
}

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString stringText(const QString& text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("QStringLiteral(\"%1\")").arg(escaped);
}

QString sectionText(const Section& section)
{
    return QStringLiteral("{%1, %2, %3, %4}")
        .arg(numberText(section.left), numberText(section.right), numberText(section.up), numberText(section.down));
}

QString legText(const ArmLeg& leg)
{
    return QStringLiteral("{%1, %2, %3, %4}")
        .arg(numberText(leg.turnDegrees), numberText(leg.profileDegrees), numberText(leg.lengthMeters), sectionText(leg.at));
}

template<typename T, typename ToText>
QString listText(const QVector<T>& items, ToText toText)
{
    QStringList texts;
    for (const T& item : items) {
        texts.append(toText(item));
    }
    return QStringLiteral("{") + texts.join(QStringLiteral(", ")) + QStringLiteral("}");
}

// Reading the initializer back: a value is a number, true or false, a
// QStringLiteral, or a braced list, optionally after a type name, of values
// each optionally designated by ".field =".

struct Node {
    enum class Kind {
        Number,
        Text,
        Flag,
        List
    };
    Kind kind = Kind::List;
    double number = 0.0;
    QString text;
    bool flag = false;
    QString designator;
    std::vector<Node> items;
};

class InitializerParser
{
public:
    explicit InitializerParser(const QString& text) :
        m_text(text)
    {
    }

    std::optional<Node> parse()
    {
        std::optional<Node> node = value();
        skipSpace();
        if (!node || m_position != m_text.size()) {
            return std::nullopt;
        }
        return node;
    }

private:
    void skipSpace()
    {
        while (m_position < m_text.size() && m_text.at(m_position).isSpace()) {
            ++m_position;
        }
    }

    bool accept(QChar character)
    {
        skipSpace();
        if (m_position < m_text.size() && m_text.at(m_position) == character) {
            ++m_position;
            return true;
        }
        return false;
    }

    QString identifier()
    {
        skipSpace();
        const qsizetype start = m_position;
        const auto isWordCharacter = [](QChar character) { return character.isLetterOrNumber() || character == QLatin1Char('_'); };
        if (m_position < m_text.size() && (m_text.at(m_position).isLetter() || m_text.at(m_position) == QLatin1Char('_'))) {
            while (m_position < m_text.size() && isWordCharacter(m_text.at(m_position))) {
                ++m_position;
            }
        }
        return m_text.mid(start, m_position - start);
    }

    std::optional<Node> value()
    {
        if (accept(QLatin1Char('{'))) {
            return list();
        }
        const QString name = identifier();
        if (name == QStringLiteral("QStringLiteral")) {
            return quoted();
        }
        if (name == QStringLiteral("true") || name == QStringLiteral("false")) {
            Node node;
            node.kind = Node::Kind::Flag;
            node.flag = name == QStringLiteral("true");
            return node;
        }
        if (!name.isEmpty()) {
            return accept(QLatin1Char('{')) ? list() : std::nullopt;
        }
        return number();
    }

    std::optional<Node> list()
    {
        Node node;
        while (!accept(QLatin1Char('}'))) {
            QString designator;
            if (accept(QLatin1Char('.'))) {
                designator = identifier();
                if (designator.isEmpty() || !accept(QLatin1Char('='))) {
                    return std::nullopt;
                }
            }
            std::optional<Node> item = value();
            if (!item) {
                return std::nullopt;
            }
            item->designator = designator;
            node.items.push_back(*item);
            if (!accept(QLatin1Char(','))) {
                return accept(QLatin1Char('}')) ? std::optional<Node>(node) : std::nullopt;
            }
        }
        return node;
    }

    std::optional<Node> quoted()
    {
        if (!accept(QLatin1Char('(')) || !accept(QLatin1Char('"'))) {
            return std::nullopt;
        }
        Node node;
        node.kind = Node::Kind::Text;
        while (m_position < m_text.size()) {
            QChar character = m_text.at(m_position++);
            if (character == QLatin1Char('"')) {
                return accept(QLatin1Char(')')) ? std::optional<Node>(node) : std::nullopt;
            }
            if (character == QLatin1Char('\\') && m_position < m_text.size()) {
                character = m_text.at(m_position++);
            }
            node.text.append(character);
        }
        return std::nullopt;
    }

    std::optional<Node> number()
    {
        skipSpace();
        const qsizetype start = m_position;
        static const QString numberCharacters = QStringLiteral("0123456789+-.eE");
        while (m_position < m_text.size() && numberCharacters.contains(m_text.at(m_position))) {
            ++m_position;
        }
        bool isNumber = false;
        Node node;
        node.kind = Node::Kind::Number;
        node.number = m_text.mid(start, m_position - start).toDouble(&isNumber);
        return isNumber ? std::optional<Node>(node) : std::nullopt;
    }

    const QString m_text;
    qsizetype m_position = 0;
};

// The item designated name, else the undesignated item at position index;
// null when neither is there.
const Node* field(const Node& node, int index, const char* name)
{
    for (const Node& item : node.items) {
        if (item.designator == QLatin1StringView(name)) {
            return &item;
        }
    }
    if (index < static_cast<int>(node.items.size()) && node.items.at(index).designator.isEmpty()) {
        return &node.items.at(index);
    }
    return nullptr;
}

// Each readInto keeps out's default when node is null and fails on a node of
// the wrong kind.
template<typename T>
bool readValue(const Node* node, Node::Kind kind, T Node::*member, T& out)
{
    if (!node) {
        return true;
    }
    if (node->kind != kind) {
        return false;
    }
    out = node->*member;
    return true;
}

bool readInto(const Node* node, double& out)
{
    return readValue(node, Node::Kind::Number, &Node::number, out);
}

bool readInto(const Node* node, bool& out)
{
    return readValue(node, Node::Kind::Flag, &Node::flag, out);
}

bool readInto(const Node* node, QString& out)
{
    return readValue(node, Node::Kind::Text, &Node::text, out);
}

bool readInto(const Node* node, Section& out)
{
    if (!node) {
        return true;
    }
    return node->kind == Node::Kind::List
           && readInto(field(*node, 0, "left"), out.left)
           && readInto(field(*node, 1, "right"), out.right)
           && readInto(field(*node, 2, "up"), out.up)
           && readInto(field(*node, 3, "down"), out.down);
}

bool readInto(const Node* node, ArmLeg& out)
{
    if (!node) {
        return true;
    }
    return node->kind == Node::Kind::List
           && readInto(field(*node, 0, "turnDegrees"), out.turnDegrees)
           && readInto(field(*node, 1, "profileDegrees"), out.profileDegrees)
           && readInto(field(*node, 2, "lengthMeters"), out.lengthMeters)
           && readInto(field(*node, 3, "at"), out.at);
}

template<typename T>
bool readInto(const Node* node, QList<T>& out)
{
    if (!node) {
        return true;
    }
    if (node->kind != Node::Kind::List) {
        return false;
    }
    out.clear();
    for (const Node& item : node->items) {
        T value {};
        if (!readInto(&item, value)) {
            return false;
        }
        out.append(value);
    }
    return true;
}

bool readInto(const Node* node, ArmSpec& out)
{
    if (!node) {
        return true;
    }
    return node->kind == Node::Kind::List
           && readInto(field(*node, 0, "planDegrees"), out.planDegrees)
           && readInto(field(*node, 1, "profileDegrees"), out.profileDegrees)
           && readInto(field(*node, 2, "lengthMeters"), out.lengthMeters)
           && readInto(field(*node, 3, "at"), out.at)
           && readInto(field(*node, 4, "atJunction"), out.atJunction)
           && readInto(field(*node, 5, "hasJunctionLrud"), out.hasJunctionLrud)
           && readInto(field(*node, 6, "hasLrud"), out.hasLrud)
           && readInto(field(*node, 7, "legsBeyond"), out.legsBeyond);
}

bool readInto(const Node* node, JunctionCase& out)
{
    return node && node->kind == Node::Kind::List
           && readInto(field(*node, 0, "name"), out.name)
           && readInto(field(*node, 1, "tags"), out.tags)
           && readInto(field(*node, 2, "throughSection"), out.throughSection)
           && readInto(field(*node, 3, "throughAtJunction"), out.throughAtJunction)
           && readInto(field(*node, 4, "throughPastJunction"), out.throughPastJunction)
           && readInto(field(*node, 5, "throughBendDegrees"), out.throughBendDegrees)
           && readInto(field(*node, 6, "throughProfileDegrees"), out.throughProfileDegrees)
           && readInto(field(*node, 7, "throughBendProfileDegrees"), out.throughBendProfileDegrees)
           && readInto(field(*node, 8, "arms"), out.arms)
           && readInto(field(*node, 9, "sampleSpacing"), out.sampleSpacing);
}

JunctionCase defaultCase()
{
    JunctionCase junctionCase;
    junctionCase.name = cwPassageTubeSandboxModel::sandboxName();
    junctionCase.tags = {QStringLiteral("gallery")};
    junctionCase.throughSection = kSandboxThroughSection;
    junctionCase.throughAtJunction = kSandboxThroughSection;
    junctionCase.throughPastJunction = kSandboxThroughSection;
    return junctionCase;
}

} // namespace

cwPassageTubeSandboxModel::cwPassageTubeSandboxModel(QObject* parent) :
    QAbstractListModel(parent),
    m_case(defaultCase())
{
}

QString cwPassageTubeSandboxModel::sandboxName()
{
    return QStringLiteral("Sandbox");
}

int cwPassageTubeSandboxModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_case.arms.size());
}

QVariant cwPassageTubeSandboxModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_case.arms.size()) {
        return QVariant();
    }

    const ArmSpec& arm = m_case.arms.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return QStringLiteral("a%1%2").arg(index.row()).arg(0);
    case PlanDegreesRole:
        return arm.planDegrees;
    case ProfileDegreesRole:
        return arm.profileDegrees;
    case LengthMetersRole:
        return arm.lengthMeters;
    case HasLrudRole:
        return arm.hasLrud;
    case HasJunctionLrudRole:
        return arm.hasJunctionLrud;
    case FarSectionRole:
        return QVariant::fromValue(wallList(arm.at));
    case JunctionSectionRole:
        return QVariant::fromValue(wallList(arm.atJunction));
    case JunctionConflictRole:
        return arm.hasLrud && arm.hasJunctionLrud && arm.atJunction != m_case.throughAtJunction;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwPassageTubeSandboxModel::roleNames() const
{
    return {
        {NameRole, QByteArrayLiteral("name")},
        {PlanDegreesRole, QByteArrayLiteral("planDegrees")},
        {ProfileDegreesRole, QByteArrayLiteral("profileDegrees")},
        {LengthMetersRole, QByteArrayLiteral("lengthMeters")},
        {HasLrudRole, QByteArrayLiteral("hasLrud")},
        {HasJunctionLrudRole, QByteArrayLiteral("hasJunctionLrud")},
        {FarSectionRole, QByteArrayLiteral("farSection")},
        {JunctionSectionRole, QByteArrayLiteral("junctionSection")},
        {JunctionConflictRole, QByteArrayLiteral("junctionConflict")},
    };
}

cwPassageTubeCases::JunctionCase cwPassageTubeSandboxModel::junctionCase() const
{
    return m_case;
}

void cwPassageTubeSandboxModel::load(const JunctionCase& junctionCase)
{
    beginResetModel();
    m_case = junctionCase;
    endResetModel();
    setSelectedArm(m_case.arms.isEmpty() ? -1 : 0);
    emit sampleSpacingChanged();
    emit caseChanged();
}

double cwPassageTubeSandboxModel::throughBendDegrees() const
{
    return m_case.throughBendDegrees;
}

void cwPassageTubeSandboxModel::setThroughBendDegrees(double degrees)
{
    JunctionCase changed = m_case;
    changed.throughBendDegrees = degrees;
    apply(changed);
}

double cwPassageTubeSandboxModel::throughProfileDegrees() const
{
    return m_case.throughProfileDegrees;
}

void cwPassageTubeSandboxModel::setThroughProfileDegrees(double degrees)
{
    JunctionCase changed = m_case;
    changed.throughProfileDegrees = degrees;
    apply(changed);
}

double cwPassageTubeSandboxModel::throughBendProfileDegrees() const
{
    return m_case.throughBendProfileDegrees;
}

void cwPassageTubeSandboxModel::setThroughBendProfileDegrees(double degrees)
{
    JunctionCase changed = m_case;
    changed.throughBendProfileDegrees = degrees;
    apply(changed);
}

double cwPassageTubeSandboxModel::sampleSpacing() const
{
    return m_case.sampleSpacing;
}

void cwPassageTubeSandboxModel::setSampleSpacing(double meters)
{
    if (m_case.sampleSpacing == meters || meters <= 0.0) {
        return;
    }
    m_case.sampleSpacing = meters;
    emit sampleSpacingChanged();
    emit caseChanged();
}

QList<double> cwPassageTubeSandboxModel::throughStart() const
{
    return wallList(m_case.throughSection);
}

QList<double> cwPassageTubeSandboxModel::throughJunction() const
{
    return wallList(m_case.throughAtJunction);
}

QList<double> cwPassageTubeSandboxModel::throughEnd() const
{
    return wallList(m_case.throughPastJunction);
}

int cwPassageTubeSandboxModel::selectedArm() const
{
    return m_selectedArm;
}

void cwPassageTubeSandboxModel::setSelectedArm(int arm)
{
    if (arm < -1 || arm >= m_case.arms.size()) {
        arm = -1;
    }
    if (m_selectedArm == arm) {
        return;
    }
    m_selectedArm = arm;
    emit selectedArmChanged();
}

void cwPassageTubeSandboxModel::addArm()
{
    ArmSpec arm;
    arm.planDegrees = nextFreePlan(m_case);
    arm.at = m_case.throughAtJunction;
    arm.atJunction = m_case.throughAtJunction;

    const int row = static_cast<int>(m_case.arms.size());
    beginInsertRows(QModelIndex(), row, row);
    m_case.arms.append(arm);
    endInsertRows();
    setSelectedArm(row);
    emit caseChanged();
}

void cwPassageTubeSandboxModel::removeArm(int arm)
{
    if (arm < 0 || arm >= m_case.arms.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), arm, arm);
    m_case.arms.removeAt(arm);
    endRemoveRows();
    setSelectedArm(std::min(m_selectedArm, static_cast<int>(m_case.arms.size()) - 1));
    emit caseChanged();
}

void cwPassageTubeSandboxModel::setPlanDegrees(int arm, double degrees)
{
    editArm(arm, [degrees](ArmSpec& spec) { spec.planDegrees = degrees; });
}

void cwPassageTubeSandboxModel::setProfileDegrees(int arm, double degrees)
{
    editArm(arm, [degrees](ArmSpec& spec) { spec.profileDegrees = degrees; });
}

void cwPassageTubeSandboxModel::setLengthMeters(int arm, double meters)
{
    editArm(arm, [meters](ArmSpec& spec) { spec.lengthMeters = meters; });
}

void cwPassageTubeSandboxModel::setHasLrud(int arm, bool hasLrud)
{
    editArm(arm, [hasLrud](ArmSpec& spec) { spec.hasLrud = hasLrud; });
}

void cwPassageTubeSandboxModel::setHasJunctionLrud(int arm, bool hasJunctionLrud)
{
    editArm(arm, [hasJunctionLrud](ArmSpec& spec) { spec.hasJunctionLrud = hasJunctionLrud; });
}

void cwPassageTubeSandboxModel::setWall(int arm, Reading reading, Wall wall, double meters)
{
    JunctionCase changed = m_case;
    Section* section = sectionOf(changed, arm, reading);
    double* value = section ? wallOf(*section, wall) : nullptr;
    if (value) {
        *value = std::max(meters, 0.0);
        apply(changed);
    }
}

QString cwPassageTubeSandboxModel::copyAsCase() const
{
    const QString text = toInitializer(m_case);
    if (QClipboard* clipboard = qGuiApp ? QGuiApplication::clipboard() : nullptr) {
        clipboard->setText(text);
    }
    return text;
}

QString cwPassageTubeSandboxModel::toInitializer(const JunctionCase& junctionCase)
{
    QStringList lines;
    lines << QStringLiteral("JunctionCase {")
          << QStringLiteral("    .name = %1,").arg(stringText(junctionCase.name))
          << QStringLiteral("    .tags = %1,").arg(listText(junctionCase.tags, stringText))
          << QStringLiteral("    .throughSection = %1,").arg(sectionText(junctionCase.throughSection))
          << QStringLiteral("    .throughAtJunction = %1,").arg(sectionText(junctionCase.throughAtJunction))
          << QStringLiteral("    .throughPastJunction = %1,").arg(sectionText(junctionCase.throughPastJunction))
          << QStringLiteral("    .throughBendDegrees = %1,").arg(numberText(junctionCase.throughBendDegrees))
          << QStringLiteral("    .throughProfileDegrees = %1,").arg(numberText(junctionCase.throughProfileDegrees))
          << QStringLiteral("    .throughBendProfileDegrees = %1,").arg(numberText(junctionCase.throughBendProfileDegrees))
          << QStringLiteral("    .arms = {");
    for (const ArmSpec& arm : junctionCase.arms) {
        lines << QStringLiteral("        ArmSpec {")
              << QStringLiteral("            .planDegrees = %1,").arg(numberText(arm.planDegrees))
              << QStringLiteral("            .profileDegrees = %1,").arg(numberText(arm.profileDegrees))
              << QStringLiteral("            .lengthMeters = %1,").arg(numberText(arm.lengthMeters))
              << QStringLiteral("            .at = %1,").arg(sectionText(arm.at))
              << QStringLiteral("            .atJunction = %1,").arg(sectionText(arm.atJunction))
              << QStringLiteral("            .hasJunctionLrud = %1,").arg(boolText(arm.hasJunctionLrud))
              << QStringLiteral("            .hasLrud = %1,").arg(boolText(arm.hasLrud))
              << QStringLiteral("            .legsBeyond = %1,").arg(listText(arm.legsBeyond, legText))
              << QStringLiteral("        },");
    }
    lines << QStringLiteral("    },")
          << QStringLiteral("    .sampleSpacing = %1,").arg(numberText(junctionCase.sampleSpacing))
          << QStringLiteral("}");
    return lines.join(QLatin1Char('\n'));
}

std::optional<cwPassageTubeCases::JunctionCase> cwPassageTubeSandboxModel::fromInitializer(const QString& text)
{
    const std::optional<Node> node = InitializerParser(text).parse();
    JunctionCase junctionCase;
    if (!node || !readInto(&*node, junctionCase)) {
        return std::nullopt;
    }
    return junctionCase;
}

void cwPassageTubeSandboxModel::apply(const JunctionCase& changed)
{
    if (changed == m_case) {
        return;
    }
    m_case = changed;
    if (!m_case.arms.isEmpty()) {
        emit dataChanged(index(0), index(static_cast<int>(m_case.arms.size()) - 1));
    }
    emit caseChanged();
}

template<typename Change>
void cwPassageTubeSandboxModel::editArm(int arm, Change change)
{
    if (arm < 0 || arm >= m_case.arms.size()) {
        return;
    }
    JunctionCase changed = m_case;
    change(changed.arms[arm]);
    apply(changed);
}
