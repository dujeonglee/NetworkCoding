#ifndef FINITE_FEILD
#define FINITE_FEILD
#include "design_pattern.h"
typedef unsigned char byte;

class FiniteField{
    SINGLETON_PATTERN_DECLARATION_H(FiniteField)
private:
    /**
     * @brief _mul_table: multiplication lookup table
     */
    byte ** _mul_table;
    /**
     * @brief _inv_table: inversion lookup table
     */
    byte * _inv_table;
    /**
     * @brief _mul: multiplication function building "_mul_table"
     */
    byte _mul(byte a, byte b);
public:
    /**
     * @brief add: Adding "a" and "b"
     */
    byte add(byte a, byte b);
    /**
     * @brief sub: Subtracting "b" from "a"
     */
    byte sub(byte a, byte b);
    /**
     * @brief mul: Multiplying "a" and "b"
     */
    byte mul(byte a, byte b);
    /**
     * @brief inv: Calculating inverse of "a"
     */
    byte inv(byte a);
};

#endif
