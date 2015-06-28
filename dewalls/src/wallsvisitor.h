#ifndef WALLSVISITOR_H
#define WALLSVISITOR_H

#include <QString>
#include <QStringList>
#include <QSharedPointer>
#include "length.h"
#include "angle.h"
#include "unitizeddouble.h"
#include "varianceoverride.h"

namespace dewalls {

class WallsVisitor
{
public:
    typedef UnitizedDouble<Length> ULength;
    typedef UnitizedDouble<Angle> UAngle;
    typedef QSharedPointer<VarianceOverride> VarianceOverridePtr;

    virtual void beginFile( QString source );
    virtual void endFile( QString source );
    virtual void beginVectorLine( );
    virtual void abortVectorLine( );
    virtual void endVectorLine( );
    virtual void beginFixLine( );
    virtual void abortFixLine( );
    virtual void endFixLine( );
    virtual void beginUnitsLine( );
    virtual void abortUnitsLine( );
    virtual void endUnitsLine( );
    virtual void visitFrom( QString from );
    virtual void visitTo( QString to );
    virtual void visitDistance( ULength distance );
    virtual void visitFrontsightAzimuth( UAngle fsAzimuth );
    virtual void visitBacksightAzimuth( UAngle bsAzimuth );
    virtual void visitFrontsightInclination( UAngle fsInclination );
    virtual void visitBacksightInclination( UAngle bsInclination );
    virtual void visitNorth( ULength north );
    virtual void visitLatitude( UAngle latitude );
    virtual void visitEast( ULength east );
    virtual void visitLongitude( UAngle longitude );
    virtual void visitRectUp( ULength up );
    virtual void visitInstrumentHeight( ULength instrumentHeight );
    virtual void visitTargetHeight( ULength targetHeight );
    virtual void visitLeft( ULength left );
    virtual void visitRight( ULength right );
    virtual void visitUp( ULength up );
    virtual void visitDown( ULength down );
    virtual void visitLrudFacingAngle( UAngle facingAngle );
    virtual void visitCFlag( );
    virtual void visitHorizontalVarianceOverride( VarianceOverridePtr variance );
    virtual void visitVerticalVarianceOverride( VarianceOverridePtr variance );
    virtual void visitInlineSegment( QString segment );
    virtual void visitInlineNote( QString note );
    virtual void visitInlineComment( QString string );
    virtual void visitCommentLine( QString comment );
    virtual void visitFlaggedStations( QString flag , QStringList stations );
    virtual void visitBlockCommentLine( QString string );
    virtual void visitNoteLine( QString station , QString note );
    virtual void visitFixedStation( QString string );
};

class PrintingWallsVisitor : public WallsVisitor
{
public:
    virtual void beginFile( QString source );
    virtual void endFile( QString source );
    virtual void beginVectorLine( );
    virtual void abortVectorLine( );
    virtual void endVectorLine( );
    virtual void beginFixLine( );
    virtual void abortFixLine( );
    virtual void endFixLine( );
    virtual void beginUnitsLine( );
    virtual void abortUnitsLine( );
    virtual void endUnitsLine( );
    virtual void visitFrom( QString from );
    virtual void visitTo( QString to );
    virtual void visitDistance( ULength distance );
    virtual void visitFrontsightAzimuth( UAngle fsAzimuth );
    virtual void visitBacksightAzimuth( UAngle bsAzimuth );
    virtual void visitFrontsightInclination( UAngle fsInclination );
    virtual void visitBacksightInclination( UAngle bsInclination );
    virtual void visitNorth( ULength north );
    virtual void visitLatitude( UAngle latitude );
    virtual void visitEast( ULength east );
    virtual void visitLongitude( UAngle longitude );
    virtual void visitRectUp( ULength up );
    virtual void visitInstrumentHeight( ULength instrumentHeight );
    virtual void visitTargetHeight( ULength targetHeight );
    virtual void visitLeft( ULength left );
    virtual void visitRight( ULength right );
    virtual void visitUp( ULength up );
    virtual void visitDown( ULength down );
    virtual void visitLrudFacingAngle( UAngle facingAngle );
    virtual void visitCFlag( );
    virtual void visitHorizontalVarianceOverride( VarianceOverridePtr variance );
    virtual void visitVerticalVarianceOverride( VarianceOverridePtr variance );
    virtual void visitInlineSegment( QString segment );
    virtual void visitInlineNote( QString note );
    virtual void visitInlineComment( QString string );
    virtual void visitCommentLine( QString comment );
    virtual void visitFlaggedStations( QString flag , QStringList stations );
    virtual void visitBlockCommentLine( QString string );
    virtual void visitNoteLine( QString station , QString note );
    virtual void visitFixedStation( QString string );
};

}

#endif // WALLSVISITOR_H

