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
void WallsVisitor::visitInlineSegment( Segment segment ){}
void WallsVisitor::visitInlineNote( QString note ){}
void WallsVisitor::visitInlineComment( QString string ){}
void WallsVisitor::visitCommentLine( QString comment ){}
void WallsVisitor::visitFlaggedStations( QString flag , QStringList stations ){}
void WallsVisitor::visitBlockCommentLine( QString string ){}
void WallsVisitor::visitNoteLine( QString station , QString note ){}
void WallsVisitor::visitDateLine( QDate date ){}
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

void PrintingWallsVisitor::visitInlineSegment( Segment segment )
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

void PrintingWallsVisitor::visitDateLine( QDate date )
{
    cout << "date: " << date.toString().toStdString() << endl;
}

void PrintingWallsVisitor::visitFixedStation( QString station )
{
    cout << "  fixed station:" << station.toStdString() << endl;
}

void CapturingWallsVisitor::beginFile( QString source )
{
}

void CapturingWallsVisitor::endFile( QString source )
{
}

void CapturingWallsVisitor::beginVectorLine( )
{
    clear();
}

void CapturingWallsVisitor::abortVectorLine( )
{
    clear();
}

void CapturingWallsVisitor::endVectorLine( )
{
}

void CapturingWallsVisitor::beginFixLine( )
{
    clear();
}

void CapturingWallsVisitor::abortFixLine( )
{
    clear();
}

void CapturingWallsVisitor::endFixLine( )
{
}

void CapturingWallsVisitor::beginUnitsLine( )
{
    clear();
}

void CapturingWallsVisitor::abortUnitsLine( )
{
    clear();
}

void CapturingWallsVisitor::endUnitsLine( )
{
}

void CapturingWallsVisitor::visitFrom( QString _from )
{
    from = _from;
}

void CapturingWallsVisitor::visitTo( QString _to )
{
    to = _to;
}

void CapturingWallsVisitor::visitDistance( ULength distance )
{
    this->distance = distance;
}

void CapturingWallsVisitor::visitFrontsightAzimuth( UAngle fsAzimuth )
{
    frontsightAzimuth = fsAzimuth;
}

void CapturingWallsVisitor::visitBacksightAzimuth( UAngle bsAzimuth )
{
    backsightAzimuth = bsAzimuth;
}

void CapturingWallsVisitor::visitFrontsightInclination( UAngle fsInclination )
{
    frontsightInclination = fsInclination;
}

void CapturingWallsVisitor::visitBacksightInclination( UAngle bsInclination )
{
    backsightInclination = bsInclination;
}

void CapturingWallsVisitor::visitNorth( ULength _north )
{
    north = _north;
}

void CapturingWallsVisitor::visitLatitude( UAngle _latitude )
{
    latitude = _latitude;
}

void CapturingWallsVisitor::visitEast( ULength _east )
{
    east = _east;
}

void CapturingWallsVisitor::visitLongitude( UAngle _longitude )
{
    longitude = _longitude;
}

void CapturingWallsVisitor::visitRectUp( ULength up )
{
    rectUp = up;
}

void CapturingWallsVisitor::visitInstrumentHeight( ULength _instrumentHeight )
{
    instrumentHeight = _instrumentHeight;
}

void CapturingWallsVisitor::visitTargetHeight( ULength _targetHeight )
{
    targetHeight = _targetHeight;
}

void CapturingWallsVisitor::visitLeft( ULength _left )
{
    left = _left;
}

void CapturingWallsVisitor::visitRight( ULength _right )
{
    right = _right;
}

void CapturingWallsVisitor::visitUp( ULength _up )
{
    up = _up;
}

void CapturingWallsVisitor::visitDown( ULength _down )
{
    down = _down;
}

void CapturingWallsVisitor::visitLrudFacingAngle( UAngle facingAngle )
{
    lrudFacingAngle = facingAngle;
}

void CapturingWallsVisitor::visitCFlag( )
{
    cflag = true;
}

void CapturingWallsVisitor::visitHorizontalVarianceOverride( VarianceOverridePtr variance )
{
    horizontalVarianceOverride = variance;
}

void CapturingWallsVisitor::visitVerticalVarianceOverride( VarianceOverridePtr variance )
{
    verticalVarianceOverride = variance;
}

void CapturingWallsVisitor::visitInlineSegment( Segment segment )
{
    inlineSegment = segment;
}

void CapturingWallsVisitor::visitInlineNote( QString note )
{
    inlineNote = note;
}

void CapturingWallsVisitor::visitInlineComment( QString comment )
{
    inlineComment = comment;
}

void CapturingWallsVisitor::visitCommentLine( QString _comment )
{
    comment = _comment;
}

void CapturingWallsVisitor::visitFlaggedStations( QString _flag , QStringList stations )
{
    flag = _flag;
    flaggedStations = stations;
}

void CapturingWallsVisitor::visitBlockCommentLine( QString comment )
{
    blockCommentLine = comment;
}

void CapturingWallsVisitor::visitNoteLine( QString station , QString _note )
{
    note = _note;
    notedStation = station;
}

void CapturingWallsVisitor::visitDateLine( QDate _date )
{
    date = _date;
}

void CapturingWallsVisitor::visitFixedStation( QString station )
{
    fixedStation = station;
}

MultiWallsVisitor::MultiWallsVisitor(QList<WallsVisitor*> visitors)
    : _visitors(visitors)
{
}

