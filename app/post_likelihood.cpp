#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdlib.h>

#include "TH1.h"
#include "TMath.h"
#include "TH2.h"
#include "TTree.h"
#include "TFile.h"
#include "TROOT.h"
#include "TCanvas.h"

#include "tools/CardDealer.h"
#include "physics/ParameterSpace.h"

int main(int argc, char** argv)
{
	double X2;
	double CP;
//	int point;
	double M23;
	double S13;
	double S23;

	std::string ModelType = argv[1]; // syst model: T2K/HK
        std::string ESType = argv[2]; // energy scale model: 1/E/M
	std::string EnergyScale = argv[3];
	int Syst = 0; 
	int SystNum = 0;
	int ESlocation = 0;

//HK syst model have 108 systematics and T2K have 110
	if (ModelType == "HK"){
		Syst = 108;
	}
		else if (ModelType == "T2K"){
			Syst = 110;
		}
			else {
			std::cout << "input model type is wrong! It should be: HK or T2K. " << std::endl;
			}

//energy scale has two case
//one energy scale (both e-like and mu-like have same value and they are fully correlated.)
//two energy scales (one for e-like and anther for mu-like and they are uncorrelated.)
	if (ESType == "1"){
		SystNum = Syst;
		ESlocation = SystNum - 1;
	}
		else if (ESType == "E"){
			SystNum = Syst + 1;
			ESlocation = SystNum - 2;
		}
			else if (ESType == "M"){
				SystNum = Syst + 1;
				ESlocation = SystNum - 1;
			}
				else{
					std::cout << "input escale type is wrong! It should be: 1, E, or M. " << std::endl;
				}

			
	double EScale = atof(argv[3]); //energy scale input
	double Epsilons[SystNum];
	double Errors[SystNum];
	
	gROOT->SetBatch(1);

//for write in the root file
/*
	int nbinsCP = 61;
	int nbinsM23 = 13;
	int nbinsS13 = 13;
	int nbinsS23 = 19;
	int nbinsEpsilon = 100;
*/
//	int nbinsX2 = 100;
//	int n_syst = 108;

	TFile *inf = new TFile(argv[4], "READ");
	TTree* sX2 = static_cast<TTree*>(inf->Get("stepX2Tree"));
	sX2->SetBranchAddress("CP",	&CP);
	sX2->SetBranchAddress("X2",	&X2);
	sX2->SetBranchAddress("M23",  &M23);
	sX2->SetBranchAddress("S13",  &S13);
	sX2->SetBranchAddress("S23",  &S23);
	sX2->SetBranchAddress("Epsilons",	Epsilons);
	sX2->SetBranchAddress("Errors", Errors);

//mutiply escale
	double EnergyError = 0.01*EScale*Epsilons[ESlocation];
	double ESpercent = 0.01*EScale;
	std::string eps = std::to_string(ESpercent) + "*Epsilons[";
	eps += std::to_string(ESlocation);
	eps += "]";
	std::string error = "Errors[";
	error += std::to_string(ESlocation);
	error += "]";

	TCanvas* c1 = new TCanvas("c1", "c1");//create a canvas

	//TH2D* h2 = new TH2D("eScale", "eScale", nbinsEpsilon, -0.5, -0.5, nbinsEpsilon, -0.01, 0.1);
	//TH2D* h2_l = new TH2D("likelihood_dCP", "likelihood_dCP", nbinsEpsilon, -0.5, 0.5, nbinsX2, 0, 1000);
/*for full root file
	TH2D* h2_dcp = new TH2D("energyscale vs dcp", "es vs dcp", nbinsCP, -1*TMath::Pi(), TMath::Pi(), nbinsEpsilon, -0.025, 0.025);
	TH2D* h2_m23 = new TH2D("energyscale vs dm23", "es vs dm23", nbinsM23, 0.002464, 0.002554, nbinsEpsilon, -0.025, 0.025);
	TH2D* h2_s13 = new TH2D("energyscale vs sin13", "es vs sin13", nbinsS13, 0.0197, 0.0239, nbinsEpsilon, -0.025, 0.025);
	TH2D* h2_s23 = new TH2D("energyscale vs sin23", "es vs sin23", nbinsS23, 0.426, 0.579, nbinsEpsilon, -0.025, 0.025);

	//

	for (int n = 0; n < sX2->GetEntries(); n++)
	{
		sX2->GetEntry(n);
		//h2->Fill(CP, Epsilons[110]);
		//h2->Fill(Epsilons[i_syst], Epsilons[107]);
		//h2_l->Fill(Epsilons[107], X2);

                h2_dcp->Fill(CP, EnergyError);
                h2_m23->Fill(M23, EnergyError);
                h2_s13->Fill(S13, EnergyError);
                h2_s23->Fill(S23, EnergyError);
	}
*/

	std::string eps1 = eps+":CP";
	std::string eps2 = eps+":M23";
	std::string eps3 = eps+":S13";
	std::string eps4 = eps+":S23";

        sX2->Draw(eps1.c_str(), "(S23>=0.5275)&&(S23<0.5281)&&(M23<=0.002502)&&(M23>0.002501)&&(S13>=0.0215)&&(S13<0.022)", "colz");
        c1->SaveAs(("correlation/Nominal/"+ModelType+ESType+EnergyScale+"_ES_CP_Nominal.pdf").c_str());

        sX2->Draw(eps2.c_str(), "(S23>=0.5275)&&(S23<0.5281)&&(CP>=-1.57079633)&&(CP<1.57079632)&&(S13>=0.0215)&&(S13<0.022)", "colz");
        c1->SaveAs(("correlation/Nominal/"+ModelType+ESType+EnergyScale+"_ES_M23_Nominal.pdf").c_str());

        sX2->Draw(eps3.c_str(), "(S23>=0.5275)&&(S23<0.5281)&&(M23<=0.002502)&&(M23>0.002501)&&(CP>=-1.57079633)&&(CP<1.57079632)", "colz");
        c1->SaveAs(("correlation/Nominal/"+ModelType+ESType+EnergyScale+"_ES_S13_Nominal.pdf").c_str());

        sX2->Draw(eps4.c_str(), "(CP>=-1.57079633)&&(CP<1.57079632)&&(M23<=0.002502)&&(M23>0.002501)&&(S13>=0.0215)&&(S13<0.022)", "colz");
        c1->SaveAs(("correlation/Nominal/"+ModelType+ESType+EnergyScale+"_ES_S23_Nominal.pdf").c_str());
/*
        sX2->Draw(eps1.c_str(), "colz");
        c1->SaveAs(("correlation/All"+ModelType+ESType+EnergyScale+"_ES_CP_All.pdf").c_str());

        sX2->Draw(eps2.c_str(), "colz");
        c1->SaveAs(("correlation/All"+ModelType+ESType+EnergyScale+"_ES_M23_All.pdf").c_str());

        sX2->Draw(eps3.c_str(), "colz");
        c1->SaveAs(("correlation/All"+ModelType+ESType+EnergyScale+"_ES_S13_All.pdf").c_str());

        sX2->Draw(eps4.c_str(), "colz");
        c1->SaveAs(("correlation/All"+ModelType+ESType+EnergyScale+"_ES_S23_All.pdf").c_str());
*/
        sX2->Draw(error.c_str(),"(S23>=0.5275)&&(S23<0.5281)&&(M23<=0.002502)&&(M23>0.002501)&&(S13>=0.0215)&&(S13<0.022)", "hist");
        c1->SaveAs(("correlation/Error/"+ModelType+ESType+EnergyScale+"error_CPfull.pdf").c_str());
/*
	sX2->Draw(error.c_str(),"(CP>=-1.57079633)&&(CP<1.57079632)&&(S23>=0.5275)&&(S23<0.5281)&&(M23<=0.002502)&&(M23>0.002501)&&(S13>=0.0215)&&(S13<0.022)", "hist");
        c1->SaveAs(("correlation/CP0/Error/"+ModelType+ESType+EnergyScale+"error_CPcentral.pdf").c_str());
        sX2->Draw(error.c_str(),"((CP<-1.57079633)||(CP>=1.57079632))&&(S23>=0.5275)&&(S23<0.5281)&&(M23<=0.002502)&&(M23>0.002501)&&(S13>=0.0215)&&(S13<0.022)", "hist");
        c1->SaveAs(("correlation/CP0/Error/"+ModelType+ESType+EnergyScale+"error_CPrest.pdf").c_str());
*/
/*
	TFile *outf = new TFile(argv[5], "RECREATE");
	outf->cd();
//	h2->Write();
//	h2_l->Write();
	h2_dcp->Write();
	h2_m23->Write();
	h2_s13->Write();
	h2_s23->Write();
	outf->Close();
	inf->Close();
*/
	return 0;
}
