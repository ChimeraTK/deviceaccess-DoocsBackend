#include "eq_dummy.h"

eq_dummy::eq_dummy()
: EqFct("NAME = location"),
  prop_someString("SOME_STRING", this),
  prop_someInt("SOME_INT",this)
{
}

eq_dummy::~eq_dummy() {
}

void eq_dummy::init() {
   prop_someString.set_value("The quick brown fox jumps over the lazy dog.");
   prop_someInt.set_value(42);
}

void eq_dummy::update() {
}

