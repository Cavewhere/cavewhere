/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBESANDBOXMODEL_H
#define CWPASSAGETUBESANDBOXMODEL_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPassageTubeCases.h"

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>
#include <QString>

//Std includes
#include <optional>

/**
 * The tube gallery's editable junction: a cwPassageTubeCases::JunctionCase
 * whose arms are the rows. Sections are lists of left, right, up and down in
 * meters. Every edit emits caseChanged; load and setSampleSpacing emit
 * sampleSpacingChanged first.
 *
 * JunctionConflictRole is true when an arm's own reading at J differs at all
 * from the through's; it stands in for a mesh-check JunctionReadingConflict
 * until cwPassageTubeMeshCheck reports one.
 *
 * toInitializer writes a case as the C++ designated initializer
 * galleryCases can append, and fromInitializer reads that text back.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeSandboxModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PassageTubeSandboxModel)
    QML_UNCREATABLE("The tube gallery model owns its sandbox")

    Q_PROPERTY(double throughBendDegrees READ throughBendDegrees WRITE setThroughBendDegrees NOTIFY caseChanged)
    Q_PROPERTY(double throughProfileDegrees READ throughProfileDegrees WRITE setThroughProfileDegrees NOTIFY caseChanged)
    Q_PROPERTY(double throughBendProfileDegrees READ throughBendProfileDegrees WRITE setThroughBendProfileDegrees NOTIFY caseChanged)
    Q_PROPERTY(double sampleSpacing READ sampleSpacing WRITE setSampleSpacing NOTIFY caseChanged)
    Q_PROPERTY(QList<double> throughStart READ throughStart NOTIFY caseChanged)
    Q_PROPERTY(QList<double> throughJunction READ throughJunction NOTIFY caseChanged)
    Q_PROPERTY(QList<double> throughEnd READ throughEnd NOTIFY caseChanged)
    Q_PROPERTY(int selectedArm READ selectedArm WRITE setSelectedArm NOTIFY selectedArmChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PlanDegreesRole,
        ProfileDegreesRole,
        LengthMetersRole,
        HasLrudRole,
        HasJunctionLrudRole,
        FarSectionRole,
        JunctionSectionRole,
        JunctionConflictRole
    };
    Q_ENUM(Role)

    // The section a wall edit lands in: the through's at t0, J and t1, or the
    // arm's at its far station and at J.
    enum Reading {
        ThroughStart,
        ThroughJunction,
        ThroughEnd,
        ArmFar,
        ArmJunction
    };
    Q_ENUM(Reading)

    enum Wall {
        Left,
        Right,
        Up,
        Down
    };
    Q_ENUM(Wall)

    explicit cwPassageTubeSandboxModel(QObject* parent = nullptr);

    static QString sandboxName();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    cwPassageTubeCases::JunctionCase junctionCase() const;
    void load(const cwPassageTubeCases::JunctionCase& junctionCase);

    double throughBendDegrees() const;
    void setThroughBendDegrees(double degrees);

    double throughProfileDegrees() const;
    void setThroughProfileDegrees(double degrees);

    double throughBendProfileDegrees() const;
    void setThroughBendProfileDegrees(double degrees);

    double sampleSpacing() const;
    void setSampleSpacing(double meters);

    QList<double> throughStart() const;
    QList<double> throughJunction() const;
    QList<double> throughEnd() const;

    int selectedArm() const;
    void setSelectedArm(int arm);

    // Appends an arm at the next free plan angle and selects it.
    Q_INVOKABLE void addArm();
    Q_INVOKABLE void removeArm(int arm);

    Q_INVOKABLE void setPlanDegrees(int arm, double degrees);
    Q_INVOKABLE void setProfileDegrees(int arm, double degrees);
    Q_INVOKABLE void setLengthMeters(int arm, double meters);
    Q_INVOKABLE void setHasLrud(int arm, bool hasLrud);
    Q_INVOKABLE void setHasJunctionLrud(int arm, bool hasJunctionLrud);

    // arm is ignored for the through's readings.
    Q_INVOKABLE void setWall(int arm, cwPassageTubeSandboxModel::Reading reading, cwPassageTubeSandboxModel::Wall wall, double meters);

    // Puts toInitializer(junctionCase()) on the clipboard and returns it.
    Q_INVOKABLE QString copyAsCase() const;

    static QString toInitializer(const cwPassageTubeCases::JunctionCase& junctionCase);
    static std::optional<cwPassageTubeCases::JunctionCase> fromInitializer(const QString& text);

signals:
    void caseChanged();
    void sampleSpacingChanged();
    void selectedArmChanged();

private:
    void apply(const cwPassageTubeCases::JunctionCase& changed);
    template<typename Change>
    void editArm(int arm, Change change);

    cwPassageTubeCases::JunctionCase m_case;
    int m_selectedArm = -1;
};

#endif // CWPASSAGETUBESANDBOXMODEL_H
