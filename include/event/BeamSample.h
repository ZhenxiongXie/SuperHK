/*
 * This class is meant to build beam observables for fitting
 * It generates them from VALOR inputs (HK MC),
 * building and populating 4 x 1D histograms in E_reco
 *
 * The histograms are created from TH2D objects that relate
 * E_true information (needed for osc. prob.) and E_reco
 * Such histograms are stored as Eigen::MatrixXd objects
 */

#ifndef BeamSample_H
#define BeamSample_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>

#include "tools/CardDealer.h"
#include "physics/Flavours.h"
#include "physics/Oscillator.h"

#include "event/Sample.h"

#include "TObject.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TAxis.h"
#include "TKey.h"
#include "TMatrixT.h"

#include "Eigen/Dense"

class BeamSample : Sample
{
	public:
		BeamSample(CardDealer *card);
		void Init();

		void LoadReconstruction(std::string channel);

		void LoadReconstruction() override;
		void LoadSystematics() override;

		std::map<std::string, Eigen::VectorXd> BuildSamples(Oscillator *osc = 0)
		override;

		std::vector<Eigen::ArrayXd> AllScale(FactorFn factor,
					std::string it, double skerr);
		std::vector<std::pair<int, int> > AllSlices(std::string it, double skerr);

	private:
		std::map<std::string, Eigen::MatrixXd> _reco;
		//std::map<std::string, std::vector<double> > _binX;
		//std::map<std::string, std::vector<double> > _binY;

		std::vector<std::string> _mode;
		std::vector<std::string> _chan;
		std::vector<std::string> _horn;
};

#endif
