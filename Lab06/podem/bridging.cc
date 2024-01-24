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
    UBFlist = BFlist;
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

void CIRCUIT::BridgingAtpg()
{

    unsigned i, total_backtrack_num(0), pattern_num(0);
    ATPG_STATUS status;
    FAULT *fptr;
    vector<BRIDGING_FAULT *>::iterator fite;

    // Prepare the output files
    ofstream OutputStrm;
    if (option.retrieve("output"))
    {
        OutputStrm.open((char *)option.retrieve("output"), ios::out);
        if (!OutputStrm)
        {
            cout << "Unable to open output file: "
                 << option.retrieve("output") << endl;
            cout << "Unsaved output!\n";
            exit(-1);
        }
    }

    if (option.retrieve("output"))
    {
        for (i = 0; i < PIlist.size(); ++i)
        {
            OutputStrm << "PI " << PIlist[i]->GetName() << " ";
        }
        OutputStrm << endl;
    }

    for (fite = BFlist.begin(); fite != BFlist.end(); ++fite)
    {
        fptr = *fite;

        if (fptr->GetStatus() == DETECTED)
        {
            cout << "(" << ((BRIDGING_FAULT *)fptr)->GetGate1()->GetName() << "," << ((BRIDGING_FAULT *)fptr)->GetGate2()->GetName() << "," << ((((BRIDGING_FAULT *)fptr)->GetType()) ? "OR" : "AND") << ")"
                 << "has been detected\n\n";
            continue;
        }

        VALUE faultvalue;
        if ((*fite)->GetType() == 0)
        {
            faultvalue == S0;
        }

        if ((*fite)->GetType() == 1)
        {
            faultvalue == S1;
        }

        fptr = new FAULT((*fite)->GetGate1(), (*fite)->GetGate1(), faultvalue);
        // run podem algorithm
        status = BridgingPodem(fptr, total_backtrack_num, (*fite)->GetGate2(), faultvalue);

        if (status != TRUE)
        {
            fptr = new FAULT((*fite)->GetGate2(), (*fite)->GetGate2(), faultvalue);
            status = BridgingPodem(fptr, total_backtrack_num, (*fite)->GetGate1(), faultvalue);
        }

        switch (status)
        {
        case TRUE:
            fptr->SetStatus(DETECTED);
            ++pattern_num;
            // run fault simulation for fault dropping
            for (i = 0; i < PIlist.size(); ++i)
            {
                ScheduleFanout(PIlist[i]);
                if (option.retrieve("output"))
                {
                    OutputStrm << PIlist[i]->GetValue();
                }
            }
            if (option.retrieve("output"))
            {
                OutputStrm << endl;
            }
            for (i = PIlist.size(); i < Netlist.size(); ++i)
            {
                Netlist[i]->SetValue(X);
            }
            LogicSim();
            BFaultSim();
            break;
        case CONFLICT:
            fptr->SetStatus(REDUNDANT);
            break;
        case FALSE:
            fptr->SetStatus(ABORT);
            break;
        }
    } // end all faults

    // compute fault coverage
    unsigned total_num(0);
    unsigned abort_num(0), redundant_num(0), detected_num(0);
    unsigned eqv_abort_num(0), eqv_redundant_num(0), eqv_detected_num(0);
    for (fite = BFlist.begin(); fite != BFlist.end(); ++fite)
    {
        fptr = *fite;
        switch (fptr->GetStatus())
        {
        case DETECTED:
            ++eqv_detected_num;
            detected_num += fptr->GetEqvFaultNum();
            break;
        case REDUNDANT:
            ++eqv_redundant_num;
            redundant_num += fptr->GetEqvFaultNum();
            break;
        case ABORT:
            ++eqv_abort_num;
            abort_num += fptr->GetEqvFaultNum();
            break;
        default:
            cerr << "Unknown fault type exists" << endl;
            cout << "(" << ((BRIDGING_FAULT *)fptr)->GetGate1()->GetName() << "," << ((BRIDGING_FAULT *)fptr)->GetGate2()->GetName() << "," << ((((BRIDGING_FAULT *)fptr)->GetType()) ? "OR" : "AND") << ")"
                 << endl;
            break;
        }
    }
    total_num = detected_num + abort_num + redundant_num;

    cout.setf(ios::fixed);
    cout.precision(2);
    cout << "---------------------------------------" << endl;
    cout << "Test pattern number = " << pattern_num << endl;
    cout << "Total backtrack number = " << total_backtrack_num << endl;
    cout << "---------------------------------------" << endl;
    cout << "Total fault number = " << total_num << endl;
    cout << "Detected fault number = " << detected_num << endl;
    cout << "Undetected fault number = " << abort_num + redundant_num << endl;
    cout << "Abort fault number = " << abort_num << endl;
    cout << "Redundant fault number = " << redundant_num << endl;
    cout << "---------------------------------------" << endl;
    cout << "Total equivalent fault number = " << BFlist.size() << endl;
    cout << "Equivalent detected fault number = " << eqv_detected_num << endl;
    cout << "Equivalent undetected fault number = " << eqv_abort_num + eqv_redundant_num << endl;
    cout << "Equivalent abort fault number = " << eqv_abort_num << endl;
    cout << "Equivalent redundant fault number = " << eqv_redundant_num << endl;
    cout << "---------------------------------------" << endl;
    cout << "Fault Coverge = " << 100 * detected_num / double(total_num) << "%" << endl;
    // cout << "Equivalent FC = " << 100 * eqv_detected_num / double(BFlist.size()) << "%" << endl;
    // cout << "Fault Efficiency = " << 100 * detected_num / double(total_num - redundant_num) << "%" << endl;
    cout << "---------------------------------------" << endl;
    return;
}

