#include "wallsunits.h"
#include "unitizedmath.h"

namespace dewalls {

typedef const Unit<Length> * LengthUnit;
typedef const Unit<Angle>  * AngleUnit;
typedef UnitizedDouble<Length> ULength;
typedef UnitizedDouble<Angle> UAngle;

WallsUnits::WallsUnits()
    : vectorType(VectorType::CT),
      ctOrder(QList<CtElement>({CtElement::D, CtElement::A, CtElement::V})),
      rectOrder(QList<RectElement>({RectElement::E, RectElement::N, RectElement::U})),
      d_unit(Length::meters()),
      s_unit(Length::meters()),
      a_unit(Angle::degrees()),
      ab_unit(Angle::degrees()),
      v_unit(Angle::degrees()),
      vb_unit(Angle::degrees()),
      decl(UAngle(0.0, Angle::degrees())),
      grid(UAngle(0.0, Angle::degrees())),
      rect(UAngle(0.0, Angle::degrees())),
      incd(ULength(0.0, Length::meters())),
      inca(UAngle(0.0, Angle::degrees())),
      incab(UAngle(0.0, Angle::degrees())),
      incv(UAngle(0.0, Angle::degrees())),
      incvb(UAngle(0.0, Angle::degrees())),
      incs(ULength(0.0, Length::meters())),
      inch(ULength(0.0, Length::meters())),
      typeab_corrected(false),
      typeab_tolerance(NAN),
      typeab_no_average(false),
      typevb_corrected(false),
      typevb_tolerance(NAN),
      typevb_no_average(false),
      case_(CaseType::Mixed),
      lrud(LrudType::From),
      lrud_order(QList<LrudElement>({LrudElement::L, LrudElement::R, LrudElement::U, LrudElement::D})),
      tape(QList<TapingMethodElement>({TapingMethodElement::InstrumentHeight, TapingMethodElement::TargetHeight})),
      flag(QString()),
      prefix(QStringList()),
      date(QDate()),
      segment(QString()),
      uvh(1.0),
      uvv(1.0),
      uv(1.0)
{

}

void WallsUnits::setPrefix(int index, QString newPrefix)
{
    if (index < 0 || index > 2)
    {
        throw std::invalid_argument("prefix index out of range");
    }
    while (prefix.size() <= index)
    {
        prefix << QString();
    }
    prefix[index] = newPrefix;

    while (!prefix.isEmpty() && prefix.last().isNull())
    {
        prefix.removeLast();
    }
}

QString WallsUnits::processStationName(QString name)
{
    if (name.isNull())
    {
        return QString();
    }
    name = applyCaseType(case_, name);
    int explicitPrefixCount = name.count(':');
    for (int i = explicitPrefixCount; i < prefix.size(); i++)
    {
        name.prepend(':').prepend(prefix[i]);
    }
    return name;
}

void WallsUnits::rectToCt(ULength north, ULength east, ULength up, ULength& distance, UAngle& azm, UAngle& inc) const
{
    ULength ne2 = usq(north) + usq(east);
    ULength ne = usqrt(ne2); // horizontal offset
    if (!up.isValid()) up = ULength(0, Length::m());

    distance = usqrt(ne2 + usq(up)).in(d_unit);
    azm = uatan2(east, north).in(a_unit);
    if (azm < UAngle(0, Angle::degrees()))
    {
        azm += UAngle(360.0, Angle::degrees());
    }
    inc = uatan2(up, ne).in(v_unit);
}

void WallsUnits::applyHeightCorrections(ULength& dist, UAngle& fsInc, UAngle& bsInc, ULength ih, ULength th) const
{
    if (inch.isNonzero() || ih.isNonzero() || th.isNonzero())
    {
        UAngle inc = avgInc(fsInc + incv, bsInc + incvb);
        if (inc.isValid() && !isVertical(inc))
        {
            // horizontal offset before height correction
            ULength ne = ucos(inc) * (dist + incd);
            // vertical offset before height correction
            ULength u = usin(inc) * (dist + incd);

            u += inch;
            if (ih.isValid()) u += ih;
            if (th.isValid()) u -= th;

            // adjust fsInc/bsInc so that their new avg
            // is the corrected inclination
            UAngle dinc = uatan2(u, ne) - inc;
            fsInc += dinc;
            bsInc += (typevb_corrected ? dinc : -dinc);

            dist = usqrt(usq(ne) + usq(u)) - incd;
        }
    }
}

UAngle WallsUnits::avgInc(UAngle fsInc, UAngle bsInc) const
{
    if (!typevb_corrected)
    {
        bsInc = -bsInc;
    }
    if (!fsInc.isValid())
    {
        return bsInc;
    }
    else if (!bsInc.isValid())
    {
        return fsInc;
    }

    return (fsInc + bsInc) * 0.5;
}

bool WallsUnits::isVertical(UAngle angle) const
{
    return abs(abs(angle.get(Angle::degrees())) - 90.0) < 0.0001;
}

} // namespace dewalls

