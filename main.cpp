/*
 * main.cpp
 *
 * This file is part of the aBeeDa Swarm Evolution project.
 *
 * Copyright 2012 Randal S. Olson, Arend Hintze.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <sstream>
#include <string.h>
#include <vector>
#include <map>
#include <math.h>
#include <time.h>
#include <iostream>
#include <dirent.h>
#include <fstream>

#include "globalConst.h"
#include "tHMM.h"
#include "tAgent.h"
#include "tGame.h"

#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */
#include <unistd.h>           /*  misc. UNIX functions      */

#include "helper.h"           /*  our own helper functions  */

#define ECHO_PORT          (2002)
#define MAX_LINE           (100000)

#define BUFLEN              512
#define NPACK               10
#define PORT                9930

int       list_s;                /*  listening socket          */
int       conn_s;                /*  connection socket         */
short int port;                  /*  port number               */
struct    sockaddr_in servaddr;  /*  socket address structure  */
char      buffer[MAX_LINE];      /*  character buffer          */
char     *endptr;                /*  for strtol()              */

void    setupBroadcast(void);
void    doBroadcast(string data);
string  findBestRun(vector<tAgent*> swarmAgents, tAgent *predatorAgent);

using namespace std;

//double  replacementRate             = 0.1;
double  perSiteMutationRate         = 0.005;
int     populationSize              = swarmSize; //It is 1000 by default
int     totalGenerations            = 200;
tGame   *game                       = NULL;

bool    make_interval_video         = false;
int     make_video_frequency        = 25;
bool    make_LOD_video              = false;
bool    track_best_brains           = false;
int     track_best_brains_frequency = 25;
bool    display_only                = false;
bool    display_directory           = false;
bool    make_logic_table            = false;
bool    make_dot_pred               = false;
bool    make_dot_swarm              = false;
double  startingDist                = 30.0 * 30.0;
double  predatorVisionAngle         = 180.0 / 2.0;
double  predatorVisionRange         = 200.0 * 200.0;
int     killDelay                   = 10;
int     numPredators                = 50;

