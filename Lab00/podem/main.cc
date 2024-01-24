#include <iostream>
#include <ctime>
#include <iomanip>
#include "circuit.h"
#include "GetLongOpt.h"
#include "ReadPattern.h"
using namespace std;

// All defined in readcircuit.l
extern char *yytext;
extern FILE *yyin;
extern CIRCUIT Circuit;
extern int yyparse(void);
extern bool ParseError;

extern void Interactive();

GetLongOpt option;

int SetupOption(int argc, char **argv)
{
    option.usage("[options] input_circuit_file");
    option.enroll("help", GetLongOpt::NoValue,
                  "print this help summary", 0);
    option.enroll("logicsim", GetLongOpt::NoValue,
                  "run logic simulation", 0);
    option.enroll("plogicsim", GetLongOpt::NoValue,
                  "run parallel logic simulation", 0);
    option.enroll("fsim", GetLongOpt::NoValue,
                  "run stuck-at fault simulation", 0);
    option.enroll("stfsim", GetLongOpt::NoValue,
                  "run single pattern single transition-fault simulation", 0);
    option.enroll("transition", GetLongOpt::NoValue,
                  "run transition-fault ATPG", 0);
    option.enroll("input", GetLongOpt::MandatoryValue,
                  "set the input pattern file", 0);
    option.enroll("output", GetLongOpt::MandatoryValue,
                  "set the output pattern file", 0);
    option.enroll("bt", GetLongOpt::OptionalValue,
                  "set the backtrack limit", 0);
    option.enroll("ass0", GetLongOpt::NoValue,
                  "GOOD", 0);
    int optind = option.parse(argc, argv);
    if (optind < 1)
    {
        exit(0);
    }
    if (option.retrieve("help"))
    {
        option.usage();
        exit(0);
    }
    return optind;
}

int main(int argc, char **argv)
{
    int optind = SetupOption(argc, argv);
    clock_t time_init, time_end;
    time_init = clock();
    // Setup File
    if (optind < argc)
    {
        if ((yyin = fopen(argv[optind], "r")) == NULL)
        {
            cout << "Can't open circuit file: " << argv[optind] << endl;
            exit(-1);
        }
        else
        {
            string circuit_name = argv[optind];
            string::size_type idx = circuit_name.rfind('/');
            if (idx != string::npos)
            {
                circuit_name = circuit_name.substr(idx + 1);
            }
            idx = circuit_name.find(".bench");
            if (idx != string::npos)
            {
                circuit_name = circuit_name.substr(0, idx);
            }
            Circuit.SetName(circuit_name);
        }
    }
    else
    {
        cout << "Input circuit file missing" << endl;
        option.usage();
        return -1;
    }
    cout << "Start parsing input file\n";
    yyparse();
    if (ParseError)
    {
        cerr << "Please correct error and try Again.\n";
        return -1;
    }
    fclose(yyin);
    Circuit.FanoutList();
    Circuit.SetupIO_ID();
    Circuit.Levelize();
    Circuit.Check_Levelization();
    Circuit.InitializeQueue();

    if (option.retrieve("logicsim"))
    {
        // logic simulator
        Circuit.InitPattern(option.retrieve("input"));
        Circuit.LogicSimVectors();
    }
    /////////////////////////////////////////////////////////////////////////////////////
    else if (option.retrieve("ass0"))
    {
        int a = Circuit.No_PI();
        int b = Circuit.No_PO();
        double Gate_total = 0;
        int d = Circuit.No_PPI();
        int cnt_stem = 0;
        int cnt_branch = 0;
        int cnt_oneout = 0;
        int Not = 0;
        int Or = 0;
        int Nor = 0;
        int Nand = 0;
        int And = 0;
        double Fanout_total = 0;
        for (int i = 0; i < Circuit.No_Gate(); i++)
        {
            switch (Circuit.Gate(i)->GetFunction())
            {
            case G_NOT:
                Not++;
                break;
            case G_OR:
                Or++;
                break;
            case G_NOR:
                Nor++;
                break;
            case G_NAND:
                Nand++;
                break;
            case G_AND:
                And++;
                break;
            default:
                break;
            }
        }

        for (int i = 0; i < Circuit.No_Gate(); i++)
        {
            if (Circuit.Gate(i)->GetFunction() >= 4 && Circuit.Gate(i)->No_Fanout() >= 2)
            {
                cnt_stem++;
            }
        }

        for (int i = 0; i < Circuit.No_Gate(); i++)
        {
            if (Circuit.Gate(i)->GetFunction() >= 4 && Circuit.Gate(i)->No_Fanout() >= 2)
            {
                cnt_branch += Circuit.Gate(i)->No_Fanout();
            }
        }

        for (int i = 0; i < Circuit.No_Gate(); i++)
        {
            if (Circuit.Gate(i)->GetFunction() >= 4 && Circuit.Gate(i)->No_Fanout() == 1)
            {
                cnt_oneout++;
            }
        }

        for (int i = 0; i < Circuit.No_Gate(); i++)
        {
            if (Circuit.Gate(i)->GetFunction() >= 4)
            {
                Fanout_total += Circuit.Gate(i)->No_Fanout();
                Gate_total++;
            }
        }

        cout << "Number of inputs = " << a << endl;
        cout << "Number of outputs = " << b << endl;
        cout << "Total number of gates including inverter,or,nor,and,nand = " << Gate_total << endl;
        cout << "inverter = " << Not << endl;
        cout << "Or = " << Or << endl;
        cout << "Nor = " << Nor << endl;
        cout << "Nand = " << Nand << endl;
        cout << "And = " << And << endl;
        cout << "DFF = " << d << endl;
        cout << "Total number of signal nets = " << cnt_stem + cnt_branch + cnt_oneout << endl;
        cout << "Number of branch nets = " << cnt_branch << endl;
        cout << "Number of stem nets = " << cnt_stem << endl;
        cout << "Average number of fanouts of each gate = " << setprecision(6) << fixed << Fanout_total / Gate_total << endl;
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    else if (option.retrieve("plogicsim"))
    {
        // parallel logic simulator
        Circuit.InitPattern(option.retrieve("input"));
        Circuit.ParallelLogicSimVectors();
    }
    else if (option.retrieve("stfsim"))
    {
        // single pattern single transition-fault simulation
        Circuit.MarkOutputGate();
        Circuit.GenerateAllTFaultList();
        Circuit.InitPattern(option.retrieve("input"));
        Circuit.TFaultSimVectors();
    }
    else if (option.retrieve("transition"))
    {
        Circuit.MarkOutputGate();
        Circuit.GenerateAllTFaultList();
        Circuit.SortFaninByLevel();
        if (option.retrieve("bt"))
        {
            Circuit.SetBackTrackLimit(atoi(option.retrieve("bt")));
        }
        Circuit.TFAtpg();
    }
    else
    {
        Circuit.GenerateAllFaultList();
        Circuit.SortFaninByLevel();
        Circuit.MarkOutputGate();
        if (option.retrieve("fsim"))
        {
            // stuck-at fault simulator
            Circuit.InitPattern(option.retrieve("input"));
            Circuit.FaultSimVectors();
        }

        else
        {
            if (option.retrieve("bt"))
            {
                Circuit.SetBackTrackLimit(atoi(option.retrieve("bt")));
            }
            // stuck-at fualt ATPG
            Circuit.Atpg();
        }
    }
    time_end = clock();
    cout << "total CPU time = " << double(time_end - time_init) / CLOCKS_PER_SEC << endl;
    cout << endl;
    return 0;
}
