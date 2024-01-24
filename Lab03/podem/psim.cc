#include <iostream>
#include <iomanip>
#include "circuit.h"
#include "GetLongOpt.h"
using namespace std;

extern GetLongOpt option;

// Event-driven Parallel Pattern Logic simulation
void CIRCUIT::ParallelLogicSimVectors()
{
    cout << "Run Parallel Logic simulation" << endl;
    int total = 0;
    double average = 0.0;
    double num = 0.0;
    double percentage = 0.0;
    unsigned pattern_num(0);
    unsigned pattern_idx(0);
    while (!Pattern.eof())
    {
        for (pattern_idx = 0; pattern_idx < PatternNum; pattern_idx++)
        {
            if (!Pattern.eof())
            {
                ++pattern_num;
                Pattern.ReadNextPattern(pattern_idx);
            }
            else
                break;
        }
        ScheduleAllPIs();
        GATE *gptr;
        int k = 0;
        for (unsigned i = 0; i <= MaxLevel; i++)
        {
            while (!Queue[i].empty())
            {
                gptr = Queue[i].front();
                Queue[i].pop_front();
                gptr->ResetFlag(SCHEDULED);
                ParallelEvaluate(gptr);
                k = k + 1;
            }
        }
        total = total + k;
        // PrintParallelIOs(pattern_idx);
    }
    num = pattern_num;
    average = total / num;
    percentage = (average / No_Gate()) * 100;
    cout << "average gate evaluations = " << fixed << setprecision(2) << average << endl;
    cout << "percentage of average gate evaluations over the total number of gates = " << fixed << setprecision(2) << percentage << " % " << endl;
}