int main(int argc, char *argv[])
{
	vector<tAgent*> swarmAgents, SANextGen, predatorAgents, PANextGen;
	tAgent *swarmAgent = NULL, *predatorAgent = NULL, *bestSwarmAgent = NULL, *bestPredatorAgent = NULL;
	double swarmMaxFitness = 0.0, predatorMaxFitness = 0.0;
    string LODFileName = "", swarmGenomeFileName = "", predatorGenomeFileName = "", inputGenomeFileName = "";
    string swarmDotFileName = "", predatorDotFileName = "", logicTableFileName = "";
    int displayDirectoryArgvIndex = 0;
    
    // initial object setup
	game = new tGame;
	swarmAgent = new tAgent;
    predatorAgent = new tAgent;
    
    // time-based seed by default. can change with command-line parameter.
    srand((unsigned int)time(NULL));
    
    for (int i = 1; i < argc; ++i)
    {
        // -d [in file name] [in file name]: display
        if (strcmp(argv[i], "-d") == 0 && (i + 2) < argc)
        {
            ++i;
            swarmAgent->loadAgent(argv[i]);
            
            ++i;
            predatorAgent->loadAgent(argv[i]);
            
            display_only = true;
        }
        
        // -dd [directory]: display all genome files in a given directory
        if (strcmp(argv[i], "-dd") == 0 && (i + 1) < argc)
        {
            ++i;
            displayDirectoryArgvIndex = i;
            
            display_directory = true;
        }
        
        // -e [out file name] [out file name] [out file name]: evolve
        else if (strcmp(argv[i], "-e") == 0 && (i + 3) < argc)
        {
            ++i;
            stringstream lodfn;
            lodfn << argv[i];
            LODFileName = lodfn.str();
            
            ++i;
            stringstream sgfn;
            sgfn << argv[i];
            swarmGenomeFileName = sgfn.str();
            
            ++i;
            stringstream pgfn;
            pgfn << argv[i];
            predatorGenomeFileName = pgfn.str();
        }
        
        // -s [int]: set seed
        else if (strcmp(argv[i], "-s") == 0 && (i + 1) < argc)
        {
            ++i;
            srand(atoi(argv[i]));
        }
        
        // -g [int]: set generations
        else if (strcmp(argv[i], "-g") == 0 && (i + 1) < argc)
        {
            ++i;
            
            // add 2 generations because we look at ancestor->ancestor as best agent at end of sim
            totalGenerations = atoi(argv[i]) + 2;
            
            if (totalGenerations < 3)
            {
                cerr << "minimum number of generations permitted is 3." << endl;
                exit(0);
            }
        }
        
        // -t [int]: track best brains
        else if (strcmp(argv[i], "-t") == 0 && (i + 1) < argc)
        {
            track_best_brains = true;
            ++i;
            track_best_brains_frequency = atoi(argv[i]);
            
            if (track_best_brains_frequency < 1)
            {
                cerr << "minimum brain tracking frequency is 1." << endl;
                exit(0);
            }
        }
        
        // -v [int]: make video of best brains at an interval
        else if (strcmp(argv[i], "-v") == 0 && (i + 1) < argc)
        {
            make_interval_video = true;
            ++i;
            make_video_frequency = atoi(argv[i]);
            
            if (make_video_frequency < 1)
            {
                cerr << "minimum video creation frequency is 1." << endl;
                exit(0);
            }
        }
        
        // -lv: make video of LOD of best agent brain at the end
        else if (strcmp(argv[i], "-lv") == 0)
        {
            make_LOD_video = true;
        }
        
        // -lt [in file name] [out file name]: create logic table for given genome
        else if (strcmp(argv[i], "-lt") == 0 && (i + 2) < argc)
        {
            ++i;
            swarmAgent->loadAgent(argv[i]);
            swarmAgent->setupPhenotype();
            ++i;
            stringstream ltfn;
            ltfn << argv[i];
            logicTableFileName = ltfn.str();
            make_logic_table = true;
        }
        
        // -dfs [in file name] [out file name]: create dot image file for given swarm genome
        else if (strcmp(argv[i], "-dfs") == 0 && (i + 2) < argc)
        {
            ++i;
            swarmAgent->loadAgent(argv[i]);
            swarmAgent->setupPhenotype();
            ++i;
            stringstream dfn;
            dfn << argv[i];
            swarmDotFileName = dfn.str();
            make_dot_swarm = true;
        }
        
        // -dfp [in file name] [out file name]: create dot image file for given predator genome
        else if (strcmp(argv[i], "-dfp") == 0 && (i + 2) < argc)
        {
            ++i;
            predatorAgent->loadAgent(argv[i]);
            predatorAgent->setupPhenotype();
            ++i;
            stringstream dfn;
            dfn << argv[i];
            predatorDotFileName = dfn.str();
            make_dot_pred = true;
        }
        
        // -sd [int]: set swarm starting distance (default: 30)
        else if (strcmp(argv[i], "-sd") == 0 && (i + 1) < argc)
        {
            ++i;
            startingDist = atof(argv[i]);
            
            // simulation works in distance squared
            startingDist *= startingDist;
        }
        
        // -pva [int]: set predator vision angle (default: 180)
        else if (strcmp(argv[i], "-pva") == 0 && (i + 1) < argc)
        {
            ++i;
            
            // halve angle since it's split evenly in front of the predator (required for simulation)
            predatorVisionAngle = atof(argv[i]) / 2.0;
            
            if (predatorVisionAngle < 0.5 || predatorVisionAngle > 180)
            {
                cerr << "predator vision angle must be between 1 and 360." << endl;
                exit(0);
            }
        }
        
        // -pvr [int]: set predator vision range (default: 200)
        else if (strcmp(argv[i], "-pvr") == 0 && (i + 1) < argc)
        {
            ++i;
            
            predatorVisionRange = atof(argv[i]);
            
            // simulation works in distance squared
            predatorVisionRange *= predatorVisionRange;
            
            if (predatorVisionRange < 1)
            {
                cerr << "predator vision range must be greater than 0." << endl;
                exit(0);
            }
        }
        
        // -kd [int]: set predator kill attempt delay (default: 10)
        else if (strcmp(argv[i], "-kd") == 0 && (i + 1) < argc)
        {
            ++i;
            
            killDelay = atoi(argv[i]);
        }
        
        // -np [int]: set number of predators in the simulation (default: 1)
        else if (strcmp(argv[i], "-np") == 0 && (i + 1) < argc)
        {
            ++i;
            
            numPredators = atoi(argv[i]);
        }
    }
    
    // initial population setup
    swarmAgents.resize(populationSize);
    predatorAgents.resize(populationSize);
    
    if (display_only || display_directory || make_interval_video || make_LOD_video)
    {
        // start monitor first, then abeeda
        setupBroadcast();
    }
    
    if (display_only)
    {
        // create clone swarm from that single brain
        vector<tAgent*> cloneSwarm;
        
        for (int j = 0; j < swarmSize; ++j)
        {
            tAgent *cloneAgent = new tAgent;
            cloneAgent->inherit(swarmAgent, 0.0, 0);
            cloneSwarm.push_back(cloneAgent);
        }
        
        string bestString = findBestRun(cloneSwarm, predatorAgent);
        bestString.append("X");
        doBroadcast(bestString);
        exit(0);
    }
    
    if (display_directory)
    {
        DIR *dir;
        struct dirent *ent;
        dir = opendir(argv[displayDirectoryArgvIndex]);
        
        // map: run # -> [swarm file name, predator file name]
        map< int, vector<string> > fileNameMap;
        
        if (dir != NULL)
        {
            cout << "reading in files" << endl;
            
            // read all of the files in the directory
            while ((ent = readdir(dir)) != NULL)
            {
                string dirFile = string(ent->d_name);
                
                if (dirFile.find(".genome") != string::npos)
                {
                    // find the first character in the file name that is a digit
                    int i = 0;
                    
                    for ( ; i < dirFile.length(); ++i)
                    {
                        if (isdigit(dirFile[i]))
                        {
                            break;
                        }
                    }
                    
                    // get the run number from the file name
                    int runNumber = atoi(dirFile.substr(i).c_str());
                    
                    if (fileNameMap[runNumber].size() == 0)
                    {
                        fileNameMap[runNumber].resize(2);
                    }
                    
                    // map the file name into the appropriate location
                    if (dirFile.find("swarm") != string::npos)
                    {
                        fileNameMap[runNumber][0] = argv[displayDirectoryArgvIndex] + dirFile;
                    }
                    else if (dirFile.find("predator") != string::npos)
                    {
                        fileNameMap[runNumber][1] = argv[displayDirectoryArgvIndex] + dirFile;
                    }
                }
            }
            
            closedir(dir);
        }
        else
        {
            cerr << "invalid directory: " << argv[displayDirectoryArgvIndex] << endl;
            exit(0);
        }
        
        // display every set of swarm/predator files
        for (map< int, vector<string> >::iterator it = fileNameMap.begin(), end = fileNameMap.end(); it != end; )
        {
            if (it->second[0] != "" && it->second[1] != "")
            {
                cout << "building video for run " << it->first << endl;
                
                swarmAgent->loadAgent((char *)it->second[0].c_str());
                predatorAgent->loadAgent((char *)it->second[1].c_str());
                
                // build clone swarm from the swarm agent brain
                vector<tAgent*> cloneSwarm;
                
                for (int j = 0; j < swarmSize; ++j)
                {
                    tAgent *cloneAgent = new tAgent;
                    cloneAgent->inherit(swarmAgent, 0.0, 0);
                    cloneSwarm.push_back(cloneAgent);
                }
                
                string bestString = findBestRun(cloneSwarm, predatorAgent);
                
                cout << "displaying video for run " << it->first << endl;
                
                if ( (++it) == end )
                {
                    bestString.append("X");
                }
                
                doBroadcast(bestString);
            }
            else
            {
                cerr << "unmatched file: " << it->second[0] << " " << it->second[1] << endl;
            }
        }
        
        exit(0);
    }
    
    if (make_logic_table)
    {
        swarmAgent->saveLogicTable(logicTableFileName.c_str());
        exit(0);
    }
    
    if (make_dot_swarm)
    {
        swarmAgent->saveToDot(swarmDotFileName.c_str(), false);
        exit(0);
    }
    
    if (make_dot_pred) 
    {
        predatorAgent->saveToDot(predatorDotFileName.c_str(), true);
        exit(0);
    }
    
    // seed the agents
    delete swarmAgent;
    swarmAgent = new tAgent;
    swarmAgent->setupRandomAgent(5000);
    //swarmAgent->loadAgent((char *)"startSwarm.genome");
    
    delete predatorAgent;
    predatorAgent = new tAgent;
    predatorAgent->setupRandomAgent(5000);
    predatorAgent->loadAgent((char *)"startPredator.genome");
    
    // make mutated copies of the start genome to fill up the initial population
	for(int i = 0; i < populationSize; ++i)
    {
		swarmAgents[i] = new tAgent;
		swarmAgents[i]->inherit(swarmAgent, 0.001, 1);
        
        predatorAgents[i] = new tAgent;
		predatorAgents[i]->inherit(predatorAgent, 0.02, 1);
    }
    
	SANextGen.resize(populationSize);
    PANextGen.resize(populationSize);
    
	swarmAgent->nrPointingAtMe--;
    predatorAgent->nrPointingAtMe--;
    
	cout << "setup complete" << endl;
    cout << "starting evolution" << endl;

    int generations[totalGenerations];
    int avgSwarmfitness[totalGenerations];
    int avgPredfitness[totalGenerations];
    
    // main loop
	for (int update = 1; update <= totalGenerations; ++update)
    {
        // reset fitnesses
		for(int i = 0; i < populationSize; ++i)
        {
			swarmAgents[i]->fitness = 0.0;
			//swarmAgents[i]->fitnesses.clear();
            
            predatorAgents[i]->fitness = 0.0;
			//predatorAgents[i]->fitnesses.clear();
		}
        
        // determine fitness of population
		swarmMaxFitness = 0.0;
        predatorMaxFitness = 0.0;
        double swarmAvgFitness = 0.0;
        double predatorAvgFitness = 0.0;
        
        // entire swarm population evaluated every time predator agent is evaluated; fitness is avg of all tests
        // predator agents are only evaluated once
		for(int i = 0; i < populationSize; ++i)
        {
            game->executeGame(swarmAgents, predatorAgents[i], NULL, false, startingDist, predatorVisionRange, predatorVisionAngle, killDelay, numPredators);
            
            for (int j = 0; j < swarmSize; ++j)
            {
                swarmAvgFitness += swarmAgents[j]->fitness;
                swarmAgents[j]->fitnesses.push_back(swarmAgents[j]->fitness);
            }
            
            predatorAvgFitness += predatorAgents[i]->fitness;
            
            if(predatorAgents[i]->fitness > predatorMaxFitness)
            {
                predatorMaxFitness = predatorAgents[i]->fitness;
                bestPredatorAgent = predatorAgents[i];
            }
		}
        
        // compute each swarm agent's fitness as its average fitness over all evaluations
        for (int i = 0; i < swarmSize; ++i)
        {
            swarmAgents[i]->fitness = 0.0;
            
            for (int j = 0; j < swarmAgents[i]->fitnesses.size(); ++j)
            {
                swarmAgents[i]->fitness += swarmAgents[i]->fitnesses[j];
            }
            
            swarmAgents[i]->fitness /= swarmAgents[i]->fitnesses.size();
            
            if(swarmAgents[i]->fitness > swarmMaxFitness)
            {
                swarmMaxFitness = swarmAgents[i]->fitness;
                bestSwarmAgent = swarmAgents[i];
            }
        }
        
        swarmAvgFitness /= (double)(populationSize * swarmSize);
        predatorAvgFitness /= (double)populationSize;
		avgSwarmfitness[update]=(int)swarmAvgFitness;
        generations[update]=(int)update;
        avgPredfitness[update]=(int)predatorAvgFitness;
		cout << "generation " << update << ": swarm [" << (int)swarmAvgFitness << " : " << (int)swarmMaxFitness << "] :: predator [" << (int)predatorAvgFitness << " : " << (int)predatorMaxFitness << "]" << endl;
        // cout << "generation " << update << ": swarm [" << (int)swarmAvgFitness << " : " << (int)swarmMaxFitness <<endl;
        // cout<<swarmSize<<endl;
        // display video of simulation
        if (make_interval_video)
        {
            bool finalGeneration = (update == totalGenerations);
            
            if (update % make_video_frequency == 0 || finalGeneration)
            {
                string bestString = game->executeGame(swarmAgents, bestPredatorAgent, NULL, true, startingDist, predatorVisionRange, predatorVisionAngle, killDelay, numPredators);
                
                if (finalGeneration)
                {
                    bestString.append("X");
                }
                
                doBroadcast(bestString);
            }
        }
        
        
		for(int i = 0; i < populationSize; ++i)
		{
            // construct swarm agent population for the next generation
			tAgent *offspring = new tAgent;
            int j = 0;
            
			do
            {
                j = rand() % populationSize;
            } while((j == i) || (randDouble > (swarmAgents[j]->fitness / swarmMaxFitness)));
            
			offspring->inherit(swarmAgents[j], perSiteMutationRate, update);
			SANextGen[i] = offspring;
            
            // construct predator agent population for the next generation
            offspring = new tAgent;
            j = 0;
            
			do
            {
                j = rand() % populationSize;
            } while((j == i) || (randDouble > (predatorAgents[j]->fitness / predatorMaxFitness)));
            
			offspring->inherit(predatorAgents[j], perSiteMutationRate, update);
			PANextGen[i] = offspring;
		}
        
        // shuffle the populations so there is a minimal chance of the same predator/prey combo in the next generation
        random_shuffle(SANextGen.begin(), SANextGen.end());
        random_shuffle(PANextGen.begin(), PANextGen.end());
        
		for(int i = 0; i < populationSize; ++i)
        {
            // retire and replace the swarm agents from the previous generation
			swarmAgents[i]->retire();
			swarmAgents[i]->nrPointingAtMe--;
			if(swarmAgents[i]->nrPointingAtMe == 0)
            {
				delete swarmAgents[i];
            }
			swarmAgents[i] = SANextGen[i];
            
            // retire and replace the predator agents from the previous generation
            predatorAgents[i]->retire();
			predatorAgents[i]->nrPointingAtMe--;
			if(predatorAgents[i]->nrPointingAtMe == 0)
            {
				delete predatorAgents[i];
            }
			predatorAgents[i] = PANextGen[i];
		}
        
		swarmAgents = SANextGen;
		predatorAgents = PANextGen;
        
        if (track_best_brains && update % track_best_brains_frequency == 0)
        {
            stringstream sss, pss;
            
            sss << "swarm" << update << ".genome";
            pss << "predator" << update << ".genome";
            
            swarmAgents[0]->ancestor->ancestor->saveGenome(sss.str().c_str());
            predatorAgents[0]->ancestor->ancestor->saveGenome(pss.str().c_str());
        }
	}
	
    // save the genome file of the lmrca
	swarmAgents[0]->ancestor->ancestor->saveGenome(swarmGenomeFileName.c_str());
    predatorAgents[0]->ancestor->ancestor->saveGenome(predatorGenomeFileName.c_str());
    
    // save video and quantitative stats on the best swarm and predator agent's LOD
    vector<tAgent*> swarmLOD, predatorLOD;
    
    cout << "building ancestor list" << endl;
    
    // swarm agents LOD
    // use 2 ancestors down from current population because that ancestor is highly likely to have high fitness
    tAgent* curAncestor = swarmAgents[0]->ancestor->ancestor;
    
    while (curAncestor != NULL)
    {
        // don't add the base ancestor
        if (curAncestor->ancestor != NULL)
        {
            swarmLOD.insert(swarmLOD.begin(), curAncestor);
        }
        
        curAncestor = curAncestor->ancestor;
    }
    
    // predator agents LOD
    curAncestor = predatorAgents[0]->ancestor->ancestor;
    
    while (curAncestor != NULL)
    {
        // don't add the base ancestor
        if (curAncestor->ancestor != NULL)
        {
            predatorLOD.insert(predatorLOD.begin(), curAncestor);
        }
        
        curAncestor = curAncestor->ancestor;
    }
    
    FILE *LOD = fopen(LODFileName.c_str(), "w");

    fprintf(LOD, "generation,prey_fitness,predator_fitness,num_alive_end,avg_bb_size,var_bb_size,avg_shortest_dist,swarm_density_count,prey_neurons_connected_prey_retina,prey_neurons_connected_predator_retina,predator_neurons_connected_prey_retina,num_attacks\n");
    
    cout << "analyzing ancestor list" << endl;
    
    for (int i = 0; i < swarmLOD.size(); ++i)
    {
        // collect quantitative stats
        vector<tAgent*> cloneSwarm;
        
        for (int j = 0; j < swarmSize; ++j)
        {
            tAgent *cloneAgent = new tAgent;
            cloneAgent->inherit(swarmLOD[i], 0.0, swarmLOD[i]->born);
            cloneSwarm.push_back(cloneAgent);
            // cout<<cloneAgent<<endl;
        }
        
        game->executeGame(cloneSwarm, predatorLOD[i], LOD, false, startingDist, predatorVisionRange, predatorVisionAngle, killDelay, numPredators);
        
        // make video
        if (make_LOD_video)
        {
            string bestString = findBestRun(cloneSwarm, predatorLOD[i]);
            
            if ( (i + 1) == swarmLOD.size() )
            {
                bestString.append("X");
            }
            
            doBroadcast(bestString);
        }
        
        for (int j = 0; j < swarmSize; ++j)
        {
            delete cloneSwarm[j];
        }
        
        cloneSwarm.clear();
    }
    
    fclose(LOD);



//*****************************************************************************************************************

    ofstream file1("SwarmFitness.csv");

    if (file1.is_open()) {
        // Assuming you are writing some x and y data points
        for (int i = 0; i < totalGenerations; ++i) {
            double x2 = generations[i]; 
            double y2 = avgSwarmfitness[i]; 
            file1 << x2 << "," << y2 << "\n";
        }
        file1.close();
    } else {
        cout << "Unable to open file";
    }

//********************************************************************************************************************
     ofstream file2("PredatorFitness.csv");

    if (file2.is_open()) {
        // Assuming you are writing some x and y data points
        for (int i = 0; i < totalGenerations; ++i) {
            double x3 = generations[i]; 
            double y3 = avgPredfitness[i]; 
            file2 << x3 << "," << y3 << "\n";
        }
        file2.close();
    } else {
        cout << "Unable to open file";
    }

//************************************************************************************************************
    return 0;
}

