#include <iostream>
#include <ctime>
#include "circuit.h"
#include "GetLongOpt.h"
#include "ReadPattern.h"
#include <unordered_map>
#include <string>
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
    option.enroll("path", GetLongOpt::NoValue,
                  "path!", 0);
    option.enroll("start", GetLongOpt::MandatoryValue,
                  "start point", 0);
    option.enroll("end", GetLongOpt::MandatoryValue,
                  "end point", 0);
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

// add the gate after the curNode!
void HANGUP(GATE *a, GATE *b, unordered_map<GATE *, vector<vector<GATE *>>> &store)
{
    store[a].insert(store[a].end(), store[b].begin(), store[b].end());
    return;
}
// fill curNode to the path!
void FILL(GATE *a, unordered_map<GATE *, vector<vector<GATE *>>> &store, size_t vertex)
{
    store[a][vertex].emplace_back(a);
}

// Depth-First Search!
void DFS(GATE *startNode, const GATE *endNode, unordered_map<GATE *, vector<vector<GATE *>>> &paths, unordered_map<GATE *, bool> &visited)
{
    visited[startNode] = true; // check if visited?

    // verify the lastNode!
    if (startNode == endNode)
    {
        paths[startNode] = {{startNode}}; // put the endNode!
        return;
    }

    // move to nextNode!
    for (size_t i = 0; i < startNode->No_Fanout(); i++)
    {
        GATE *nextNode = startNode->Fanout(i);

        if (visited[nextNode] == false)
        {
            DFS(nextNode, endNode, paths, visited);
        }

        HANGUP(startNode, nextNode, paths);
    }

    for (size_t i = 0; i < paths[startNode].size(); ++i)
    {
        FILL(startNode, paths, i);
    }
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
    else if (option.retrieve("levelize"))
    {
        Circuit.Check_Levelization();
    }

    else if (option.retrieve("path"))
    {
        string Startpoint(argv[3]);
        string Endpoint(argv[5]);

        GATE *startpoint;
        GATE *endpoint;

        // find startpoint!
        for (size_t i = 0; i < Circuit.No_PI(); i++)
        {
            if (Startpoint == Circuit.PIGate(i)->GetName())
            {
                startpoint = Circuit.PIGate(i);
                break;
            }
        }

        // find endpoint!
        for (size_t i = 0; i < Circuit.No_PO(); i++)
        {
            if (Endpoint == Circuit.POGate(i)->GetName())
            {
                endpoint = Circuit.POGate(i);
                break;
            }
        }

        unordered_map<GATE *, vector<vector<GATE *>>> paths{};
        unordered_map<GATE *, bool> visited{};
        DFS(startpoint, endpoint, paths, visited);

        for (vector<GATE *> &path : paths[startpoint])
        {
            for (auto it = path.rbegin(); it != path.rend(); ++it)
                cout << (*it)->GetName() << ' ';
            cout << endl;
        }
        cout << "The paths from " << Startpoint << " to " << Endpoint << ": " << paths[startpoint].size() << endl;
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
