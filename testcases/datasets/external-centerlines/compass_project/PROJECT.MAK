/ Mini Compass project: three DATs and a fourth, DELTA.DAT, that is
/ listed and absent. ALPHA fixes A1, BRAVO links to ALPHA through A5,
/ and CHARLIE fixes its own C1. Tape lengths are decimal feet.
@500000.000,4000000.000,100.000,16,0.000;
&WGS 1984;
$16;
#ALPHA.DAT,A1[m,500000.000,4000000.000,100.000];
#BRAVO.DAT,A5;
#CHARLIE.DAT,C1[m,500200.000,4000000.000,100.000];
#DELTA.DAT;
