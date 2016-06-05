#ifndef FINITE_FEILD
#define FINITE_FEILD
#include "design_pattern.h"
typedef unsigned char byte;
class FiniteField{
    SINGLETON_PATTERN_DECLARATION_H(FiniteField)

private:
    byte ** _mul_table;
    byte * _inv_table;
    byte _mul(byte a, byte b);
public:
    byte add(byte a, byte b);
    byte sub(byte a, byte b);
    byte mul(byte a, byte b);
    byte inv(byte a);
};

#endif
