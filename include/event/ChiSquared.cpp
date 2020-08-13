/* SK energy scale smart handling
 *
 * It was decided that bins with zero events should be not inclded in chi^2 calculation
 * especially due to the energy scale problem
 * If events migrate to previously-empty bins, the chi^2 becomes ill-defenied.
 *
 * For this reason, it should be safer (and faster too!!) to only store and handle nonzero bins
 * 1Re-like hists and 1Rmu-like hists have therefore a different binning range
 */

#include "ChiSquared.h"

ChiSquared::ChiSquared(CardDealer *card) :
	cd(card),
	_nBin(-1),
	_nSys(-1)
{
	Init();
}

ChiSquared::ChiSquared(CardDealer *card, const std::vector<Sample*> &sams) :
	cd(card),
	_sample(sams),
	_nBin(-1),
	_nSys(-1)
{
	Init();
}

void ChiSquared::Init()
{
	if (!cd->Get("lm_0", lm_0))
		lm_0 = 1;		//default value
	if (!cd->Get("lm_up", lm_up))
		lm_up = 5;		//default value
	if (!cd->Get("lm_down", lm_down))
		lm_down = 10;		//default value

	if (!cd->Get("max_iterations", maxIteration))
		maxIteration = 10;
	if (!cd->Get("fit_error", fitErr))
		fitErr = 1e-9;


	if (!cd->Get("verbose", kVerbosity))
		kVerbosity = 0;
}

void ChiSquared::AddSample(Sample* sam)
{
	_sample.push_back(sam);
}

void ChiSquared::Combine()
{
	CombineBinning();
	CombineCorrelation();
	CombineSystematics();
}


/*
int ChiSquared::NumSys()
{
	if (_nSys < 0)
		_nSys = std::accumulate(_sample.begin(), _sample.end(), 0,
			[&](double sum, Sample* is)
			{ return sum + is->NumSys(); });

	return _nSys;
}
*/


/*
int ChiSquared::NumBin() {
	// compute the number of nonempty bins
	// if not defined, the routine will do this check
	// and creates lower and upper bin pairs in _limits
	//
	if (_nBin < 0)
		_nBin = std::accumulate(_sample.begin(), _sample.end(), 0,
			[&](double sum, Sample* is)
			{ return sum + is->NumBin(); });

	return _nBin;
}
*/


/*
int ChiSquared::NumScale()
{
	if (_nScale < 0)
		_nScale = std::accumulate(_sample.begin(), _sample.end(), 0,
			[&](double sum, Sample* is)
			{ return sum + is->NumScale(); });

	return _nScale;
}
*/


int ChiSquared::DOF() {
	return std::abs(_nBin - _nSys);
}


// defines nBin too
void ChiSquared::CombineBinning() {
	_nBin = std::accumulate(_sample.begin(), _sample.end(), 0,
		[&](double sum, Sample* is)
		{ return sum + is->_nBin; });
}

// defines _nSys too
void ChiSquared::CombineCorrelation()
{
	_corr = Eigen::MatrixXd::Identity(_nSys, _nSys);
	_nSys = 0;

	for (const auto &is : _sample) {
		int ns = is->_nSys;
		_corr.block(_nSys, _nSys, ns, ns) = is->_corr;
		_nSys += ns;
	}
}

void ChiSquared::CombineSystematics()
{
	zeroEpsilons = true;
	for (const auto &is : _sample) {
		_sysMatrix.insert(is->_sysMatrix.begin(), is->_sysMatrix.end());
		zeroEpsilons = zeroEpsilons && is->zeroEpsilons;
	}


}


//On is the true spectrum, En is the observed spectrum
//return time taken for computation
Eigen::VectorXd ChiSquared::FitX2(const Eigen::VectorXd &On, const Eigen::VectorXd &En)
{
	//initialize epsil with zeroes

	Eigen::VectorXd epsil = Eigen::VectorXd::Zero(_nSys);
	//Eigen::VectorXd epsil = Eigen::VectorXd::Constant(_nSys, 0);
	Eigen::VectorXd best_eps = epsil;
	double best_x2 = X2(On, En, best_eps);

	int tries = 0;
	// if _nSys == 0 there is no fit, good for stats only
	while (!zeroEpsilons > 0 && !FindMinimum(On, En, epsil) && tries < maxIteration) {
		++tries;
		double x2 = X2(On, En, epsil);

		if (kVerbosity > 1)
			std::cout << "~~~~~~ No convergence reached ~~~~~~~";

		double dist = (best_eps - epsil).norm();
		double step = best_x2 - x2;
		if (best_x2 > x2) {
			best_x2 = x2;
			best_eps = epsil;
			std::cout << " but new best!";
		}


		if (kVerbosity > 1) {
			std::cout << " X2 " << best_x2 << std::endl;
			std::cout << "distance from previous best " << dist
				<< " with dx2 " << step << std::endl;
			std::cout << "trying new point " << tries << std::endl;
			std::cout << std::endl;
		}
		epsil.setRandom();
		epsil = best_eps + (std::abs(step) < 1 ? step : 1) * epsil;
	}
	if (kVerbosity > 1)
		std::cout << "Number of attempts: " << tries << std::endl;

	if (zeroEpsilons)	// no need to fit
		return epsil;

	if (tries < maxIteration)
		return epsil;
	else
		return best_eps;
}


