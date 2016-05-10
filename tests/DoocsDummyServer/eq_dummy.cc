#include "eq_dummy.h"

eq_dummy::eq_dummy()
: EqFct("NAME = location"),
  prop_someInt("SOME_INT",this),
  prop_someFloat("SOME_FLOAT",this),
  prop_someDouble("SOME_DOUBLE",this),
  prop_someString("SOME_STRING", this),
  prop_someStatus("SOME_STATUS",this),
  prop_someBit("SOME_BIT",0,&prop_someStatus.stat_,this),
  prop_someIntArray("SOME_INT_ARRAY", 42, this),
  prop_someShortArray("SOME_SHORT_ARRAY", 5, this),
  prop_someFloatArray("SOME_FLOAT_ARRAY", 5, this),
  prop_someDoubleArray("SOME_DOUBLE_ARRAY", 5, this)
{
}

eq_dummy::~eq_dummy() {
}

void eq_dummy::init() {
    prop_someInt.set_value(42);
    prop_someFloat.set_value(3.1415);
    prop_someDouble.set_value(2.8);
    prop_someString.set_value("The quick brown fox jumps over the lazy dog.");
    prop_someStatus.set_value(3);
    for(int i=0; i<42; i++) prop_someIntArray.set_value(3*i+120, i);
    for(int i=0; i<5; i++) prop_someShortArray.set_value(10+i, i);
    for(int i=0; i<5; i++) prop_someFloatArray.set_value((float)i/1000., i);
    for(int i=0; i<5; i++) prop_someDoubleArray.set_value((float)i/333., i);
}

void eq_dummy::update() {
}

