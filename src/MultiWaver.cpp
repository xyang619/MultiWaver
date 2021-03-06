/*
 * MultiWaveInfer.cpp
 *
 *  Created on: May 26, 2015
 *  Author: young
 */
#include <map>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "Utils.hpp"

//#include "omp.h"

using namespace std;


int main(int argc, char **argv)
{
	if (argc < 2)
	{
		cerr << "Need more arguments than provided, please use -h/--help to get help" << endl;
		exit(1);
	}
	string filename = "";
	double lower = 0;
	double alpha = 0.001;
	double epsilon = 0.000001;
	double minP = 0.05;
	int maxIter = 10000;
	bool simpleMode = false;
	for (int i = 1; i < argc; ++i)
	{
		string arg(argv[i]);
		if (arg == "-h" || arg == "--help")
		{
			help();
			exit(0);
		}
		else if (arg == "-i" || arg == "--input")
		{
			filename = string(argv[++i]);
		}
		else if (arg == "-l" || arg == "--lower")
		{
			lower = atof(argv[++i]);
		}
		else if (arg == "-m" || arg == "--maxIter")
		{
			maxIter = atoi(argv[++i]);
		}
		else if (arg == "-a" || arg == "--alpha")
		{
			alpha = atof(argv[++i]);
		}
		else if (arg == "-e" || arg == "--epsilon")
		{
			epsilon = atof(argv[++i]);
		}
		else if (arg == "-p" || arg == "--minProp")
		{
			minP = atof(argv[++i]);
		}
		else if (arg == "-s" || arg == "--simple")
		{
			simpleMode = true;
		}
		else
		{
			cerr << "unrecognizable argument found, please check again!" << endl;
			exit(1);
		}
	}
	if (filename.size() == 0)
	{
		cerr << "Input file name required, please check help" << endl;
		exit(1);
	}
	//echo command entered
	cout << endl << "// COMMAND ";
	for (int i = 0; i < argc; ++i)
	{
		cout << argv[i] << " ";
	}
	cout << endl << endl;

	srand(time(NULL));
	vector<double> observ;
	//Reading test data from file named as test.dat
	ifstream fin(filename.c_str());
	if (!fin.is_open())
	{
		cout << "Can't open file " << filename << endl;
		exit(1);
	}
	cout << "Reading data from " << filename << "..." << endl;
	map<string, vector<double> > segs;
	map<string, double> sumLengths;
	vector<string> labels;
	double totalLength = 0;
	string line;
	while (getline(fin, line))
	{
		istringstream ss(line);
		double start, end;
		string label;
		ss >> start >> end >> label;
		double len = end - start;
		//no key not found, then create an new one
		if (segs.find(label) == segs.end())
		{
			vector<double> tmp;
			segs[label] = tmp;
			sumLengths[label] = 0.0;
			labels.push_back(label);
		}
		sumLengths.at(label) += len;
		totalLength += len;
		//greater than or equal to cutoff
		if (len > lower)
		{
			segs.at(label).push_back(len - lower);
		}
	}
	fin.close();
	
    //TODO validate the input data, to make sure the input has valid data
    int numLabel = static_cast<int>(labels.size());
    bool hasEmpty = false;
    for (int i = 0; i < numLabel; ++i)
    {
        if(segs.at(labels.at(i)).size() <= 0)
        {
            cout << "Ancestral population " << labels.at(i) << " has no data after filtering!" << endl;
            cout << "Please check your input or decrease the lower bound and try again!" << endl;
            hasEmpty = true;
            break;
        } 
    }
    if (hasEmpty)
    {
        exit(1);
    }
    
	cout << "Start scan for admixture waves... " << endl;
	map<string, double> mixtureProps; //S_k
	map<string, ParamExp> optPars;
	double criticalValue = cv_chisq(2, alpha);
	//#pragma omp parallel for
	for (int i = 0; i < numLabel; ++i)
	{
		string label = labels.at(i);
		cout << "Perform scanning for waves of population " << label << "..." << endl;
		mixtureProps[label] = sumLengths.at(label) / totalLength;
		ParamExp par = findOptPar(segs.at(label), maxIter, mixtureProps.at(label), criticalValue, epsilon, minP, simpleMode);
		solveTrueProp(par, lower);
		optPars[label] = par;
	}
	cout << "Finished scanning for admixture waves." << endl << endl;
//	for (int i = 0; i < numLabel; ++i)
//	{
//		string label = labels.at(i);
//		cout << "Population: " << label << "; Mix proportion: " << mixtureProps.at(label) << endl;
//		cout << "Optimal Parameters: ";
//		optPars.at(label).print();
//	}

	int totalNumOfWaves = 0;
	vector<int> popOrder;
	/*
	 * calculate survivalProportion m_Ik(j) of each wave
	 */
	map<int, vector<double> > survivalProps; //m_Ik(j)
	for (int i = 0; i < numLabel; ++i)
	{
		string label = labels.at(i);
		int numOfExp = optPars.at(label).getK();
		totalNumOfWaves += numOfExp;
		vector<double> tempMIK;
		double tempSum = 0;
		double temp[numOfExp];
		for (int j = 0; j < numOfExp; ++j)
		{
			popOrder.push_back(i);
			temp[j] = optPars.at(label).getProp(j) / optPars.at(label).getLambda(j);
			tempSum += temp[j] / temp[0];
		}
		tempSum = mixtureProps.at(label) / tempSum;
		for (int j = 0; j < numOfExp; ++j)
		{
			tempMIK.push_back(tempSum * temp[j] / temp[0]);
		}
		survivalProps[i] = tempMIK;
	}

	bool hasSolution = false;
	if (totalNumOfWaves > 2)
	{
		cout << "There are " << totalNumOfWaves - 1 << " waves of admixture events detected" << endl;
	}
	else
	{
		cout << "There is only " << totalNumOfWaves - 1 << " wave of admixture event detected" << endl;
	}
	cout << "-----------------------------------------------------------------------------" << endl;
	cout << setw(44) << "Results summary" << endl << endl;
	cout << setw(32) << "Parental population" << setw(32) << "Admixture proportion" << endl;
	for (map<string, double>::iterator iter = mixtureProps.begin(); iter != mixtureProps.end(); iter++)
	{
		cout << setw(32) << iter->first << setw(32) << iter->second << endl;
	}
	cout << endl;

	int scenarioCount = 0;
	vector<vector<int> > allOrder = perm(popOrder);
	for (vector<vector<int> >::iterator iter = allOrder.begin(); iter != allOrder.end(); ++iter)
	{

		/*for a given order, calculate alpha, H and time T */
		double mInOrder[totalNumOfWaves];
		for (int i = 0; i < numLabel; ++i)
		{
			int index = 0;
			for (int j = 0; j < totalNumOfWaves; ++j)
			{
				if (iter->at(j) == i)
				{
					mInOrder[j] = survivalProps[i].at(index);
					index++;
				}
			}
		}

		/* calculate alpha */
		double alphaInOrder[totalNumOfWaves];
		double multiplier = 1.0;
		alphaInOrder[0] = mInOrder[0];
		for (int i = 1; i < totalNumOfWaves - 1; ++i)
		{
			multiplier *= (1 - alphaInOrder[i - 1]);
			alphaInOrder[i] = mInOrder[i] / multiplier;
		}
		//here make sure the proportions of first two populations sum to 1
		alphaInOrder[totalNumOfWaves - 1] = 1.0 - alphaInOrder[totalNumOfWaves - 2];

		/* calculate total ancestry proportion of kth ancestral population at t generation H_k(t)*/
		double hInOrder[numLabel][totalNumOfWaves];
		/*
		 * Initial last element of H for each population.
		 * look at the last two positions of population order: if the second last
		 * position is from population k, then set the last value of H for population
		 * k as the alpha in order of the second last value; if the last position is
		 * from population k, then set the last value of H for population k as the alpha
		 * in order of the last value; otherwise, set the last value of H for population
		 * k as 0.0
		 */
		for (int i = 0; i < numLabel; ++i)
		{
			int lastIndexOfH = totalNumOfWaves - 2;
			for (int j = 0; j < numLabel; ++j)
			{
				if (iter->at(lastIndexOfH) == j)
				{
					hInOrder[j][lastIndexOfH] = alphaInOrder[lastIndexOfH];
				}
				else if (iter->at(lastIndexOfH + 1) == j)
				{
					hInOrder[j][lastIndexOfH] = alphaInOrder[lastIndexOfH + 1];
				}
				else
				{
					hInOrder[j][lastIndexOfH] = 0;
				}
			}
			/*
			 * Recursively update H
			 * if alpha in order from population k, then update H as H*(1-alpha)+alpha;
			 * else update H as H*(1-alpha)
			 */
			for (int j = totalNumOfWaves - 3; j >= 0; --j)
			{
				if (iter->at(j) == i)
				{
					hInOrder[i][j] = hInOrder[i][j + 1] * (1 - alphaInOrder[j]) + alphaInOrder[j];
				}
				else
				{
					hInOrder[i][j] = hInOrder[i][j + 1] * (1 - alphaInOrder[j]);
				}
			}
		}
        
        for (int i = 0; i < numLabel; ++i)
        {
            hInOrder[i][totalNumOfWaves - 1] = hInOrder[i][totalNumOfWaves - 2];
        }

		double admixTime[totalNumOfWaves]; //here is delta T between waves, in reverse order
		int indexes[numLabel];
		for (int i = 0; i < numLabel; ++i)
		{
			indexes[i] = 0;
		}

		for (int i = 0; i < totalNumOfWaves; ++i)
		{
			for (int j = 0; j < numLabel; ++j)
			{
				if (iter->at(i) == j)
				{
					double tempSum = 0;
					for (int k = 0; k < i; ++k)
					{
						tempSum += (1 - hInOrder[j][k]) * admixTime[k];
					}
					double rate = optPars.at(labels.at(j)).getLambda(indexes[j]) - tempSum;
					admixTime[i] = rate / (1 - hInOrder[j][i]);
					indexes[j]++;
					break;
				}
			}
		}
        
		/* 
         * Check whether the results is reasonable or not
         * Assume total N waves of admixture events, let T[i] denotes the admixture time of i+1 wave
         *  t[i] denotes the time difference between i and i+1 wave
         * then T[i] = sum_{j=0}^i (t[j]}
         * if the order is reasonable, we must make sure T[0] <= T[1] <= ... <= T[N-2] and T[N-3] <= T[N-1]
         * T[0] <= T[1] <= ... <= T[N-2] => t[i] >= 0 for i = 0, 1, 2, ..., N-2
         * and T[N-3] <= T[N-1] => T[N-3] <= T[N-3] + t[N-2] + t[N-1] => t[N-2] + t[N-1] >= 0
         */
         
		bool isReasonable = true;
        
		for (int i = 0; i < totalNumOfWaves - 1; ++i)
		{
			if (admixTime[i] < 0)
			{
				isReasonable = false;
				break;
			}
		}
        if (admixTime[totalNumOfWaves - 2] + admixTime[totalNumOfWaves - 1] < 0)
        {
            isReasonable = false;
        }
        
		for (int i = 1; i < totalNumOfWaves; ++i)
		{
			admixTime[i] += admixTime[i - 1];
		}
        /* 
         * This part is to check whether the times of first admixture events are closer enough
         * In theory, these two times should be the same, in order to keep at least one possible result,
         * The filtering is toggered off now
         */
//		if (abs(admixTime[totalNumOfWaves - 1] - admixTime[totalNumOfWaves - 2]) / admixTime[totalNumOfWaves - 1] > 0.05)
//		{
//			isReasonable = false;
//		}
        
		if (isReasonable)
		{
			if (!hasSolution)
			{
				hasSolution = true;
			}
			cout << endl;
			cout << "Possible scenario: #" << ++scenarioCount << endl;
			//average first two as the time of initial admixture
			double meanTime = (admixTime[totalNumOfWaves - 1] + admixTime[totalNumOfWaves - 2]) / 2.0;
			if (iter->at(totalNumOfWaves - 1) % 2 == 0)
			{
				//cout << setw(10) << admixTime[totalNumOfWaves - 1]; //time
				cout << setw(10) << meanTime;
				cout << ": (" << setw(1) << iter->at(totalNumOfWaves - 1); //population
				cout << ", " << setw(10) << alphaInOrder[totalNumOfWaves - 1]; //proportion
				cout << ") =========>||<========= (";
				cout << setw(1) << iter->at(totalNumOfWaves - 2) << ", "; //population
				cout << setw(10) << alphaInOrder[totalNumOfWaves - 2] << ") :"; //proportion
				//cout << setw(10) << admixTime[totalNumOfWaves - 2] << endl; //time
				cout << setw(10) << meanTime << endl;
			}
			else
			{
				//cout << setw(10) << admixTime[totalNumOfWaves - 2]; //time
				cout << setw(10) << meanTime;
				cout << ": (" << setw(1) << iter->at(totalNumOfWaves - 2); //population
				cout << ", " << setw(10) << alphaInOrder[totalNumOfWaves - 2]; //proportion
				cout << ") =========>||<========= (";
				cout << setw(1) << iter->at(totalNumOfWaves - 1) << ", "; //population
				cout << setw(10) << alphaInOrder[totalNumOfWaves - 1] << ") :"; //proportion
				//cout << setw(10) << admixTime[totalNumOfWaves - 1] << endl; //time
				cout << setw(10) << meanTime << endl;
			}
			for (int i = totalNumOfWaves - 3; i >= 0; --i)
			{
				//cout << setw(40) << "|" << endl << setw(40) << "|" << endl << setw(40) << "|" << endl;
				cout << setw(40) << "||" << endl << setw(40) << "||" << endl << setw(40) << "||" << endl;
				if (iter->at(i) % 2 == 0)
				{
					cout << setw(10) << admixTime[i] << ": (" << setw(1) << iter->at(i);
					cout << ", " << setw(10) << alphaInOrder[i] << ") =========>||" << endl;
				}
				else 
				{
					cout << setw(40) << "||" << "<========= ("<< setw(1) << iter->at(i) << ", "; 
					cout << setw(10) << alphaInOrder[i] << ") :" << setw(10) << admixTime[i] << endl;
				}
			}
			//cout << setw(40) << "|" << endl << setw(40) << "|" << endl << setw(40) << "|" << endl << endl;
			cout << setw(40) << "||" << endl << setw(40) << "||" << endl << setw(40) << "||" << endl << endl;
		}
	}
	if (!hasSolution)
	{
		cout << "No possible scenario for current settings, please change settings and retry!" << endl;
	}
	else
	{
		cout << "Hint: " << endl;
		for (int i = 0; i < numLabel; ++i)
		{
			cout << i << ": population-" << labels.at(i) << "; ";
		}
		cout << endl;
	}
	cout << "-----------------------------------------------------------------------------" << endl;

	return 0;
}