//uses Levenberg-Marquardt algorithm
//quites when x2 variation falls below sens and if lambda < 1
//which means it iconvergin
bool ChiSquared::FindMinimum(const Eigen::VectorXd &On,
			     const Eigen::VectorXd &En, 
			     Eigen::VectorXd &epsil)
			     //double alpha)
{
	int c = 0;	//counter
	double lambda = lm_0;

	Eigen::VectorXd delta = Eigen::VectorXd::Ones(_nSys);

	double x2 = X2(On, En, epsil);
	double diff = 1;
	//double minj = 1;
	//while (diff > err && minj > NumSys() * err && minj > 1e-6 &&
	//       lambda < 1e6 && c < maxIteration)
	//while (delta.norm() > NumSys() * err && c < maxIteration)

	if (kVerbosity > 2)
		std::cout << "Minimising fit from x2: " << x2 << std::endl;

	while (std::abs(diff) / DOF() > fitErr
	      && delta.norm() / _nSys > fitErr) {
		++c;	//counter

		// build hessian and gradient/jacobian together
		// to save computation time
		Eigen::VectorXd jac(_nSys);
		Eigen::MatrixXd hes(_nSys, _nSys);
		JacobianHessian(jac, hes, On, En, epsil);

		//std::cout << "jac\n" << jac.transpose() << "\n"
		//	  << "hes\n" << hes.transpose() << std::endl;
		//add diagonal to hesT hes
		double maxd = hes.diagonal().maxCoeff();
		hes.diagonal() += Eigen::VectorXd::Constant(_nSys, maxd * lambda);
		delta = hes.ldlt().solve(jac);

		//cos for uphill moves
		//double cosb = delta.dot(newdt) / (newdt.norm() * delta.norm());

		Eigen::VectorXd nextp = epsil - delta;	//next step
		//check if this step is good
		double x2_ = X2(On, En, nextp);
		diff = x2 - x2_;

		//if (x2_ < x2 || (1-cosb) * x2_ < x2)
		if (x2_ < x2) {	//next x2 is better, update lambda and epsilons
			lambda /= lm_down;
			epsil = nextp;
			x2 = x2_;
		}
		else if (std::abs(delta.norm()) > 0)	//next x2 is worse
			lambda *= lm_up;	//nothing changes but lambda
						//if step is nonnull

		//std::cout << "\ndelta\n" << delta << std::endl;
		//if (false)
		if (kVerbosity > 2) {
			std::cout << c << " -> l " << lambda
				  << ",\tstep: " << std::abs(delta.norm() / _nSys)
				  << ", X2: " << x2
				  << " ( " << std::abs(diff / DOF())
				  << " ) " << std::endl;
		}
	}


	return (lambda <= 1./lm_0);	//convergence was reached
					// else	no convergence, change starting point
}


Eigen::VectorXd ChiSquared::Jacobian(const Eigen::VectorXd &On,
				      const Eigen::VectorXd &En, 
				      const Eigen::VectorXd &epsil)
{
	Eigen::VectorXd jac(_nSys);
	Eigen::MatrixXd hes(_nSys, _nSys);
	JacobianHessian(jac, hes, On, En, epsil);

	return 2 * jac;
}


Eigen::MatrixXd ChiSquared::Hessian(const Eigen::VectorXd &On,
				     const Eigen::VectorXd &En, 
				     const Eigen::VectorXd &epsil)
{
	Eigen::VectorXd jac(_nSys);
	Eigen::MatrixXd hes(_nSys, _nSys);
	JacobianHessian(jac, hes, On, En, epsil);

	return 2 * hes;
}


