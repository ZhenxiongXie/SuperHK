#include <fstream>
#include <iostream>
#include <chrono>

#include "event/Sample.h"
#include "event/BeamSample.h"
#include "event/ChiSquared.h"

#include "physics/Oscillator.h"
#include "physics/ParameterSpace.h"

#include "TF1.h"
#include "TTree.h"
#include "TKey.h"
#include "TRandom3.h"
#include "TMatrixD.h"

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		std::cerr << "Fitter: need at least three parameters: id jobs card [point]"
			  << std::endl;
		return 1;
	}

	int id  = std::atoi(argv[1]);	//id of current process
	int all = std::atoi(argv[2]);	//total number of processes

	if (id >= all)
	{
		std::cerr << "Fitter: requesting process " << id
			  << ", but only " << all << " jobs" << std::endl;
		return 1;
	}

	// main card
	CardDealer cd(argv[3]);

	std::string fit_card, osc_card, sample_card;
	if (!cd.Get("oscillation_parameters", osc_card)) {
		std::cerr << "Fitter: no oscillation options card defined, very bad!" << std::endl;
		return 1;
	}
	std::shared_ptr<Oscillator> osc(new Oscillator(osc_card));
	std::unique_ptr<ParameterSpace> parms(new ParameterSpace(osc_card));

	if (!cd.Get("fit_parameters", fit_card)) {
		std::cerr << "Fitter: no fit options card defined, very bad!" << std::endl;;
		return 1;
	}
	std::unique_ptr<ChiSquared> fitter(new ChiSquared(fit_card));


	if (cd.Get("beam_parameters", sample_card))
		fitter->Add<BeamSample>(sample_card);
	if (cd.Get("atmo_parameters", sample_card))
		fitter->Add<AtmoSample>(sample_card);

	// combining samples
	if (!fitter->Combine()) {
		std::cerr << "Fitter: not possible to combine samples. Make sure at least one is defined" << std::endl;
		return 1;
	}


	// parameters for the main script
	int kVerbosity;
	if (!cd.Get("verbose", kVerbosity))
		kVerbosity = 0;
	std::string scan;
	if (!cd.Get("scan", scan))
		scan = "";

	std::string trueOrder, fitOrder;
	if (!cd.Get("true_hierarchy", trueOrder))
		trueOrder = "normal";
	if (!cd.Get("fit_hierarchy", fitOrder))
		fitOrder = "normal";
	
	//for the fake data study - set a flag here
/*	bool FakeData;
	if (!cd.Get("FakeData", FakeData))
		FakeData = false;
*/

	//open output file
	std::string outName;
	cd.Get("output", outName);	//path for out file with extension
	
	int NumSys;
	double Epsilons[3000], Errors[3000];
	double X2, ObsX2, SysX2;
	double Time;
	int Point, tPoint;
	if (!cd.Get("point", tPoint))
		tPoint = parms->GetNominalEntry();

	double M12, tM12;
	double M23, tM23;
	double S12, tS12;
	double S13, tS13;
	double S23, tS23;
	double dCP, tdCP;

	//get first file for True point

	if (kVerbosity)
		std::cout << "Fitter: getting true entry " << tPoint << std::endl;
	parms->GetEntry(tPoint, tM12, tM23, tS12, tS13, tS23, tdCP);

	//spectra for true point ( On )
	if (trueOrder == "normal")
		osc->SetMasses<Oscillator::normal>(tM12, tM23);
	else if (trueOrder == "inverted")
		osc->SetMasses<Oscillator::inverted>(tM12, tM23);
	osc->SetPMNS<Oscillator::sin2>(tS12, tS13, tS23, tdCP);
	fitter->SetPoint(tPoint);
	//Eigen::VectorXd trueSpectra = fitter->ConstructSamples(osc);
	Eigen::VectorXd trueSpectra;	// don't compute just now
	bool trueLoad = true;
