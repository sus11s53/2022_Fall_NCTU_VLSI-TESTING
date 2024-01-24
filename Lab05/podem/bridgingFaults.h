#ifndef BRIDGING_FAULTS_H
#define BRIDGING_FAULTS_H

#include "gate.h"

enum BFAULT_TYPE
{
    AND,
    OR
};

enum BFAULT_STATUS
{
    UNCHECKED,
    CHECKED,
    NOUSE,
    ABANDON

};

class BRIDGING_FAULT
{
private:
    BFAULT_TYPE Fault;
    GATE *gate1;
    GATE *gate2;
    BFAULT_STATUS Status;
    bool Branch; // fault is on branch

public:
    BRIDGING_FAULT(GATE *gate1, GATE *gate2, BFAULT_TYPE Fault) : Fault(Fault), gate1(gate1), gate2(gate2), Status(UNCHECKED) {}
    ~BRIDGING_FAULT() {}
    BFAULT_TYPE GetType() { return Fault; }
    void SetStatus(BFAULT_TYPE fault) { Fault = fault; }
    BFAULT_STATUS GetStatus() { return Status; }
    void SetStatus(BFAULT_STATUS status) { Status = status; }
    GATE *GetGate1() { return gate1; }
    GATE *GetGate2() { return gate2; }
    bool Is_Branch() { return Branch; }
};

#endif