void ChiSquared::JacobianHessian(Eigen::VectorXd &jac, Eigen::MatrixXd &hes,
				 const Eigen::VectorXd &On,
				 const Eigen::VectorXd &En, 
				 const Eigen::VectorXd &epsil)
{
	//corr is inverse of correlation matrix
	jac = _corr * epsil;	//gradient/jacobian
	hes = _corr;		//hessian
	//hes.setZero();		//hessian

	// event distribution with systematics applied
	const Eigen::ArrayXd Ep = Gamma(En, epsil);
	// matrix contains derivative terms for each systematics and for each bin
	const Eigen::ArrayXXd Fp = one_Fp(epsil);

	// loop over samples
	int err_off = 0;
	for (const auto &is : _sample) {	// beam or atmo
		for (const std::string &it: is->_type) {	// 1Re, 1Rmu, ...
			int t = is->ScaleError(it) + err_off;
			double skerr = t < epsil.size() ? epsil(t) : 0;

			std::vector<std::pair<int, int> > slices = is->AllSlices(it, skerr);
			std::vector<Eigen::ArrayXd> scales = is->AllScale(&Sample::Nor, it, skerr);
			std::vector<Eigen::ArrayXd> jacobs = is->AllScale(&Sample::Jac, it, skerr);
			std::vector<Eigen::ArrayXd> hessis = is->AllScale(&Sample::Hes, it, skerr);

	// looping over bins of given sample and type <is, it>
	/* >>>>>>>>>>>>>>> */
	const auto &binpos = is->_binpos[it];
	for (int n = 0; n < binpos.second - binpos.first; ++n) {

		int m0 = slices[n].first, dm = slices[n].second - m0;
		const Eigen::ArrayXd &ev = Ep.segment(m0, dm);

		if (kVerbosity > 4)
			std::cout << "type " << it << ", bin "
				  << n + binpos.first
				  << " scales are between "
				  << m0 << " and " << m0 + dm << std::endl;

		double on = On(n + binpos.first);
		double en = (scales[n] * ev).sum();

		double one_oe = 1 - on / en;

		double en_jac = (jacobs[n] * ev).sum();

		// jacobian
		if (is->_nScale) {
			jac(t) += one_oe * en_jac;

			// hessian of scale error first
			//std::cout << "large " << on << ", " << en
			//	  << ", " << en_jac << ", " << one_oe
			//	  << ";\n\t" << ev.transpose()
			//	  << ";\n\t" << scales[n].transpose()
			//	  << ";\n\t" << jacobs[n].transpose()
			//	  << ";\n\t" << hessis[n].transpose()
			//	  << " = " << (hessis[n] * ev).sum() << std::endl;
			//std::cout << "->terms " << on * pow(en_jac / en, 2)
			//	  << "\t" << one_oe * (hessis[n] * ev).sum() << std::endl;
			hes(t, t) += on * pow(en_jac / en, 2)
				+ one_oe * (hessis[n] * ev).sum();
		}

		//do the rest, including mixed term with SK syst
		for (int k = 0; k < is->_nSys - is->_nScale; ++k) {
			k += err_off;
			// scaled jacobian in this bin
			const Eigen::ArrayXd &kk = Fp.col(k).segment(m0, dm);

			// ev * kk is the derivative wrt to k-th syst
			double gn_jac = (scales[n] * ev * kk).sum();

			// jacobian
			jac(k) += one_oe * gn_jac;

			//diagonal term
			hes(k, k) += on * pow(gn_jac / en, 2);

			if (is->_nScale)
				//mixed term with SK error
				hes(k, t) += en_jac * gn_jac * on / en / en
					+ one_oe * (jacobs[n] * ev * kk).sum();
					//+ one_oe * (jacobs[n] * kk).sum();

			// mixed terms with non energy scale parameters
			for (int j = k + 1; j < is->_nSys - is->_nScale; ++j) {
				j += err_off;

				if (kVerbosity > 5)
					std::cout << "n " << n << "\tk " << k
						  << "\tj " << j << "\n";
				const Eigen::ArrayXd &jj
					= Fp.col(j).segment(m0, dm);

				hes(k, j) += gn_jac * on / en / en
					  * (scales[n] * ev * jj).sum()
					+ one_oe
					* (scales[n] * ev * jj * kk).sum();
			}
		}
	}
	/* <<<<<<<<<<<<<<<<<<<<<< */

		}
		err_off += is->_nSys;
		//std::cout << "\tend type\n";
	}

	// make matrix symmetric
	hes.triangularView<Eigen::Lower>() = hes.transpose();
}


