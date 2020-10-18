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
	if (argc < 2)
	{
		std::cerr << "Validation: need onw parameters: card" << std::endl;
		return 1;
	}

	// main card
	std::string cardFile = argv[1];
	CardDealer *cd = new CardDealer(cardFile);

	std::string fit_card, osc_card, sample_card;
	if (!cd->Get("oscillation_parameters", osc_card)) {
		std::cerr << "Fitter: no oscillation options card defined, very bad!" << std::endl;
		return 1;
	}
	Oscillator *osc = new Oscillator(osc_card);
	ParameterSpace *parms = new ParameterSpace(osc_card);

	if (!cd->Get("fit_parameters", fit_card)) {
		std::cerr << "Fitter: no fit options card defined, very bad!" << std::endl;;
		return 1;
	}
	ChiSquared *fitter = new ChiSquared(fit_card);

	if (cd->Get("beam_parameters", sample_card))
		fitter->Add<BeamSample>(sample_card);
	if (cd->Get("atmo_parameters", sample_card))
		fitter->Add<AtmoSample>(sample_card);

	// combining samples
	if (!fitter->Combine()) {
		std::cerr << "Fitter: not possible to combine samples. Make sure at least one is defined" << std::endl;
		return 1;
	}

	std::string outfile;
	if (!cd->Get("output", outfile))
		outfile = "validation.dat";
	else if (outfile.find(".dat") == std::string::npos)
		outfile += ".dat";

	// parameters for the main script
	int kVerbosity;
	if (!cd->Get("verbose", kVerbosity))
		kVerbosity = 0;

	std::string trueOrder, fitOrder;
	if (!cd->Get("true_hierarchy", trueOrder))
		trueOrder = "normal";
	if (!cd->Get("fit_hierarchy", fitOrder))
		trueOrder = "normal";

	double X2, ObsX2, SysX2;
	double Time;
	int truePoint, fitPoint;
	if (!cd->Get("point", truePoint))
		truePoint = parms->GetNominalEntry();
	if (!cd->Get("fit_point", fitPoint))
		fitPoint = -1;

	double tM12, fM12;
	double tM23, fM23;
	double tS12, fS12;
	double tS13, fS13;
	double tS23, fS23;
	double tdCP, fdCP;

	//get True point
	//parms->GetEntry(truePoint, tM12, tM23, tS12, tS13, tS23, tdCP);
	tM12 = 7.49e-5; tM23 = 2.5e-3; tS12 = 0.309; tS13 = 0.021; tS23 = 0.425; tdCP = -2.79252680319;

	if (trueOrder == "normal")
		osc->SetMasses<Oscillator::normal>(tM12, tM23);
	else if (trueOrder == "inverted")
		osc->SetMasses<Oscillator::inverted>(tM12, tM23);
	osc->SetPMNS<Oscillator::sin2>(tS12, tS13, tS23, tdCP);
	std::map<std::string, Eigen::VectorXd> trueSpectra = fitter->BuildSamples(osc);
	std::map<std::string, Eigen::VectorXd> fitSpectra;

	if (fitPoint >= 0) {
		//get Fit point
		//parms->GetEntry(fitPoint, fM12, fM23, fS12, fS13, fS23, fdCP);
		fM12 = 7.49e-5; fM23 = 2.5e-3; fS12 = 0.309; fS13 = 0.021; fS23 = 0.425; fdCP = 0;

		if (fitOrder == "normal")
			osc->SetMasses<Oscillator::normal>(fM12, fM23);
		else if (fitOrder == "inverted")
			osc->SetMasses<Oscillator::inverted>(fM12, fM23);
		osc->SetPMNS<Oscillator::sin2>(fS12, fS13, fS23, fdCP);
		fitSpectra = fitter->BuildSamples(osc);
	}

	Eigen::ArrayXd x2n;
	for (const auto &is : trueSpectra) {
		std::string output = outfile;
		output.insert(output.find(".dat"), "_" + is.first);
		std::ofstream fout(output.c_str());
		std::cout << "Comparing " << is.first << " sample and saving to "
			  << output << "\n";

		fout << "# true: m12 " << tM12 << ", m23 " << tM23
		     << ", s12 " << tS12 << ", s13 " << tS13
		     << ", s23 " << tS23 << ", dcp " << tdCP << std::endl;
		if (fitPoint >= 0) {
			x2n = fitter->RawX2n(is.second, fitSpectra[is.first]);
			fout << "#  fit: m12 " << fM12 << ", m23 " << fM23
			     << ", s12 " << fS12 << ", s13 " << fS13
			     << ", s23 " << fS23 << ", dcp " << fdCP << std::endl;
			fout << "# total x2 = " << x2n.sum() << std::endl;
		}

		for (int i = 0; i < is.second.size(); ++i) {
			fout << i << "\t" << is.second(i);
			if (fitPoint >= 0)
				fout << "\t" << fitSpectra[is.first](i)
				     << "\t" << x2n(i) << std::endl;
			else
				fout << "\n";
		}
	}

	delete cd;
	delete osc;
	delete fitter;

	return 0;
}
