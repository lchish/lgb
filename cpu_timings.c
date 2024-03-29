#include "cpu_timings.h"

int t[]  = {//duration in cycles
  1,3,2,2,1,1,2,1,5,2,2,2,1,1,2,1,
  0,3,2,2,1,1,2,1,3,2,2,2,1,1,2,1,
  2,3,2,2,1,1,2,1,2,2,2,2,1,1,2,1,
  2,3,2,2,3,3,3,1,2,2,2,2,1,1,2,1,
  1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
  1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
  1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
  2,2,2,2,2,2,0,2,1,1,1,1,1,1,2,1,
  1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
  1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
  1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
  1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
  2,3,3,4,3,4,2,4,2,4,3,0,3,6,2,4,
  2,3,3,0,3,4,2,4,2,4,3,0,3,0,2,4,
  3,3,1,0,0,4,2,4,4,1,4,0,0,0,2,4,
  3,3,1,1,0,4,2,4,3,2,4,1,0,0,2,4
};

int cb_table[] = {//cb clock cycles
//0,1,2,3,4,5,6  ,7,8,9,A,B,C,D,E, F
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
  2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
  2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
  2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
  2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2
};
