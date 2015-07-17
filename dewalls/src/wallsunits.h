#ifndef DEWALLS_WALLSUNITS_H
#define DEWALLS_WALLSUNITS_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>
#include "unit.h"
#include "unitizeddouble.h"
#include "length.h"
#include "angle.h"
#include "wallstypes.h"
#include "math.h"

namespace dewalls {

class WallsUnits
{
public:
    typedef const Unit<Length> * LengthUnit;
    typedef const Unit<Angle>  * AngleUnit;
    typedef UnitizedDouble<Length> ULength;
    typedef UnitizedDouble<Angle> UAngle;

    WallsUnits();

    VectorType vectorType;
    QList<CtElement> ctOrder;
    QList<RectElement> rectOrder;
    LengthUnit d_unit;
    LengthUnit s_unit;
    AngleUnit a_unit;
    AngleUnit ab_unit;
    AngleUnit v_unit;
    AngleUnit vb_unit;
    UAngle decl;
    UAngle grid;
    UAngle rect;
    ULength incd;
    UAngle inca;
    UAngle incab;
    UAngle incv;
    UAngle incvb;
    ULength incs;
    ULength inch;
    bool typeab_corrected;
    double typeab_tolerance;
    bool typeab_no_average;
    bool typevb_corrected;
    double typevb_tolerance;
    bool typevb_no_average;
    CaseType case_;
    LrudType lrud;
    QList<LrudElement> lrud_order;
    QList<TapingMethodElement> tape;
    QString flag;
    QStringList prefix;
    QDate date;
    QString segment;
    double uvh;
    double uvv;
    double uv;

    void setPrefix(int index, QString newPrefix);
    QString processStationName(QString name);

    void rectToCt(ULength north, ULength east, ULength up, ULength& distance, UAngle& azm, UAngle& inc) const;
    void applyHeightCorrections(ULength& dist, UAngle& fsInc, UAngle& bsInc, ULength ih, ULength th) const;
    UAngle avgInc(UAngle fsInc, UAngle bsInc) const;
    bool isVertical(UAngle angle) const;
};

} // namespace dewalls

#endif // DEWALLS_WALLSUNITS_H
