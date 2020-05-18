#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

#include "TFile.h"
#include "TTree.h"

class sorter
{
	public:
		sorter(std::vector<double> &v) : vorder(v) {};
		bool operator()(int a, int b)
		{
			return vorder.at(a) < vorder.at(b);
		}
	private:
		std::vector<double> vorder;
};

int main(int argc, char** argv)
{
	//std::map<double, double> compCP, compX2, minCP, minX2;
	std::map<double, double> trueX2, compX2, systX2;

	//std::map<double, double> minS13, minS23, minM23;
	std::map<double, int> minPoint;
	double S13, S23, M23;
	double TS13, TS23, TM23;
	int point;

	if (argc < 3)
	{
		std::cerr << "Name file required." << std::endl;
		return 1;
	}

	std::string cmd = "find " + std::string(argv[1]) + "/scan_* -name \"" +
			  std::string(argv[2]) + ".*.root\" > .tmp_exclusion";
	std::cout << cmd << std::endl;
	system(cmd.c_str());

	std::string file;
	std::ifstream listExclusion(".tmp_exclusion");
	while (std::getline(listExclusion, file))
	//for (int f = 1; f < argc; ++f) 
	{
		std::cout << "opening " << file << std::endl;
		TFile inf(file.c_str(), "READ");
		TTree *t = static_cast<TTree*>(inf.Get("stepX2Tree"));
		double X2, sysX2, dCP, tdCP;
		t->SetBranchAddress("X2",  &X2);
		t->SetBranchAddress("SysX2", &sysX2);
		t->SetBranchAddress("CP",  &dCP);
		t->SetBranchAddress("TCP", &tdCP);
		t->SetBranchAddress("S13", &S13);
		t->SetBranchAddress("S23", &S23);
		t->SetBranchAddress("M23", &M23);
		t->SetBranchAddress("TS13", &TS13);
		t->SetBranchAddress("TS23", &TS23);
		t->SetBranchAddress("TM23", &TM23);
		t->SetBranchAddress("Point", &point);

		t->GetEntry(0);

		double min_X2 = X2;
		double min_CP = dCP;

		double comp_X2 = -1;
		double comp_CP = -1;

		std::cout << "looping on " << t->GetEntries() << std::endl;
		for (int i = 0; i < t->GetEntries(); ++i)
		{
			t->GetEntry(i);

			if (std::abs(S23-0.528) > 1e-6)
				continue;

			if (std::abs(dCP - tdCP) < 1e-5)
			{
				if (!trueX2.count(tdCP))
					trueX2[tdCP] = X2;
				else if (X2 < trueX2[tdCP])
				{
					trueX2[tdCP] = X2;
				}
			}

			if (S13 != TS13 || S23 != TS23 || M23 != M23)
				continue;

			if (1 - std::abs(std::cos(dCP)) < 1e-5)
			{
				if (!compX2.count(tdCP))
				{
					compX2[tdCP] = X2;
					systX2[tdCP] = sysX2;
				}
				else if (X2 < compX2[tdCP])
				{
					compX2[tdCP] = X2;
					systX2[tdCP] = sysX2;

					//minS13[tdCP] = S13;
					//minS23[tdCP] = S23;
					//minM23[tdCP] = M23;
					minPoint[tdCP] = point;
				}
			}
		}

		/*
		if (!compCP.count(tdCP) && comp_CP > 0) 
		{
		       compCP[tdCP] = comp_CP;
		       compX2[tdCP] = comp_X2;
		}
		else if (compX2[tdCP] > comp_X2 && comp_CP > 0)
		{
		       compCP[tdCP] = comp_CP;
		       compX2[tdCP] = comp_X2;
		}

		if (!minCP.count(tdCP)) 
		{
		       minCP[tdCP] = min_CP;
		       minX2[tdCP] = min_X2;
		}
		else if (minX2[tdCP] > min_X2)
		{
		       minCP[tdCP] = min_CP;
		       minX2[tdCP] = min_X2;
		}
		*/

		inf.Close();
	}
	listExclusion.close();

	std::ofstream out("Exclusion.dat");
	std::map<double, double>::iterator im;
	for (im = trueX2.begin(); im != trueX2.end(); ++im)
	{
		//std::cout << minPoint[im->first] << " -> " << im->first
		//	  << ":\t(" << minS13[im->first]
		//	  << ", " << minS23[im->first] << ", "
		//	  << minM23[im->first] << ")" << std::endl;
		out << im->first << "\t"
		    << std::sqrt(std::abs(im->second - compX2[im->first])) << "\t"
		    << im->second << "\t" << systX2[im->first] << "\t"
		    << compX2[im->first] << "\t" << minPoint[im->first] << std::endl;
		   // << "\t" << im->second << "\t" << compX2[im->first] << "\t"
		   // << minCP[im->first] << "\t" << minX2[im->first] << std::endl;
	}

	return 0;
}
