*DECK DF2O
      DOUBLE PRECISION FUNCTION DF2O (X)
C***BEGIN PROLOGUE  DF2O
C***PURPOSE  Subsidiary to
C***LIBRARY   SLATEC
C***AUTHOR  (UNKNOWN)
C***ROUTINES CALLED  (NONE)
C***REVISION HISTORY  (YYMMDD)
C   ??????  DATE WRITTEN
C   891214  Prologue converted to Version 4.0 format.  (BAB)
C***END PROLOGUE  DF2O
      DOUBLE PRECISION X
C***FIRST EXECUTABLE STATEMENT  DF2O
      DF2O = 0.0D+00
      IF(X.NE.0.0D+00) DF2O = 0.1D+01/(X*X*SQRT(X))
      RETURN
      END