void CIRCUIT::Coutcc(string argv2)
{
    ofstream OutputStrm;
    string y = argv2;
    OutputStrm.open(y, ios::out);
    OutputStrm << "#include<iostream>" << endl;
    OutputStrm << "#include <ctime>" << endl;
    OutputStrm << "#include <bitset>" << endl;
    OutputStrm << "#include <string>" << endl;
    OutputStrm << "#include <fstream>" << endl;
    OutputStrm << " " << endl;
    OutputStrm << "using namespace std;" << endl;
    OutputStrm << "const unsigned PatternNum =  " << PatternNum << ";" << endl;
    OutputStrm << " " << endl;
    OutputStrm << "void evaluate();" << endl;
    OutputStrm << "void printIO(unsigned idx);" << endl;
    OutputStrm << " " << endl;
    for (size_t k = 0; k < No_Gate(); k++)
    {
        OutputStrm << "bitset<PatternNum> "
                   << "G_" << Gate(k)->GetName() << "[2];" << endl;
    }
    OutputStrm << "bitset<PatternNum> temp;" << endl;
    string x = y.replace(y.find("."), 3, ".out");
    OutputStrm << "ofstream fout(\"" << x << "\",ios::out);" << endl;
    OutputStrm << " " << endl;
    OutputStrm << "int main()" << endl;
    OutputStrm << "{" << endl;
    OutputStrm << "clock_t time_init, time_end;" << endl;
    OutputStrm << "time_init = clock();" << endl;

    unsigned pattern_num(0);
    unsigned pattern_idx(0);
    while (!Pattern.eof())
    {
        for (pattern_idx = 0; pattern_idx < PatternNum; pattern_idx++)
        {
            if (!Pattern.eof())
            {
                ++pattern_num;
                Pattern.ReadNextPattern(pattern_idx);
            }
            else
                break;
        }
        for (size_t i = 0; i < No_PI(); i++)
        {
            for (size_t j = 0; j < 2; j++)
            {
                int r = (int)(PIGate(i)->GetWireValue(j).to_ulong());
                OutputStrm << "G_" << PIGate(i)->GetName() << "[" << j << "] = " << r << ";" << endl;
            }
        }
        OutputStrm << " " << endl;
        OutputStrm << "evaluate();" << endl;
        OutputStrm << "printIO(" << pattern_idx << ");" << endl;
        OutputStrm << " " << endl;
    }
    OutputStrm << "time_end = clock();" << endl;
    OutputStrm << "cout << \"Total CPU Time = \" << double(time_end - time_init)/CLOCKS_PER_SEC << endl;" << endl;
    OutputStrm << "system(\" ps aux | grep a.out \");" << endl;
    OutputStrm << "return 0;" << endl;
    OutputStrm << "}" << endl;
    OutputStrm << " " << endl;
    OutputStrm << "void evaluate()" << endl;
    OutputStrm << "{" << endl;
    for (unsigned i = 0; i < No_Gate(); i++)
    {
        for (unsigned j = 0; j < Gate(i)->No_Fanout(); j++)
        {
            if (!Gate(i)->Fanout(j)->GetFlag(SCHEDULED))
            {
                Gate(i)->Fanout(j)->SetFlag(SCHEDULED);
                Queue[Gate(i)->Fanout(j)->GetLevel()].push_back(Gate(i)->Fanout(j));
            }
        }
    }
    GATE *gptr;
    for (unsigned i = 0; i <= MaxLevel; i++)
    {
        while (!Queue[i].empty())
        {
            gptr = Queue[i].front();
            Queue[i].pop_front();
            gptr->ResetFlag(SCHEDULED);
            for (size_t k = 0; k < 2; k++)
            {
                OutputStrm << "G_" << gptr->GetName() << "[" << k << "] = "
                           << "G_" << gptr->Fanin(0)->GetName() << "[" << k << "] ;" << endl;
            }
            switch (gptr->GetFunction())
            {
            case G_AND:
            case G_NAND:
                for (size_t k = 0; k < 2; k++)
                {
                    OutputStrm << "G_" << gptr->GetName() << "[" << k << "] &= "
                               << "G_" << gptr->Fanin(1)->GetName() << "[" << k << "] ;" << endl;
                }
                break;
            case G_OR:
            case G_NOR:
                for (size_t k = 0; k < 2; k++)
                {
                    OutputStrm << "G_" << gptr->GetName() << "[" << k << "] |= "
                               << "G_" << gptr->Fanin(1)->GetName() << "[" << k << "] ;" << endl;
                }
                break;
            default:
                break;
            }
            if (gptr->Fanout(0))
            {
                OutputStrm << "temp = G_" << gptr->GetName() << "[0] ;" << endl;
                if (gptr->Is_Inversion())
                {
                    OutputStrm << "G_" << gptr->GetName() << "[0] = ~"
                               << "G_" << gptr->GetName() << "[1] ;" << endl;
                    OutputStrm << "G_" << gptr->GetName() << "[1] = ~temp ;" << endl;
                }
                else
                {
                    OutputStrm << "G_" << gptr->GetName() << "[0] = "
                               << "G_" << gptr->GetName() << "[1] ;" << endl;
                    OutputStrm << "G_" << gptr->GetName() << "[1] = temp ;" << endl;
                }
            }
        }
    }
    OutputStrm << "}" << endl;
    OutputStrm << " " << endl;
    OutputStrm << "void printIO(unsigned idx)" << endl;
    OutputStrm << "{" << endl;
    OutputStrm << " " << endl;
    OutputStrm << "for (unsigned j = 0; j < idx; j++)" << endl;
    OutputStrm << "{" << endl;
    for (size_t k = 0; k < No_PI(); k++)
    {
        OutputStrm << "    if(G_" << PIGate(k)->GetName() << "[0][j] == 0)" << endl;
        OutputStrm << "    {" << endl;
        OutputStrm << "        if(G_" << PIGate(k)->GetName() << "[1][j] == 1)" << endl;
        OutputStrm << "            fout << \"F\";" << endl;
        OutputStrm << "        else" << endl;
        OutputStrm << "            fout << \"0\";" << endl;
        OutputStrm << "    }" << endl;
        OutputStrm << "    else" << endl;
        OutputStrm << "    {" << endl;
        OutputStrm << "        if(G_" << PIGate(k)->GetName() << "[1][j] == 1)" << endl;
        OutputStrm << "            fout << \"1\";" << endl;
        OutputStrm << "        else" << endl;
        OutputStrm << "            fout << \"2\";" << endl;
        OutputStrm << "    }" << endl;
    }
    OutputStrm << "fout << \" \" ;" << endl;
    for (size_t k = 0; k < No_PO(); k++)
    {
        OutputStrm << "    if(G_" << POGate(k)->GetName() << "[0][j] == 0)" << endl;
        OutputStrm << "    {" << endl;
        OutputStrm << "        if(G_" << POGate(k)->GetName() << "[1][j] == 1)" << endl;
        OutputStrm << "            fout << \"F\";" << endl;
        OutputStrm << "        else" << endl;
        OutputStrm << "            fout << \"0\";" << endl;
        OutputStrm << "    }" << endl;
        OutputStrm << "    else" << endl;
        OutputStrm << "    {" << endl;
        OutputStrm << "        if(G_" << POGate(k)->GetName() << "[1][j] == 1)" << endl;
        OutputStrm << "            fout << \"1\";" << endl;
        OutputStrm << "        else" << endl;
        OutputStrm << "            fout << \"2\";" << endl;
        OutputStrm << "    }" << endl;
    }
    OutputStrm << "fout << endl;" << endl;
    OutputStrm << "}" << endl;
    OutputStrm << "}" << endl;
}

