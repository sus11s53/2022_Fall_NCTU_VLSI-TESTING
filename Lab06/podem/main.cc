#include <iostream>
#include <ctime>
#include "circuit.h"
#include "GetLongOpt.h"
#include "ReadPattern.h"
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
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
    option.enroll("cp", GetLongOpt::NoValue,
                  "check-point theorem", 0);
    option.enroll("gl", GetLongOpt::NoValue,
                  "certain fault list", 0);
    option.enroll("bridging", GetLongOpt::NoValue,
                  "generates bridging faults", 0);
    option.enroll("pattern", GetLongOpt::NoValue,
                  "produce patterns", 0);
    option.enroll("random_pattern", GetLongOpt::NoValue,
                  "random pattern generation before ATPG", 0);
    option.enroll("num", GetLongOpt::MandatoryValue,
                  "patterns numbers", 0);
    option.enroll("unknown", GetLongOpt::NoValue,
                  "patterns have x", 0);
    option.enroll("bridging_fsim", GetLongOpt::NoValue,
                  "run bridging fault simulation", 0);
    option.enroll("bridging_atpg", GetLongOpt::NoValue,
                  "run bridging atpg", 0);

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
    else if (option.retrieve("bridging_fsim"))
    {
        Circuit.GenerateBridgingFaults();
        Circuit.SortFaninByLevel();
        Circuit.MarkOutputGate();
        Circuit.InitPattern(option.retrieve("input"));
        Circuit.BridgingFaultSimVectors();
    }

    else if (option.retrieve("pattern"))
    {
        int pattern_num(atoi(argv[3]));
        cout << pattern_num << endl; // debug
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
            for (size_t i = 0; i < Circuit.No_PI(); ++i)
            {
                OutputStrm << "PI " << Circuit.PIGate(i)->GetName() << " ";
            }
            OutputStrm << endl;
            if (option.retrieve("unknown"))
            {
                for (size_t i = 0; i < pattern_num; ++i)
                {
                    for (unsigned i = 0; i < Circuit.No_PI(); ++i)
                    {
                        int k = int(3.0 * rand() / (RAND_MAX + 1.0));
                        stringstream x;
                        string y;
                        if (k == 2)
                        {
                            x << k;
                            x >> y;
                            y = y.replace(y.find("2"), 1, "X");
                            OutputStrm << y << " ";
                        }
                        else
                        {
                            OutputStrm << k << " ";
                        }
                    }
                    OutputStrm << endl;
                }
            }
            else
            {
                for (size_t i = 0; i < pattern_num; ++i)
                {
                    for (unsigned i = 0; i < Circuit.No_PI(); ++i)
                    {
                        int k = int(2.0 * rand() / (RAND_MAX + 1.0));
                        OutputStrm << k << " ";
                    }
                    OutputStrm << endl;
                }
            }
        }
    }

    else if (option.retrieve("random_pattern"))
    {
        Circuit.GenerateAllFaultList();
        Circuit.SortFaninByLevel();
        Circuit.MarkOutputGate();
        Circuit.RandomPattern();
        Circuit.Atpg();
    }

    else if (option.retrieve("bridging_atpg"))
    {
        Circuit.GenerateBridgingFaults();
        Circuit.SortFaninByLevel();
        Circuit.MarkOutputGate();
        Circuit.BridgingAtpg();
    }

    else
    {
        if (option.retrieve("cp"))
        {
            Circuit.GenerateAllCpFaultList();
        }
        else if (option.retrieve("gl"))
        {
            Circuit.GenerateGoalFaultList();
        }
        else
        {
            Circuit.GenerateAllFaultList();
        }
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
