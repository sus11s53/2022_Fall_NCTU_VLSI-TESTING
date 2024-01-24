#include <iostream>
#include <vector>
#include "circuit.h"
#include "gate.h"
#include "bridgingFaults.h"
using namespace std;

void CIRCUIT::GenerateBridgingFaults(string a)
{
    ofstream OutputStrm;
    string y = a;
    OutputStrm.open(y, ios::out);
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
                cout << gptr->GetName() << " ";
            }
        }
        cout << "level = " << level << endl;
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

    for (i = 0; i < BFlist.size(); i++)
    {
        OutputStrm << BFlist[i]->output() << endl;
    }
}
