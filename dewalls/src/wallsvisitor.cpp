#include "wallsvisitor.h"

namespace dewalls {

using namespace std;

void WallsVisitor::beginFile( QString source ){}
void WallsVisitor::endFile( QString source ){}
void WallsVisitor::beginVectorLine( ){}
void WallsVisitor::abortVectorLine( ){}
void WallsVisitor::endVectorLine( ){}
void WallsVisitor::beginFixLine( ){}
void WallsVisitor::abortFixLine( ){}
void WallsVisitor::endFixLine( ){}
void WallsVisitor::beginUnitsLine( ){}
void WallsVisitor::abortUnitsLine( ){}
void WallsVisitor::endUnitsLine( ){}
void WallsVisitor::visitFrom( QString from ){}
void WallsVisitor::visitTo( QString to ){}
void WallsVisitor::visitDistance( ULength distance ){}
void WallsVisitor::visitFrontsightAzimuth( UAngle fsAzimuth ){}
void WallsVisitor::visitBacksightAzimuth( UAngle bsAzimuth ){}
void WallsVisitor::visitFrontsightInclination( UAngle fsInclination ){}
void WallsVisitor::visitBacksightInclination( UAngle bsInclination ){}
void WallsVisitor::visitNorth( ULength north ){}
void WallsVisitor::visitLatitude( UAngle latitude ){}
void WallsVisitor::visitEast( ULength east ){}
void WallsVisitor::visitLongitude( UAngle longitude ){}
void WallsVisitor::visitRectUp( ULength up ){}
void WallsVisitor::visitInstrumentHeight( ULength instrumentHeight ){}
void WallsVisitor::visitTargetHeight( ULength targetHeight ){}
void WallsVisitor::visitLeft( ULength left ){}
void WallsVisitor::visitRight( ULength right ){}
void WallsVisitor::visitUp( ULength up ){}
void WallsVisitor::visitDown( ULength down ){}
void WallsVisitor::visitLrudFacingAngle( UAngle facingAngle ){}
void WallsVisitor::visitCFlag( ){}
void WallsVisitor::visitHorizontalVarianceOverride( VarianceOverridePtr variance ){}
void WallsVisitor::visitVerticalVarianceOverride( VarianceOverridePtr variance ){}
void WallsVisitor::visitInlineSegment( QString segment ){}
void WallsVisitor::visitInlineNote( QString note ){}
void WallsVisitor::visitInlineComment( QString string ){}
void WallsVisitor::visitCommentLine( QString comment ){}
void WallsVisitor::visitFlaggedStations( QString flag , QStringList stations ){}
void WallsVisitor::visitBlockCommentLine( QString string ){}
void WallsVisitor::visitNoteLine( QString station , QString note ){}
void WallsVisitor::visitFixedStation( QString string ){}

void PrintingWallsVisitor::beginFile( QString source )
{
    cout << "<BEGIN FILE: " << source.toStdString() << ">" << endl;
}

void PrintingWallsVisitor::endFile( QString source )
{
    cout << "<END FILE:   " << source.toStdString() << ">" << endl;
}

void PrintingWallsVisitor::beginVectorLine( )
{
    cout << "=== begin vector line ===" << endl;
}

void PrintingWallsVisitor::abortVectorLine( )
{
    cout << "=== abort vector line ===" << endl;
}

void PrintingWallsVisitor::endVectorLine( )
{
    cout << "=== end vector line ===" << endl;
}

void PrintingWallsVisitor::beginFixLine( )
{
    cout << "=== begin fix line ===" << endl;
}

void PrintingWallsVisitor::abortFixLine( )
{
    cout << "=== abort fix line ===" << endl;
}

void PrintingWallsVisitor::endFixLine( )
{
    cout << "=== end fix line ===" << endl;
}

void PrintingWallsVisitor::beginUnitsLine( )
{
    cout << "=== begin units line ===" << endl;
}

void PrintingWallsVisitor::abortUnitsLine( )
{
    cout << "=== abort units line ===" << endl;
}

void PrintingWallsVisitor::endUnitsLine( )
{
    cout << "=== end units line ===" << endl;
}

void PrintingWallsVisitor::visitFrom( QString from )
{
    cout << "  from:         " << from.toStdString() << endl;
}

void PrintingWallsVisitor::visitTo( QString to )
{
    cout << "  to:           " << to.toStdString() << endl;
}

void PrintingWallsVisitor::visitDistance( ULength distance )
{
    cout << "  distance:     " << distance << endl;
}

void PrintingWallsVisitor::visitFrontsightAzimuth( UAngle fsAzimuth )
{
    cout << "  fsAzm:        " << fsAzimuth << endl;
}

void PrintingWallsVisitor::visitBacksightAzimuth( UAngle bsAzimuth )
{
    cout << "  bsAzm:        " << bsAzimuth << endl;
}

void PrintingWallsVisitor::visitFrontsightInclination( UAngle fsInclination )
{
    cout << "  fsInc:        " << fsInclination << endl;
}

void PrintingWallsVisitor::visitBacksightInclination( UAngle bsInclination )
{
    cout << "  bsInc:        " << bsInclination << endl;
}

void PrintingWallsVisitor::visitNorth( ULength north )
{
    cout << "  north:        " << north << endl;
}

void PrintingWallsVisitor::visitLatitude( UAngle latitude )
{
    cout << "  latitude:     " << latitude << endl;
}

void PrintingWallsVisitor::visitEast( ULength east )
{
    cout << "  east:         " << east << endl;
}

void PrintingWallsVisitor::visitLongitude( UAngle longitude )
{
    cout << "  longitude:    " << longitude << endl;
}

void PrintingWallsVisitor::visitRectUp( ULength up )
{
    cout << "  vUp:          " << up << endl;
}

void PrintingWallsVisitor::visitInstrumentHeight( ULength instrumentHeight )
{
    cout << "  ih:           " << instrumentHeight << endl;
}

void PrintingWallsVisitor::visitTargetHeight( ULength targetHeight )
{
    cout << "  th:           " << targetHeight << endl;
}

void PrintingWallsVisitor::visitLeft( ULength left )
{
    cout << "  left:         " << left << endl;
}

void PrintingWallsVisitor::visitRight( ULength right )
{
    cout << "  right:        " << right << endl;
}

void PrintingWallsVisitor::visitUp( ULength up )
{
    cout << "  up:           " << up << endl;
}

void PrintingWallsVisitor::visitDown( ULength down )
{
    cout << "  down:         " << down << endl;
}

void PrintingWallsVisitor::visitLrudFacingAngle( UAngle facingAngle )
{
    cout << "  facingAngle:  " << facingAngle << endl;
}

void PrintingWallsVisitor::visitCFlag( )
{
    cout << "  cflag" << endl;
}

void PrintingWallsVisitor::visitHorizontalVarianceOverride( VarianceOverridePtr variance )
{
    cout << "  h:            " << variance->toString().toStdString() << endl;
}

void PrintingWallsVisitor::visitVerticalVarianceOverride( VarianceOverridePtr variance )
{
    cout << "  v:            " << variance->toString().toStdString() << endl;
}

void PrintingWallsVisitor::visitInlineSegment( QString segment )
{
    cout << "  segment:      " << segment.toStdString() << endl;
}

void PrintingWallsVisitor::visitInlineNote( QString note )
{
    cout << "  note:         " << note.toStdString() << endl;
}

void PrintingWallsVisitor::visitInlineComment( QString comment )
{
    cout << "  comment:      " << comment.toStdString() << endl;
}

void PrintingWallsVisitor::visitCommentLine( QString comment )
{
    cout << "comment:        " << comment.toStdString() << endl;
}

void PrintingWallsVisitor::visitFlaggedStations( QString flag , QStringList stations )
{
    cout << "flag stations: TODO" << endl;
}

void PrintingWallsVisitor::visitBlockCommentLine( QString comment )
{
    cout << ";" << comment.toStdString() << endl;
}

void PrintingWallsVisitor::visitNoteLine( QString station , QString note )
{
    cout << "note: " << station.toStdString() << ": " << note.toStdString() << endl;
}

void PrintingWallsVisitor::visitFixedStation( QString station )
{
    cout << "  fixed station:" << station.toStdString() << endl;
}

} // namespace dewalls