void MultiWallsVisitor::beginFile( QString source ){  multicast(&WallsVisitor::beginFile, source); }
void MultiWallsVisitor::endFile( QString source ){  multicast(&WallsVisitor::endFile, source); }
void MultiWallsVisitor::beginVectorLine( ){  multicast(&WallsVisitor::beginVectorLine); }
void MultiWallsVisitor::abortVectorLine( ){  multicast(&WallsVisitor::endVectorLine); }
void MultiWallsVisitor::endVectorLine( ){  multicast(&WallsVisitor::endVectorLine); }
void MultiWallsVisitor::beginFixLine( ){  multicast(&WallsVisitor::beginFixLine); }
void MultiWallsVisitor::abortFixLine( ){  multicast(&WallsVisitor::abortFixLine); }
void MultiWallsVisitor::endFixLine( ){  multicast(&WallsVisitor::endFixLine); }
void MultiWallsVisitor::beginUnitsLine( ){  multicast(&WallsVisitor::beginUnitsLine); }
void MultiWallsVisitor::abortUnitsLine( ){  multicast(&WallsVisitor::abortUnitsLine); }
void MultiWallsVisitor::endUnitsLine( ){  multicast(&WallsVisitor::endUnitsLine); }
void MultiWallsVisitor::visitFrom( QString from ){  multicast(&WallsVisitor::visitFrom, from ); }
void MultiWallsVisitor::visitTo( QString to ){  multicast(&WallsVisitor::visitTo, to); }
void MultiWallsVisitor::visitDistance( ULength distance ){  multicast(&WallsVisitor::visitDistance, distance); }
void MultiWallsVisitor::visitFrontsightAzimuth( UAngle fsAzimuth ){  multicast(&WallsVisitor::visitFrontsightAzimuth, fsAzimuth); }
void MultiWallsVisitor::visitBacksightAzimuth( UAngle bsAzimuth ){  multicast(&WallsVisitor::visitBacksightAzimuth, bsAzimuth); }
void MultiWallsVisitor::visitFrontsightInclination( UAngle fsInclination ){  multicast(&WallsVisitor::visitFrontsightInclination, fsInclination); }
void MultiWallsVisitor::visitBacksightInclination( UAngle bsInclination ){  multicast(&WallsVisitor::visitBacksightInclination, bsInclination); }
void MultiWallsVisitor::visitNorth( ULength north ){  multicast(&WallsVisitor::visitNorth, north); }
void MultiWallsVisitor::visitLatitude( UAngle latitude ){  multicast(&WallsVisitor::visitLatitude, latitude); }
void MultiWallsVisitor::visitEast( ULength east ){  multicast(&WallsVisitor::visitEast, east); }
void MultiWallsVisitor::visitLongitude( UAngle longitude ){  multicast(&WallsVisitor::visitLongitude, longitude); }
void MultiWallsVisitor::visitRectUp( ULength up ){  multicast(&WallsVisitor::visitRectUp, up); }
void MultiWallsVisitor::visitInstrumentHeight( ULength instrumentHeight ){  multicast(&WallsVisitor::visitInstrumentHeight, instrumentHeight); }
void MultiWallsVisitor::visitTargetHeight( ULength targetHeight ){  multicast(&WallsVisitor::visitTargetHeight, targetHeight); }
void MultiWallsVisitor::visitLeft( ULength left ){  multicast(&WallsVisitor::visitLeft, left); }
void MultiWallsVisitor::visitRight( ULength right ){  multicast(&WallsVisitor::visitRight, right); }
void MultiWallsVisitor::visitUp( ULength up ){  multicast(&WallsVisitor::visitUp, up); }
void MultiWallsVisitor::visitDown( ULength down ){  multicast(&WallsVisitor::visitDown, down); }
void MultiWallsVisitor::visitLrudFacingAngle( UAngle facingAngle ){  multicast(&WallsVisitor::visitLrudFacingAngle, facingAngle); }
void MultiWallsVisitor::visitCFlag( ){  multicast(&WallsVisitor::visitCFlag); }
void MultiWallsVisitor::visitHorizontalVarianceOverride( VarianceOverridePtr variance ){  multicast(&WallsVisitor::visitHorizontalVarianceOverride, variance); }
void MultiWallsVisitor::visitVerticalVarianceOverride( VarianceOverridePtr variance ){  multicast(&WallsVisitor::visitVerticalVarianceOverride, variance); }
void MultiWallsVisitor::visitInlineSegment( Segment segment ){  multicast(&WallsVisitor::visitInlineSegment, segment); }
void MultiWallsVisitor::visitInlineNote( QString note ){  multicast(&WallsVisitor::visitInlineNote, note); }
void MultiWallsVisitor::visitInlineComment( QString string ){  multicast(&WallsVisitor::visitInlineComment, string); }
void MultiWallsVisitor::visitCommentLine( QString comment ){  multicast(&WallsVisitor::visitCommentLine, comment); }
void MultiWallsVisitor::visitFlaggedStations( QString flag , QStringList stations ){  multicast(&WallsVisitor::visitFlaggedStations, flag, stations); }
void MultiWallsVisitor::visitBlockCommentLine( QString string ){  multicast(&WallsVisitor::visitBlockCommentLine, string); }
void MultiWallsVisitor::visitNoteLine( QString station , QString note ){  multicast(&WallsVisitor::visitNoteLine, station, note); }
void MultiWallsVisitor::visitDateLine( QDate date ){  multicast(&WallsVisitor::visitDateLine, date); }
void MultiWallsVisitor::visitFixedStation( QString string ){  multicast(&WallsVisitor::visitFixedStation, string); }

} // namespace dewalls

