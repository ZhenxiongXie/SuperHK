#! /bin/bash

Sens=$PWD/cross-fitter.sh
sub=condor_submit

name=${1%.*}

while read -r point ; do
	# if finished is not shown, means something bad happened
	echo Checking point $point

	outdir=$name'_'$point
	outdir=$(realpath $outdir)

	allog=$(find $outdir -name "*.log")
	allog=(${allog})
	all=${#allog[@]}

	if [ "$all" -eq 0 ]; then
		echo No log files for point $point, job did not even start. Restarting
		if [ -s "$outdir/R"*".$point.sub" ] ; then
			$sub $outdir/R*.$point.sub
		fi
	else
		for log in "${allog[@]}" ; do
			if ! tail $log | grep -q Finished ; then
				#log is #/long/path/to/folder/CPV_scan_point/fitter.proc.log
				proc=${log##*/}
				#proc is fitter.proc.log
				proc=${proc#*.}
				#proc is proc.log
				proc=${proc%.*}
				#proc is proc
				echo process $point : $proc did not complete

				scriptname=$outdir/Recover.$proc.sub
				cat > $scriptname << EOF
#! /bin/bash
# script submission for HTCondor
# sumbit with --
#	$sub $scriptname

executable		= $Sens
arguments		= $proc $all $outdir/this_sensitivity.card
getenv			= True
#requirements		= HasAVXInstructions
should_transfer_files	= IF_NEEDED
when_to_transfer_output	= ON_EXIT
initialdir		= $PWD
output			= $log
error			= $log

queue
EOF
				$sub $scriptname
			fi
		done
	fi
done < $1
