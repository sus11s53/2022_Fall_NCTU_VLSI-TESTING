#include <iostream>
#include <vector>
#include "circuit.h"
#include "gate.h"
#include "bridgingFaults.h"
#include <iterator>
#include "GetLongOpt.h"
using namespace std;

extern GetLongOpt option;

void CIRCUIT::GenerateBridgingFaults() //(string a)
{
    // ofstream OutputStrm;
    // string y = a;
    // OutputStrm.open(y, ios::out);
    list<GATE *> Levellist;
    cout << "Generate bridging fault list" << endl;
    register unsigned i;
    unsigned level = 0;
    GATEPTR gptr;
    GATEPTR gate1, gate2;
    BRIDGING_FAULT *bri1, *bri2;
    while (level <= (MaxLevel - 1))
    {
        for (i = 0; i < No_Gate(); i++)
        {
            gptr = Netlist[i];
            if (gptr->GetLevel() == level)
            {
                Levellist.push_back(gptr);
                // cout << gptr->GetName() << " ";
            }
        }
        // cout << "level = " << level << endl;
        level = level + 1;
    }
    while (Levellist.size() > 1)
    {
        gate1 = Levellist.front();
        Levellist.pop_front();
        gate2 = Levellist.front();
        if (gate1->GetLevel() == gate2->GetLevel())
        {
            bri1 = new BRIDGING_FAULT(gate1, gate2, AND);
            BFlist.push_back(bri1);
            bri2 = new BRIDGING_FAULT(gate1, gate2, OR);
            BFlist.push_back(bri2);
        }
    }
}

void CIRCUIT::BridgingFaultCov()
{
    FAULT *fptr;
    BRIDGING_FAULT *bfptr;
    GATEPTR gate1, gate2;
    string a, b, c;
    for (unsigned i = 0; i < BFlist.size(); ++i)
    {
        bfptr = BFlist[i];
        gate1 = bfptr->GetGate1();
        gate2 = bfptr->GetGate2();

        // bridging fault is not active (1,1) or (0,0)
        if (gate1->GetValue() == gate2->GetValue())
        {
            continue;
        }
        c = "(" + gate1->GetName() + "," + gate2->GetName() + "," + ((bfptr->GetType() == AND) ? "AND" : "OR") + ")";
        Dlist.emplace(c);
        switch (bfptr->GetType())
        {
        case AND:
            if (gate1->GetValue() == S0)
            {
                fptr = new FAULT(gate2, gate2, S0);
                BTFlist.emplace_back(fptr);
            }
            else
            {
                fptr = new FAULT(gate1, gate1, S0);
                BTFlist.emplace_back(fptr);
            }
            break;
        case OR:
            if (gate1->GetValue() == S1)
            {
                fptr = new FAULT(gate2, gate2, S1);
                BTFlist.emplace_back(fptr);
            }
            else
            {
                fptr = new FAULT(gate1, gate1, S1);
                BTFlist.emplace_back(fptr);
            }
            break;
        default:
            break;
        }
    }
    BTF2list = BTFlist;
    cout << BTFlist.size() << endl;
    for (unsigned j = 0; j < BTFlist.size(); ++j)
    {
        b = (BTFlist[j]->GetValue()) ? "1" : "0";
        a = "(" + BTFlist[j]->GetInputGate()->GetName() + "," + BTFlist[j]->GetOutputGate()->GetName() + "," + b + ")";
        cout << a << endl;
    }

    cout << "/" << endl;
    for (auto iter = Dlist.begin(); iter != Dlist.end(); ++iter)
    {
        cout << *iter << endl;
    }

    BTFlist.clear();
}

void CIRCUIT::BridgingFaultSimVectors()
{
    cout << "Run stuck-at fault simulation" << endl;
    unsigned pattern_num(0);
    unsigned s = 0;
    unsigned detected_num = 0;
    unsigned undetected_num = 0;
    if (!Pattern.eof())
    { // Readin the first vector
        while (!Pattern.eof())
        {
            ++pattern_num;
            Pattern.ReadNextPattern();
            // fault-free simulation
            SchedulePI();
            LogicSim();
            BridgingFaultCov();
            // single pattern parallel fault simulation
            BridgingFaultSim();
        }
    }

    undetected_num = BFlist.size() - Dlist.size();
    cout.setf(ios::fixed);
    cout.precision(2);
    cout << "---------------------------------------" << endl;
    cout << "Test pattern number = " << pattern_num << endl;
    cout << "---------------------------------------" << endl;
    cout << "Total fault number = " << BFlist.size() << endl;
    cout << "Detected fault number = " << Dlist.size() << endl;
    cout << "Undetected fault number = " << undetected_num << endl;
    cout << "---------------------------------------" << endl;
    cout << "Fault Coverge = " << 100 * Dlist.size() / double(BFlist.size()) << "%" << endl;
    cout << "---------------------------------------" << endl;
    return;
}

