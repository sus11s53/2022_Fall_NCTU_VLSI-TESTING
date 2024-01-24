#ifndef BRIDGING_FAULTS_H
#define BRIDGING_FAULTS_H

#include "gate.h"
#include "fault.h"

enum BFAULT_TYPE
{
    AND,
    OR
};

// enum BFAULT_STATUS
//{
// UN,
// DE,
// RE,
// AB

//};

class BRIDGING_FAULT : public FAULT
{
private:
    BFAULT_TYPE Ftype;
    GATE *gate1;
    GATE *gate2;
    // BFAULT_STATUS Status;
    bool Branch; // fault is on branch

public:
    BRIDGING_FAULT(GATE *gate1, GATE *gate2, BFAULT_TYPE Fault) : Ftype(Fault), gate1(gate1), gate2(gate2), FAULT(gate1, gate2, S0) {}
    ~BRIDGING_FAULT() {}
    BFAULT_TYPE GetType() { return Ftype; }
    void SetType(BFAULT_TYPE fault) { Ftype = fault; }
    // BFAULT_STATUS GetStatus() { return Status; }
    // void SetStatus(BFAULT_STATUS status) { Status = status; }
    GATE *GetGate1() { return gate1; }
    GATE *GetGate2() { return gate2; }
    // bool Is_Branch() { return Branch; }
};

#endif