// Inverted hessian
Eigen::MatrixXd ChiSquared::Covariance(const Eigen::VectorXd &On,
				       const Eigen::VectorXd &En,
				       const Eigen::VectorXd &epsil)
{
	//modified expected events with systematics
	Eigen::MatrixXd hes = Hessian(On, En, epsil);

	return hes.inverse();
}

Eigen::VectorXd ChiSquared::Variance(const Eigen::VectorXd &On,
				     const Eigen::VectorXd &En,
				     const Eigen::VectorXd &epsil)
{
	return Covariance(On, En, epsil).diagonal();
}


double ChiSquared::X2(const Eigen::VectorXd &On, const Eigen::VectorXd &En,
		      const Eigen::VectorXd &epsil)
{
	return ObsX2(On, En, epsil) + SysX2(epsil);
}


// epsil is an array with all sigmas including SK energy scale
// sum up all bin contributions to the chi2
double ChiSquared::ObsX2(const Eigen::VectorXd &On,
			 const Eigen::VectorXd &En,
			 const Eigen::VectorXd &epsil)
{
	// modified expected events with systematics
	return ObsX2n(On, En, epsil).sum();
}


// return statistics X2 as a vector
// so X2 contribution to each bin
Eigen::ArrayXd ChiSquared::ObsX2n(const Eigen::VectorXd &On,
				  const Eigen::VectorXd &En,
				  const Eigen::VectorXd &epsil)
{
	// modified expected events with systematics
	Eigen::ArrayXd gam = Gamma(En, epsil);

	
	Eigen::ArrayXd chi2(_nBin);

	int err_off = 0;
	for (const auto &is : _sample) {	// beam or atmo
		for (const std::string &it: is->_type) {	// 1Re, 1Rmu, ...
			int t = is->ScaleError(it) + err_off;
			double skerr = t < is->_nSys ? epsil(t) : 0;

			std::vector<Eigen::ArrayXd> scales = is->AllScale(&Sample::Nor, it, skerr);
			for (int n = is->_binpos[it].first; n < is->_binpos[it].second; ++n) {
				double en = (scales[n] * gam).sum();
				chi2(n) = 2 * en - 2 * On(n) * (1 + log(en / On(n)));
			}
				//std::cout << "bin " << n << " -> x2 " << chi2(n) << std::endl;
				//double en = Scale(gam, skerr, n, it);
		}
		err_off += is->_nSys;
	}

	return chi2;
}


// return systematic X2
double ChiSquared::SysX2(const Eigen::VectorXd &epsil) {
	return epsil.transpose() * _corr * epsil;
}


// this return the spectrum modified with the systematics
Eigen::ArrayXd ChiSquared::Gamma(const Eigen::VectorXd &En,
				 const Eigen::VectorXd &epsil)
{
	Eigen::ArrayXd gam = En.matrix();

	int err_off = 0;
	for (const auto &is : _sample) {	// beam or atmo
		for (int k = 0; k < is->_nSys - is->_nScale; ++k)
			gam *= one_Fk(epsil(k), k);
		err_off += is->_nSys;
	}

	return gam;
}



// this is (1 + F)
Eigen::ArrayXXd ChiSquared::one_F(const Eigen::VectorXd &epsil)
{
	Eigen::ArrayXXd f(_nBin, _nSys);
	
	for (int k = 0; k < epsil.size(); ++k)
		f.col(k) = one_Fk(epsil(k), k);

	return f;
}

// k-th entry of the previous function
Eigen::ArrayXd ChiSquared::one_Fk(double err, int k)
{
	double dl, du;

	if (err < -1) {
		dl = -3; du = -1;
	}
	else if (err >= -1 && err < 0) {
		dl = -1; du = 0;
	}
	else if (err >= 0 && err < 1) {
		dl = 0; du = 1;
	}
	else if (err >= 1) {
		dl = 1; du = 3;
	}

	const Eigen::ArrayXd &sl = _sysMatrix[dl].col(k);
	Eigen::ArrayXd su = _sysMatrix[du].col(k);

	su = (su - sl) / (du - dl);
	return Eigen::ArrayXd::Ones(_nBin, 1) + su * (err - dl) + sl;
}

// this is Fp / (1 + F) which is derivative wrt epsilon
Eigen::ArrayXXd ChiSquared::one_Fp(const Eigen::VectorXd &epsil)
{
	Eigen::ArrayXXd fp(_nBin, _nSys);
	
	for (int k = 0; k < epsil.size(); ++k)
		fp.col(k) = one_Fpk(epsil(k), k);

	return fp;
}