ATPG_STATUS CIRCUIT::BridgingPodem(FAULT *fptr, unsigned &total_backtrack_num, GATEPTR gp, VALUE v)
{
    unsigned i, backtrack_num(0);
    GATEPTR pi_gptr(0), decision_gptr(0);
    ATPG_STATUS status;

    // set all values as unknown
    for (i = 0; i < Netlist.size(); ++i)
    {
        Netlist[i]->SetValue(X);
    }
    // mark propagate paths
    MarkPropagateTree(fptr->GetOutputGate());
    // propagate fault free value
    status = SetUniqueImpliedValue(fptr);
    switch (status)
    {
    case TRUE:
        LogicSim();
        // inject faulty value
        if (FaultEvaluate(fptr))
        {
            // forward implication
            ScheduleFanout(fptr->GetOutputGate());
            LogicSim();
        }
        // check if the fault has propagated to PO
        if (!CheckTest())
        {
            status = FALSE;
        }
        break;
    case CONFLICT:
        status = CONFLICT;
        break;
    case FALSE:
        break;
    }

    while (backtrack_num < BackTrackLimit && status == FALSE)
    {
        // search possible PI decision
        pi_gptr = TestPossible(fptr);
        if (pi_gptr)
        { // decision found
            ScheduleFanout(pi_gptr);
            // push to decision tree
            GateStack.push_back(pi_gptr);
            decision_gptr = pi_gptr;
        }
        else
        { // backtrack previous decision
            while (!GateStack.empty() && !pi_gptr)
            {
                // all decision tried (1 and 0)
                if (decision_gptr->GetFlag(ALL_ASSIGNED))
                {
                    decision_gptr->ResetFlag(ALL_ASSIGNED);
                    decision_gptr->SetValue(X);
                    ScheduleFanout(decision_gptr);
                    // remove decision from decision tree
                    GateStack.pop_back();
                    decision_gptr = GateStack.back();
                }
                // inverse current decision value
                else
                {
                    decision_gptr->InverseValue();
                    ScheduleFanout(decision_gptr);
                    decision_gptr->SetFlag(ALL_ASSIGNED);
                    ++backtrack_num;
                    pi_gptr = decision_gptr;
                }
            }
            // no other decision
            if (!pi_gptr)
            {
                status = CONFLICT;
            }
        }
        if (pi_gptr)
        {
            // cout << "Implication" << endl;
            LogicSim();
            // fault injection
            if (FaultEvaluate(fptr))
            {
                // cout << "Implication" << endl;
                // forward implication
                ScheduleFanout(fptr->GetOutputGate());
                LogicSim();
            }
            if (CheckTest())
            {
                status = TRUE;
            }
        }
    } // end while loop

    // clean ALL_ASSIGNED and MARKED flags
    list<GATEPTR>::iterator gite;
    for (gite = GateStack.begin(); gite != GateStack.end(); ++gite)
    {
        (*gite)->ResetFlag(ALL_ASSIGNED);
        // cout << (*gite)->GetName() << "," << (*gite)->GetValue() << endl;
    }
    for (gite = PropagateTree.begin(); gite != PropagateTree.end(); ++gite)
    {
        (*gite)->ResetFlag(MARKED);
        // cout << (*gite)->GetName() << "," << (*gite)->GetValue() << endl;
    }

    // clear stacks
    GateStack.clear();
    PropagateTree.clear();

    // assign true values to PIs
    if (status == TRUE)
    {
        if (gp->GetValue() == X || gp->GetValue() == v)
        {
            gp->SetValue(v);
        }
        else
        {
            return CONFLICT;
        }

        for (i = 0; i < PIlist.size(); ++i)
        {
            switch (PIlist[i]->GetValue())
            {
            case S1:
                break;
            case S0:
                break;
            case D:
                PIlist[i]->SetValue(S1);
                // cout << "PI assignment" << endl;
                // cout << "set " << PIlist[i]->GetName() << " = " << PIlist[i]->GetValue() << endl;
                break;
            case B:
                PIlist[i]->SetValue(S0);
                // cout << "PI assignment" << endl;
                // cout << "set " << PIlist[i]->GetName() << " = " << PIlist[i]->GetValue() << endl;
                break;
            case X:
                PIlist[i]->SetValue(VALUE(2.0 * rand() / (RAND_MAX + 1.0)));
                // cout << "PI assignment" << endl;
                // cout << "set " << PIlist[i]->GetName() << " = " << PIlist[i]->GetValue() << endl;
                break;
            default:
                cerr << "Illigal value" << endl;
                break;
            }
        } // end for all PI
    }     // end status == TRUE

    total_backtrack_num += backtrack_num;
    return status;
}

