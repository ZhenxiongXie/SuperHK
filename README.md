# Easy Osc3++

The core of this simplified version of osc3++ is given by the ```event/ChiSquared``` class.
The main code in ```app/fitter.cpp``` uses the class to load systematics, build distributions and minimise the chi-squared.
It fits only the beam sample.  The ```app/atmofitter.cpp``` processes the atmospheric sample, but it is under development.
The oscillation space is scanned over and it is defined/managed by the ```physics/ParameterSpace``` class.
Other important classes are ```physics/Oscillation``` to deal with oscillation physics (it relies on Eigen), ```event/Reco``` to build reconstructed events from true energy, and ```tools/CardDealer``` to handle cards/configuration files.

The space is specified by combinations of oscillation parameters defined inside ```cards/fit.card```.
For each point of the oscillation parameter space, a set of histograms is created and oscillated with those oscillation parameters.
The main set available is *Asimov A*, labelled as `asim`.
The true point is defined by the *Point* variable: in the default space, point 49773 is Asimov A.

A set of script is used to facilitate the execution of the code and extraction of results.
In executing the following scripts, the structure of the folder is important.
We will refer to paths where input files to be fitted are created as ```global``` (data only for atmospheric sample, but configuration files for beam sample too), and as ```root``` to where the outputs of the fitted file will be.

## Requirements

ROOT should be installed and properly linked. To test if true, simply run
```
root-config --cflags --glibs
```