string findBestRun(vector<tAgent*> swarmAgents, tAgent *predatorAgent)
{
    string reportString = "", bestString = "";
    double bestFitness = 0.0;
    
    for (int rep = 0; rep < 100; ++rep)
    {
        reportString = game->executeGame(swarmAgents, predatorAgent, NULL, true, startingDist, predatorVisionRange, predatorVisionAngle, killDelay, numPredators);
        
        double swarmFitness = 0.0;
        
        for (int i = 0; i < swarmSize; ++i)
        {
            swarmFitness += swarmAgents[i]->fitness;
        }
        
        swarmFitness /= (double)swarmSize;
        
        if (swarmFitness > bestFitness)
        {
            bestString = reportString;
            bestFitness = swarmFitness;
        }
    }
    
    return bestString;
}

void setupBroadcast(void)
{
    port = ECHO_PORT;
	if ( (list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
		fprintf(stderr, "ECHOSERV: Error creating listening socket.\n");
    }
	/*  Set all bytes in socket address structure to
	 zero, and fill in the relevant data members   */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);
	/*  Bind our socket addresss to the 
	 listening socket, and call listen()  */
    if ( bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 )
    {
		fprintf(stderr, "ECHOSERV: Error calling bind()\n");
    }
    if ( listen(list_s, LISTENQ) < 0 )
    {
		fprintf(stderr, "ECHOSERV: Error calling listen()\n");
    }
}

void doBroadcast(string data)
{
    if ( (conn_s = accept(list_s, NULL, NULL) ) < 0 )
    {
        fprintf(stderr, "ECHOSERV: Error calling accept()\n");
    }
    Writeline(conn_s, data.c_str(), data.length());
    
    if ( close(conn_s) < 0 )
    {
        fprintf(stderr, "ECHOSERV: Error calling close()\n");
    }
}