void CIRCUIT::BridgingFaultSim()
{
    register unsigned i, fault_idx(0);
    GATEPTR gptr;
    FAULT *fptr;
    FAULT *simulate_flist[PatternNum];
    list<GATEPTR>::iterator gite;
    // initial all gates
    for (i = 0; i < Netlist.size(); ++i)
    {
        Netlist[i]->SetFaultFreeValue();
    }

    // for all undetected faults
    vector<FAULT *>::iterator fite;
    for (fite = BTF2list.begin(); fite != BTF2list.end(); ++fite)
    {
        fptr = *fite;
        // skip redundant and detected faults
        if (fptr->GetStatus() == REDUNDANT || fptr->GetStatus() == DETECTED)
        {
            continue;
        }
        // the fault is not active
        if (fptr->GetValue() == fptr->GetInputGate()->GetValue())
        {
            continue;
        }
        // the fault can be directly seen
        gptr = fptr->GetInputGate();
        if (gptr->GetFlag(OUTPUT) && (!fptr->Is_Branch() || fptr->GetOutputGate()->GetFunction() == G_PO))
        {
            fptr->SetStatus(DETECTED);
            continue;
        }
        if (!fptr->Is_Branch())
        { // stem
            if (!gptr->GetFlag(FAULTY))
            {
                gptr->SetFlag(FAULTY);
                GateStack.push_front(gptr);
            }
            InjectFaultValue(gptr, fault_idx, fptr->GetValue());
            gptr->SetFlag(FAULT_INJECTED);
            ScheduleFanout(gptr);
            simulate_flist[fault_idx++] = fptr;
        }
        else
        { // branch
            if (!CheckFaultyGate(fptr))
            {
                continue;
            }
            gptr = fptr->GetOutputGate();
            // if the fault is shown at an output, it is detected
            if (gptr->GetFlag(OUTPUT))
            {
                fptr->SetStatus(DETECTED);
                continue;
            }
            if (!gptr->GetFlag(FAULTY))
            {
                gptr->SetFlag(FAULTY);
                GateStack.push_front(gptr);
            }
            // add the fault to the simulated list and inject it
            VALUE fault_type = gptr->Is_Inversion() ? NotTable[fptr->GetValue()] : fptr->GetValue();
            InjectFaultValue(gptr, fault_idx, fault_type);
            gptr->SetFlag(FAULT_INJECTED);
            ScheduleFanout(gptr);
            simulate_flist[fault_idx++] = fptr;
        }

        // collect PatternNum fault, do fault simulation
        if (fault_idx == PatternNum)
        {
            // do parallel fault simulation
            for (i = 0; i <= MaxLevel; ++i)
            {
                while (!Queue[i].empty())
                {
                    gptr = Queue[i].front();
                    Queue[i].pop_front();
                    gptr->ResetFlag(SCHEDULED);
                    FaultSimEvaluate(gptr);
                }
            }

            // check detection and reset wires' faulty values
            // back to fault-free values
            for (gite = GateStack.begin(); gite != GateStack.end(); ++gite)
            {
                gptr = *gite;
                // clean flags
                gptr->ResetFlag(FAULTY);
                gptr->ResetFlag(FAULT_INJECTED);
                gptr->ResetFaultFlag();
                if (gptr->GetFlag(OUTPUT))
                {
                    for (i = 0; i < fault_idx; ++i)
                    {
                        if (simulate_flist[i]->GetStatus() == DETECTED)
                        {
                            continue;
                        }
                        // faulty value != fault-free value && fault-free != X &&
                        // faulty value != X (WireValue1[i] == WireValue2[i])
                        if (gptr->GetValue() != VALUE(gptr->GetValue1(i)) && gptr->GetValue() != X && gptr->GetValue1(i) == gptr->GetValue2(i))
                        {
                            simulate_flist[i]->SetStatus(DETECTED);
                        }
                    }
                }
                gptr->SetFaultFreeValue();
            } // end for GateStack
            GateStack.clear();
            fault_idx = 0;
        } // end fault simulation
    }     // end for all undetected faults

    // fault sim rest faults
    if (fault_idx)
    {
        // do parallel fault simulation
        for (i = 0; i <= MaxLevel; ++i)
        {
            while (!Queue[i].empty())
            {
                gptr = Queue[i].front();
                Queue[i].pop_front();
                gptr->ResetFlag(SCHEDULED);
                FaultSimEvaluate(gptr);
            }
        }

        // check detection and reset wires' faulty values
        // back to fault-free values
        for (gite = GateStack.begin(); gite != GateStack.end(); ++gite)
        {
            gptr = *gite;
            // clean flags
            gptr->ResetFlag(FAULTY);
            gptr->ResetFlag(FAULT_INJECTED);
            gptr->ResetFaultFlag();
            if (gptr->GetFlag(OUTPUT))
            {
                for (i = 0; i < fault_idx; ++i)
                {
                    if (simulate_flist[i]->GetStatus() == DETECTED)
                    {
                        continue;
                    }
                    // faulty value != fault-free value && fault-free != X &&
                    // faulty value != X (WireValue1[i] == WireValue2[i])
                    if (gptr->GetValue() != VALUE(gptr->GetValue1(i)) && gptr->GetValue() != X && gptr->GetValue1(i) == gptr->GetValue2(i))
                    {
                        simulate_flist[i]->SetStatus(DETECTED);
                    }
                }
            }
            gptr->SetFaultFreeValue();
        } // end for GateStack
        GateStack.clear();
        fault_idx = 0;
    } // end fault simulation

    // remove detected faults
    for (fite = BTF2list.begin(); fite != BTF2list.end();)
    {
        fptr = *fite;
        if (fptr->GetStatus() == DETECTED || fptr->GetStatus() == REDUNDANT)
        {
            fite = BTF2list.erase(fite);
        }
        else
        {
            ++fite;
        }
    }
    return;
}