// k-th entry of the previous function
Eigen::ArrayXd ChiSquared::one_Fpk(double err, int k)
{
	double dl, du;

	if (err < -1) {
		dl = -3; du = -1;
	}
	else if (err >= -1 && err < 0) {
		dl = -1; du = 0;
	}
	else if (err >= 0 && err < 1) {
		dl = 0; du = 1;
	}
	else if (err >= 1) {
		dl = 1; du = 3;
	}

	Eigen::ArrayXd sl = _sysMatrix[dl].col(k);
	Eigen::ArrayXd su = _sysMatrix[du].col(k);

	su = (su - sl) / (du - dl);
	sl = Eigen::ArrayXd::Ones(_nBin) + su * (err - dl) + sl;

	return su / sl;
}

/*
//F has the epsilon in it
double ChiSquared::F(int k, int n, double eij)
{
	double dl, du;

	if (eij < -1) {
		dl = -3; du = -1;
	}
	else if (eij >= -1 && eij < 0) {
		dl = -1; du = 0;
	}
	else if (eij >= 0 && eij < 1) {
		dl = 0; du = 1;
	}
	else if (eij >= 1) {
		dl = 1; du = 3;
	}

	return Fp(k, n, dl, du) * (eij - dl) + sysMatrix[dl](n, k);
}

//Fp does not have the epsilon in it
double ChiSquared::Fp(int k, int n, double eij)
{
	double dl, du;

	if (eij < -1) {
		dl = -3; du = -1;
	}
	else if (eij >= -1 && eij < 0) {
		dl = -1; du = 0;
	}
	else if (eij >= 0 && eij < 1) {
		dl = 0; du = 1;
	}
	else if (eij >= 1) {
		dl = 1; du = 3;
	}

	return Fp(k, n, dl, du);
}

double ChiSquared::Fp(int k, int n, double dl, double du)
{
	return (sysMatrix[du](n, k) - sysMatrix[dl](n, k)) / (du - dl);
}
*/

/*
// this is the derivative of modified spectrum by gamma at given k
Eigen::MatrixXd ChiSquared::GammaJac(const Eigen::VectorXd &En,
				     const Eigen::VectorXd &epsil)
{
	Eigen::MatrixXd gam(_nBin, _nSys);	// should remove _nScale?
	gam.rightCols(_nScale).setZero();
	for (int k = 0; k < _nSys - _nScale; ++k)
		gam.col(k) = GammaJac(En, epsil, k);

	return gam;
}


// derive by k-th systematic
// En is already derived once, so this can be done recursively
Eigen::VectorXd ChiSquared::GammaJac(const Eigen::VectorXd &En,
				     const Eigen::VectorXd &epsil, int k)
{
	Eigen::VectorXd gam = En;
	for (int n = 0; n < En.size(); ++n)
		gam(n) *= Fp(k, n, epsil(k)) / (1 + F(k, n, epsil(k)));

	return gam;
}
*/

// this function return a modifier for the bin relative to jacobians of the gamma function
// it returns a correct factor only if a valid bin is passed,
// otherwise it returns 1 as backward-compatibility
//double ChiSquared::GammaHes(int n, int k, double ek, int j, double ej)
//{
//	if (k < 0)
//		return 1;
//	else if (j < 0)
//		return gp(k, n, ek);
//	else
//		return gp(k, n, ek) * gp(j, n, ej);
//}



/*
// call jacobian of Scale Energy function
double ChiSquared::ScaleJac(const Eigen::VectorXd &En,
			 double skerr,
			 int n, std::string it, int m0)
{
	Eigen::ArrayXd arrEn = En.array();
	return Scale(&ChiSquared::Nor, arrEn, skerr, n, it, m0);
}

double ChiSquared::ScaleJac(const Eigen::VectorXd &En,
			    double skerr,
			    int n, std::string it, int m0)
			    //int k, double ek, int j, double ej)
{
	return Scale(&ChiSquared::Jac, En, skerr, n, it, m0); //, k, ek, j, ej);
}


// call hessian of Scale Energy function
double ChiSquared::ScaleHes(const Eigen::VectorXd &En,
			    double skerr,
			    int n, std::string it, int m0)
			    //int k, double ek, int j, double ej)
{
	return Scale(&ChiSquared::Hes, En, skerr, n, it, m0); //, k, ek, j, ej);
}
*/