void CIRCUIT::BFaultSim()
{
    register unsigned i, fault_idx(0);
    GATEPTR gptr;
    BRIDGING_FAULT *bfptr, *bp;
    BRIDGING_FAULT *bsimulate_flist[PatternNum];
    list<GATEPTR>::iterator gite;
    // initial all gates
    for (i = 0; i < Netlist.size(); ++i)
    {
        Netlist[i]->SetFaultFreeValue();
    }

    // for all undetected faults
    vector<BRIDGING_FAULT *>::iterator bfite;
    for (bfite = UBFlist.begin(); bfite != UBFlist.end(); ++bfite)
    {
        bfptr = *bfite;
        // skip redundant and detected faults
        if (bfptr->GetStatus() == REDUNDANT || bfptr->GetStatus() == DETECTED)
        {
            continue;
        }
        // the fault is not active
        if (bfptr->GetGate1()->GetValue() == bfptr->GetGate2()->GetValue())
        {
            continue;
        }

        if (bfptr->GetType() == AND)
        {
            if (bfptr->GetGate1()->GetValue() == S1)
            {
                gptr = bfptr->GetGate1();
            }
            if (bfptr->GetGate2()->GetValue() == S1)
            {
                gptr = bfptr->GetGate2();
            }
        }

        if (bfptr->GetType() == OR)
        {
            if (bfptr->GetGate1()->GetValue() == S0)
            {
                gptr = bfptr->GetGate1();
            }
            if (bfptr->GetGate2()->GetValue() == S0)
            {
                gptr = bfptr->GetGate2();
            }
        }

        if (gptr->GetFlag(OUTPUT))
        {
            bfptr->SetStatus(DETECTED);
            continue;
        }
        if (!gptr->GetFlag(FAULTY))
        {
            gptr->SetFlag(FAULTY);
            GateStack.push_front(gptr);
        }

        InjectFaultValue(gptr, fault_idx, bfptr->GetValue());
        gptr->SetFlag(FAULT_INJECTED);
        ScheduleFanout(gptr);
        bsimulate_flist[fault_idx++] = bfptr;

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
                        bp = bsimulate_flist[i];
                        if (bp->GetStatus() == DETECTED)
                        {
                            continue;
                        }
                        // faulty value != fault-free value && fault-free != X &&
                        // faulty value != X (WireValue1[i] == WireValue2[i])
                        if (gptr->GetValue() != VALUE(gptr->GetValue1(i)) && gptr->GetValue() != X && gptr->GetValue1(i) == gptr->GetValue2(i))
                        {
                            bp->SetStatus(DETECTED);
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
                    bp = bsimulate_flist[i];
                    if (bp->GetStatus() == DETECTED)
                    {
                        continue;
                    }
                    // faulty value != fault-free value && fault-free != X &&
                    // faulty value != X (WireValue1[i] == WireValue2[i])
                    if (gptr->GetValue() != VALUE(gptr->GetValue1(i)) && gptr->GetValue() != X && gptr->GetValue1(i) == gptr->GetValue2(i))
                    {
                        bp->SetStatus(DETECTED);
                    }
                }
            }
            gptr->SetFaultFreeValue();
        } // end for GateStack
        GateStack.clear();
        fault_idx = 0;
    } // end fault simulation
    return;
}