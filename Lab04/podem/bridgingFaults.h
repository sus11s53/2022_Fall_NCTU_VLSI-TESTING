#ifndef BRIDGING_FAULTS_H
#define BRIDGING_FAULTS_H

#include "gate.h"

enum BFAULT_TYPE
{
    AND,
    OR
};

class BRIDGING_FAULT
{
private:
    GATE *gate1;
    GATE *gate2;
    BFAULT_TYPE fault;

public:
    BRIDGING_FAULT(GATE *gate1, GATE *gate2, BFAULT_TYPE fault)
    {
        this->gate1 = gate1;
        this->gate2 = gate2;
        this->fault = fault;
    }
    string output()
    {
        string out = "(" + gate1->GetName() + ", " + gate2->GetName() + ", " + (fault ? "OR" : "AND") + ")";
        return out;
    }
};

#endif