//	FakeData = false;

	if (outName.find(".root") == std::string::npos)
		outName += ".root";
	outName.insert(outName.find(".root"), "." + std::to_string(id));

	TFile *outf = new TFile(outName.c_str(), "RECREATE");
	TTree *stepX2 = new TTree("stepX2Tree", "Fit Axis Info");

	stepX2->Branch("Time",		&Time, "Time/D");
	NumSys = fitter->NumSys();

	TMatrixD Covariance(NumSys,NumSys);
	
	// for CPV scans there is no need to store these
	if (scan != "CPV") {
		std::string epsilArray = "Epsilons[" +
				 std::to_string(NumSys) + "]/D";
		std::string errorArray = "Errors[" +
				 std::to_string(NumSys) + "]/D";

		stepX2->Branch("NumSys",	&NumSys,   "NumSys/I");
		stepX2->Branch("Epsilons",	Epsilons, epsilArray.c_str());
		stepX2->Branch("Errors",	Errors,   errorArray.c_str());
	}

	stepX2->Branch("X2",		&X2,		"X2/D");
	stepX2->Branch("SysX2",		&SysX2,		"SysX2/D");
	stepX2->Branch("Point",		&Point,		"Point/I");
	stepX2->Branch("TPoint",	&tPoint,	"TPoint/I");
	stepX2->Branch("CP",	&dCP,	"CP/D");
	stepX2->Branch("TCP",	&tdCP,	"TCP/D");
	stepX2->Branch("M12",	&M12,	"TM12/D");
	stepX2->Branch("TM12",	&tM12,	"TM12/D");
	stepX2->Branch("M23",	&M23,	"M23/D");
	stepX2->Branch("TM23",	&tM23,	"TM23/D");
	stepX2->Branch("S12",	&S12,	"S12/D");
	stepX2->Branch("TS12",	&tS12,	"TS12/D");
	stepX2->Branch("S13",	&S13,	"S13/D");
	stepX2->Branch("TS13",	&tS13,	"TS13/D");
	stepX2->Branch("S23",	&S23,	"S23/D");
	stepX2->Branch("TS23",	&tS23,	"TS23/D");

	int entries = parms->GetEntries();

	int off = 0;
	int fpp = entries / all;	//entries per process
	if (id < (entries % all))	//equally distribute
		++fpp;
	else
		off = entries % all;

	int nstart = std::max(off + fpp *  id, 0);
	int nend   = std::min(off + fpp * (id + 1), entries);

	if (kVerbosity)
		std::cout << "Fitter: proc " << id << " / " << all
			  << ", opening files from " << nstart << " to " << nend
			  << " ( " << fpp << " ) " << std::endl;

	auto t_start = std::chrono::high_resolution_clock::now();

	if (argc > 4) {
		nstart = std::stoi(argv[4]);
		nend = nstart + 1;
		std::cout << "Fitter: OVERRIDE fitting only point " << nstart << "\n";
	}
		
	for (int i = nstart; i < nend; ++i)
	{
		Point = i;
		parms->GetEntry(Point, M12, M23, S12, S13, S23, dCP);

		double PenX2 = parms->GetPenalty(Point);

		//fast fit flag, which means skips unless dCP is 0, ±pi, or tdCP
		if (scan == "CPV" && std::abs(dCP - tdCP) > 1e-5 &&
			     std::abs(std::sin(dCP)) > 1e-5)
			continue;

		//check the osc
                        std::cout << "\nFitter: the true point " <<  tPoint << "\n";
                        std::cout << "m23 " << tM23 << ", s13 " << tS13
                                  << ", s23 " << tS23 << ", dcp " << tdCP << std::endl;
	        if (trueOrder == "normal")
        	        osc->SetMasses<Oscillator::normal>(tM12, tM23);
        	else if (trueOrder == "inverted")
                	osc->SetMasses<Oscillator::inverted>(tM12, tM23);
        	osc->SetPMNS<Oscillator::sin2>(tS12, tS13, tS23, tdCP);
        	fitter->SetPoint(tPoint);
			trueSpectra = fitter->ConstructSamples(osc);
		// load trueSpectra now, osc has not changed yet
		if (trueLoad) {
			trueSpectra = fitter->ConstructSamples(osc);
			trueLoad = false;
		}

		if (kVerbosity) {
			std::cout << "\nFitter: now fitting point " << Point
				  << " (" << i-nstart << "/" << nend-nstart
				  << ") vs " << tPoint << "\n";
			std::cout << "m23 " << M23 << ", s13 " << S13
				  << ", s23 " << S23 << ", dcp " << dCP << std::endl;
		}

		auto t_begin = std::chrono::high_resolution_clock::now();

		//Get expected spectrum ( En )
		if (fitOrder == "normal")
			osc->SetMasses<Oscillator::normal>(M12, M23);
		else if (fitOrder == "inverted")
			osc->SetMasses<Oscillator::inverted>(M12, M23);
		osc->SetPMNS<Oscillator::sin2>(S12, S13, S23, dCP);
		fitter->SetPoint(Point);
		Eigen::VectorXd fitSpectra = fitter->ConstructSamples(osc);

		//for the new input
		//trueSpectra = fitter -> NonZeroSpectra(trueSpectra);
		//fitSpectra = fitter -> NonZeroSpectra(fitSpectra);

		//for check
//                        std::cout << "Non-zero True Spectrum for check: " << NewtrueSpectra.transpose() << std::endl;
//                        std::cout << "Non-zero Fit Spectrum for check: " << NewfitSpectra.transpose() << std::endl;

//                        std::cout << "True Spectrum for check: " << trueSpectra.transpose() << std::endl;
//			std::cout << "Fit Spectrum for check: " << fitSpectra.transpose() << std::endl;

		//for the fake data study
		
		//if (kVerbosity)
                std::cout << "True Spectrum before energy shift: " << trueSpectra.transpose()<< std::endl;

		Eigen::VectorXd epsil = Eigen::VectorXd::Zero(NumSys);
                epsil(NumSys-1) = 1;
		//if (kVerbosity)
                //        std::cout << "epsil: " << epsil << std::endl;	
		trueSpectra = fitter->GammaP(trueSpectra, epsil);

		//if (kVerbosity)
                //        std::cout << "True Spectrum after energy shift: " << trueSpectra << std::endl;

		Eigen::VectorXd eps = fitter->FitX2(trueSpectra, fitSpectra);
//		eps = fitter->FitX2(trueSpectra, fitSpectra);
		ObsX2 = fitter->ObsX2(trueSpectra, fitSpectra, eps);
		SysX2 = fitter->SysX2(eps);
		X2 = ObsX2 + SysX2 + PenX2;
		std::cout << "true spectra is " << trueSpectra.transpose() << std::endl;
		std::cout << "fit spectra is " << fitSpectra.transpose() << std::endl;

//new input
/*
		std::cout << "stops at eps" << std::endl;
		Eigen::VectorXd eps = fitter->FitX2(NewtrueSpectra, NewfitSpectra);
                //eps = fitter->FitX2(NewtrueSpectra, NewfitSpectra);
                std::cout << "stops at Obs" << std::endl;
                ObsX2 = fitter->ObsX2(NewtrueSpectra, NewfitSpectra, eps);
		std::cout << "stops at Sys" << std::endl;
                SysX2 = fitter->SysX2(eps);
                X2 = ObsX2 + SysX2 + PenX2;
		fitter-> ResetBins(trueSpectra);
*/
		/*//for check
		Eigen::VectorXd ObsX2n = fitter -> ObsX2n(trueSpectra, fitSpectra, eps);
		if (kVerbosity)
                        std::cout << "fake data study - eps: " << trueSpectra << std::endl;
		*/	
		

		if (scan != "CPV") {
			Eigen::VectorXd var = fitter->Variance(trueSpectra, fitSpectra, eps);
			Eigen::MatrixXd cov = fitter->Covariance(trueSpectra, fitSpectra, eps);
			for (int i_sys = 0; i_sys < NumSys; ++i_sys) {
				Epsilons[i_sys] = eps(i_sys);
				Errors[i_sys] = sqrt(var(i_sys));
				for (int j_sys = 0; j_sys < NumSys; ++j_sys){
					Covariance[i_sys][j_sys] = cov(i_sys,j_sys);
				}
			}
		}
		std::string cov_name = "covariance_point_"+std::to_string(Point);
		Covariance.Write(cov_name.c_str());

		if (kVerbosity)
			std::cout << "Fitter: X2 computed " << X2 << " ("
				  << ObsX2 << " + " << SysX2 << " + "
				  << PenX2 << ")\n" << std::endl;

		auto t_end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> t_duration = t_end - t_begin;

		Time = t_duration.count() / 1000.;

		stepX2->Fill();

		if ((t_end - t_start).count() / 1000 > 1000) {	// 1000 s
			stepX2->Write();
			t_start = std::chrono::high_resolution_clock::now();
		}
	}

	std::cout << "Fitter: Finished and out" << std::endl;

	outf->cd();
	stepX2->Write();
	outf->Close();

	return 0;
}