You must download and extract the [Eigen 3 library](https://eigen.tuxfamily.org/dox/index.html) and modify the ```EIGENINC``` variable in the ```Makefile``` to point to the Eigen folder.

## Build

Running
```
make
```
will create the executables in the ```bin/``` folder.


## Folder structure

After building the executables, run
```
./setup.sh
```
to build the folder structure required by the framework.
The script also downloads the input files from http://hep.lancs.ac.uk/~tdealtry/oa/.

You can specify a specific path with the ```-p prefix``` option, as specified in the usage. The default value is the folder ```errorstudy/``` in the current working directory 

Let's say the main path for the studies is the ```errorstudy/``` folder.
For the Asimov A points ("asim"), the global inputs and parameter will be downloaded in the folders (e.g. asim)
```
errorstudy/global/asim
errorstudy/global/reconstruction
```
When dealing with atmospheric samples, ```asim``` will contain sub-folders with oscillated files to be fitted, but most importantly ```.info``` files with a list of true points to fit: **point.info** to generate a simple chi-squared profile against one single true point, **scan.info** with all the points for a sensitivity sweep.
The folder ```reconstruction``` keeps configuration files and root files used to build the beam observables. From VALOR.


There is also a folder for each set to be studied, with its own systematic files
```
errorstudy/root/systematics/
```
where root is one of the following
* **0** the nominal systematics model
* **1a** addition of nu_e CC xsec for 0.1GeV < E_true < 0.6GeV
* **1b** addition of nu_e CC xsec for 0.6GeV < E_true < 1.0GeV
* **2a** addition of nubar_e CC xsec for 0.1GeV < E_true < 0.6GeV
* **2b** addition of nubar_e CC xsec for 0.6GeV < E_true < 1.0GeV
* **6a** subtraction of nu 2p2h normalisation
* **7a** subtraction of nubar 2p2h normalisation
* **67** subtraction of nu and nubar 2p2h normalisation
* **8**  increment of nu_e flux uncertainty in FHC mode
* **9**  addition of CC1pi and CC-coh xsec for nu_e and nubar_e
* **10** addition of CC1pi and CC-coh xsec for nu_mu and nubar_mu
* **11a** increment of SK energy scale uncertainty (to 2.9%)
* **11b** decrement of SK energy scale uncertainty (to 1.9%)
* **flux_lukas** flux models additions
* **nuenorm_5_corr** correlated nu_e nubar_e CC cross-section at 5%
* **nuenorm_5_anti** anticorrelated nu_e nubar_e CC cross-section at 5%


The sub-directory ```systematics``` contains spline root files and the correlation matrix.

The idea is that there is a single global folder with common parameters and multiple root folders, one for each systematic set.

### To study another oscillation set

The default oscillation point set (and the only one provided) is Asimov A, asim for short.
If you want to study a different combination of oscillation points you have to make one yourself.
Let's call this new set ```space```.
The following sub-folder must be created
```
mkdir -p errorstudy/global/space
```
in which the files **point.info** and **scan.info** should also be created accordingly.
The first one should contain just a single point of the space for chi-squared profiling, the latter a list of points for a sensitivity scan.
The file ```card/fit.card``` needs to be update too with the details of the new space: the ```parm_``` entries specify the grid of the oscillation parameter space.

This process will be made more automatic in the future.

## Creating the fitting space for atmospheric sample

**WARNING** *it uses ```GlobalOsc``` part of the original Osc3++ package. Skip if not interested in atmospheric stuff or do not have GlobalOsc installed.*

The first step is to create the fitting space, by running ```GlobalOsc```.
This can be done with the following script:
```
./global.sh [-i | -n] -f nFiles -g /path/to/global
```
where ```-i``` or ```-n``` select inverted or normal hierarchy for the oscillation.

If using batch jobs, the following script works with HTCondor.
```
./global_c.sh [-i | -n] -f nFiles -g /path/to/global
```


## Prepare the systematics

If you built and donwloaded the files using ```setup.sh``` then this step in unnecessary.

Creates correlation matrix of systematic parameters by combining matrices found in files of matrixN.root.
The script expects to find the systematics folder under root with the spline files to be processed (renaming of files and histograms).
It uses ```app/purifysystematics.cpp``` and ```app/addmatrix.cpp```.
```
./prepare_systematics.sh -r errorstudy/root matrix1.root [matrix2.root ...]
```

# Run the fit

The syntax is
```
./bin/fitter id all $output/this_sensitivity.card$1 $2 $3
```
The fitter is meant to be used with some parallel computing, like a batch system.
For this reason, the first two inputs are telling the process which set of files (or points) to study ```id``` and how many processes are running ```all```.
This maximises the effort from the different CPU running. 
The last parameter is the configuration file which specifies oscillation, samples, fitter, ecc.

The easiest way to run the fit is using the following script, which handles parallel computing, folder structure, and configuration files.
It work with HTCondor, but can be easily adapted to any other manager.
```
./trisens_c.sh -g errorstudy/global -r errorstudy/root/asim -1 [NH | IH] -2 [NH | IH] [-f] [-s]
```
where options 1 and 2 specify the mass hierarchies of respectively the observed and expected event samples.
The script uses the point specified in ```global/asim/point.info``` for the fit.
The option -s does a stats only fit (no systematics). The option -f performs the scan of multiple true points, using the ones specified in ```global/asim/scan.info```


The study multiple sets at the same time, the scheduler ```launch_trisens_c.sh``` working with HTCondor was devised.
Manually modify the model array and the point variable.

Assuming for example, a fit on the Asimov A point 49773 with normal hierarchy for both the observed and expected samples,
the fit result will be organised under the following folder
```
errorstudy/asim/NH_NH/sensitivity/point_49773/
```
The folder will contain also log files of the fitter, the card used, and the batch jobs scripts, besides the output root files.
There will be a root file per job, each containing a tree with the computed chi-squared.
The default output name will be *SpaghettiSens.T2HK.XXX.root*, where XXX is a serial file number.



## Extract results

Once the fit is finished, chi-square penalty terms can be optionally added and the profiles created.
In the case of a scanning fit, the CP exclusion is also computed.

To penalise for instance the result of an asim NH vs NH fit, run the script
```
./penalise.sh -r errorstudy/root/asim/NH_NH/
```
The options are specified in the cards/penalty_asim.card.  The executable ```app/addpenalty.cpp``` is used.
The penalised files are stored under 
```
errorstudy/asim/NH_NH/sensitivity/point_49773/
```
and they are just the same as the fitter output files, but with a different name: *SpaghettiSens_penalised*.


To build the contour for point 49773 of an asim NH vs NH fit, run the script
```
./contours.sh -p -r errorstudy/root/asim/NH_NH point_49773
```
the name ```point_49773``` is the name of a folder (automatically generated), it is not a string.
The -p option specifies to use penalised files.
The contours are saved under
```
errorstudy/asim/NH_NH/contours/point_49773/
```

To create the sensitivity plot
```
./excludeall.sh -p -r errorstudy/root/asim/NH_NH
```
The exclusion file is saved under
```
errorstudy/asim/NH_NH/exclusions/
```

These operations could be done more easily and on multiple sets with the scheduler ```launch_contours_c.sh```.
It uses HTCondor and it works similarly to the fitter scheduler.

## Producing plots

In order to produce plots with gnuplot, the following script should be called first
```
./dropchi2.sh -p point_49773 -n name_plots -r errorstudy/ root0/asim/NH_NH/ [root1/asim/NH_NH/  ... ]
```
This will create text file with all the data necessary to make chi-squared profiles, contours, and sensitivity plots.
The -p option specifies which fitted point to use, the -n option is an output name and a folder with the same name will be created to collect all the files.
The -r option specifies the root path, after which a list of root folders can be specified: the created text file will contain also relative data with respect to the first root folder. This is useful when comparing different systematic sets.

Run 
```
./create_plots.sh name_plots
```
to execute gnuplot scripts.
By default pdf plots are generated with the text exported to latex files.