// Assign next input pattern to PI's idx'th bits
void PATTERN::ReadNextPattern(unsigned idx)
{
    char V;
    for (int i = 0; i < no_pi_infile; i++)
    {
        patterninput >> V;
        if (V == '0')
        {
            inlist[i]->ResetWireValue(0, idx);
            inlist[i]->ResetWireValue(1, idx);
        }
        else if (V == '1')
        {
            inlist[i]->SetWireValue(0, idx);
            inlist[i]->SetWireValue(1, idx);
        }
        else if (V == 'X')
        {
            inlist[i]->SetWireValue(0, idx);
            inlist[i]->ResetWireValue(1, idx);
        }
    }
    // Take care of newline to force eof() function correctly
    patterninput >> V;
    if (!patterninput.eof())
        patterninput.unget();
    return;
}

// Simulate PatternNum vectors
void CIRCUIT::ParallelLogicSim()
{
    GATE *gptr;
    for (unsigned i = 0; i <= MaxLevel; i++)
    {
        while (!Queue[i].empty())
        {
            gptr = Queue[i].front();
            Queue[i].pop_front();
            gptr->ResetFlag(SCHEDULED);
            ParallelEvaluate(gptr);
        }
    }
    return;
}

// Evaluate parallel value of gptr
void CIRCUIT::ParallelEvaluate(GATEPTR gptr)
{
    register unsigned i;
    bitset<PatternNum> new_value1(gptr->Fanin(0)->GetValue1());
    bitset<PatternNum> new_value2(gptr->Fanin(0)->GetValue2());
    switch (gptr->GetFunction())
    {
    case G_AND:
    case G_NAND:
        for (i = 1; i < gptr->No_Fanin(); ++i)
        {
            new_value1 &= gptr->Fanin(i)->GetValue1();
            new_value2 &= gptr->Fanin(i)->GetValue2();
        }
        break;
    case G_OR:
    case G_NOR:
        for (i = 1; i < gptr->No_Fanin(); ++i)
        {
            new_value1 |= gptr->Fanin(i)->GetValue1();
            new_value2 |= gptr->Fanin(i)->GetValue2();
        }
        break;
    default:
        break;
    }
    // swap new_value1 and new_value2 to avoid unknown value masked
    if (gptr->Is_Inversion())
    {
        new_value1.flip();
        new_value2.flip();
        bitset<PatternNum> value(new_value1);
        new_value1 = new_value2;
        new_value2 = value;
    }
    if (gptr->GetValue1() != new_value1 || gptr->GetValue2() != new_value2)
    {
        gptr->SetValue1(new_value1);
        gptr->SetValue2(new_value2);
        ScheduleFanout(gptr);
    }
    return;
}

void CIRCUIT::PrintParallelIOs(unsigned idx)
{
    register unsigned i;
    for (unsigned j = 0; j < idx; j++)
    {
        for (i = 0; i < No_PI(); ++i)
        {
            if (PIGate(i)->GetWireValue(0, j) == 0)
            {
                if (PIGate(i)->GetWireValue(1, j) == 1)
                {
                    cout << "F";
                }
                else
                    cout << "0";
            }
            else
            {
                if (PIGate(i)->GetWireValue(1, j) == 1)
                {
                    cout << "1";
                }
                else
                    cout << "X";
            }
        }
        cout << " ";
        for (i = 0; i < No_PO(); ++i)
        {
            if (POGate(i)->GetWireValue(0, j) == 0)
            {
                if (POGate(i)->GetWireValue(1, j) == 1)
                {
                    cout << "F";
                }
                else
                    cout << "0";
            }
            else
            {
                if (POGate(i)->GetWireValue(1, j) == 1)
                {
                    cout << "1";
                }
                else
                    cout << "X";
            }
        }
        cout << endl;
    }
    return;
}

void CIRCUIT::ScheduleAllPIs()
{
    for (unsigned i = 0; i < No_PI(); i++)
    {
        ScheduleFanout(PIGate(i));
    }
    return;
}
