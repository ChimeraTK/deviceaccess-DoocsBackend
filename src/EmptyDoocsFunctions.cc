#include <eq_fct.h>

void interrupt_usr1_prolog(int) {}

void interrupt_usr1_epilog(int) {}

void interrupt_usr2_prolog(void) {}

void interrupt_usr2_epilog(void) {}

void post_init_prolog(void) {}

void post_init_epilog(void) {}

void eq_cancel(void) {}

void refresh_prolog() {}

void refresh_epilog() {}

void eq_init_prolog() {}

void eq_init_epilog() {}

EqFct *eq_create(int, void *) { return nullptr; }
