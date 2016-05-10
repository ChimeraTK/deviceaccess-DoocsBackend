#ifndef EQ_INFO_H
#define	EQ_INFO_H

#include <eq_fct.h>
#include <d_fct.h>

class eq_dummy  : public EqFct {
  public:
    eq_dummy();
    virtual ~eq_dummy();
    
    D_int           prop_someInt;
    D_float         prop_someFloat;
    D_double        prop_someDouble;
    D_string        prop_someString;
    D_status        prop_someStatus;
    D_bit           prop_someBit;
    
    D_intarray      prop_someIntArray;
    D_shortarray    prop_someShortArray;
    D_floatarray    prop_someFloatArray;
    D_doublearray   prop_someDoubleArray;

    D_spectrum      prop_someSpectrum;

    void init();
    void update();

    int fct_code() { return fctCode; }
    
    static constexpr int fctCode = 10;

};

#endif	/* EQ_INFO_H */

