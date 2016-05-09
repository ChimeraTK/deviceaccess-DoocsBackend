#include "eq_dummy.h"

eq_dummy::eq_dummy()
: EqFct("NAME = location"),
  prop_someInt("SOME_INT",this),
  prop_someFloat("SOME_FLOAT",this),
  prop_someString("SOME_STRING", this),
  prop_someIntArray("SOME_INT_ARRAY", 42, this)
{
}

eq_dummy::~eq_dummy() {
}

void eq_dummy::init() {
    prop_someInt.set_value(42);
    prop_someFloat.set_value(3.1415);
    prop_someString.set_value("The quick brown fox jumps over the lazy dog.");
    for(int i=0; i<42; i++) prop_someIntArray.set_value(3*i+120, i);
}

void eq_dummy::update() {
}

