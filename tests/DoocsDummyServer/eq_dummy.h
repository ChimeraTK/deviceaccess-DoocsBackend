#ifndef EQ_INFO_H
#define	EQ_INFO_H

#include <eq_fct.h>
#include <d_fct.h>

class eq_dummy  : public EqFct {
  public:
    eq_dummy();
    virtual ~eq_dummy();
    
    D_string    prop_someString;
    D_int       prop_someInt;
    
    void init();
    void update();

    int fct_code() { return fctCode; }
    
    static constexpr int fctCode = 10;

};

#endif	/* EQ_INFO_H */
