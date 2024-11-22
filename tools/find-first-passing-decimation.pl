#!/usr/bin/perl
#
# find-first-passing-decimation.pl version 0.0.2
# This file is part of the Theseus distribution: https://github.com/KeyPair-Consulting/Theseus
# Copyright 2024 Joshua E. Hill <josh@keypair.us>
#
# Licensed under the 3-clause BSD license. For details, see the LICENSE file.
#
# Author(s)
# Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
#
#Use this as follows:
#find-first-passing-decimation.pl <filename> <decimation bound>
#
#Note that the various Theseus and NIST tools used need to be available in the PATH.
#This uses downsample and ea_iid. Note: v1.1.7 needs to be modified to report all results;
#see https://github.com/usnistgov/SP800-90B_EntropyAssessment/pull/235
#The NIST tool version 1.1.8 released 20240712 (commit 68ed165fd7a3eeef26b87a546ba23f338e82a3f3) 
#includes this update.

use strict; 
use warnings;

#The names of the various SP 800-90B Section 5 tests
my @testList = ("Excursion Test Statistic", "Number of Directional Runs", "Length of Directional Run", "Numbers of Increases and Decreases", "Number of Runs Based on the Median", "Length of Runs Based on Median", "Average Collision Test Statistic", "Maximum Collision Test Statistic", "Periodicity Test Statistic, p=1", "Periodicity Test Statistic, p=2", "Periodicity Test Statistic, p=8", "Periodicity Test Statistic, p=16", "Periodicity Test Statistic, p=32", "Covariance Test Statistic, p=1", "Covariance Test Statistic, p=2", "Covariance Test Statistic, p=8", "Covariance Test Statistic, p=16", "Covariance Test Statistic, p=32", "Compression Test Statistic", "Chi-Square Independence", "Chi-Square Goodness-of-Fit", "Length of the Longest Repeated Substring");
my $testCount = scalar(@testList);

#This calculates the total number of times that each of the SP 800-90B Section 5 tests can fail.
sub permittedFailures {
	my ($inRounds, @bad) = @_;
	die "Extra argument" if @bad;

	#Calculate cutoff
	#First, note that we want an overall significance of alpha across all 22 tests.
	#If we call the per-test false positive rate q, then we want
	# 1 - (1-q)^22 <= alpha, or equivalently q <= 1 - (1-alpha)^(1/22), thus
	# so if alpha is 0.01 then q <= 0.000456729115374976, which is the per test false positive rate.
	#
	#Let X be the count of failures (prob 1/1000); calculate the probabilities using the binomial distribution.
	#want Pr(false positive for a single test) < q
	# We'll denote the binomial coefficient \binom{n}{k} (where this is the same thing as "n choose k")
	# Pr(X > f_c) = 1 - \sum_{i=0}^{f_c} \binom{n}{i} (1/1000)^i (999/1000)^{n-i} < q
	# iff 1-q < \sum_{i=0}^{f_c} \binom{n}{i} (1/1000)^i (999/1000)^{n-i}
	# Note that \binom{n}{i} = \frac{n+1-k}{k} \bimom{n}{k-1} so we can build up all the terms as we go.
	# Let a_k = \binom{n}{k} (1/1000)^k (999/1000)^{n-k}.
	# Note a_{k} = a_{k-1} * (n+1-k)/k * (1/999)
	# We'll calculate the current partial sum, formally s_k = \sum_{i=0}^{k} a_{i}

	#first calculate a_0
	my $ak = 0.999 ** $inRounds;
	my $runningSum = $ak;
	my $allowedFailures = 1;
	my $alpha = 0.01;
	my $q = 1.0 - (1.0-$alpha)**(1.0/$testCount);

	while($runningSum < 1.0 - $q) {
		$ak = $ak * ($inRounds + 1 - $allowedFailures)/($allowedFailures * 999.0);
		$runningSum += $ak;
		$allowedFailures++;
	}

	# We undershot the false positive rate by one round, 
	# so the allowable failure count must be decremented.
	$allowedFailures--;

	#print "Up to $allowedFailures errors are allowed for a per-round p-value of 0.01 (per-test p-value of $q).\n";
	return $allowedFailures;
}

#This determines the total number of blocks of testing that will be used, across all decimation levels.
#This forces a consistent meaning for the testing across all decimation levels.
sub minDecimatedRounds {
	my ($fileName, $decimationBound, @bad) = @_;
	die "Extra argument" if @bad;
	my $fileSize = -s $fileName || die "Can't find file";
	my $minRounds = int($fileSize / 1000000);

	die "More data is required to test with the provided decimation level.\n" if $minRounds < $decimationBound;

	my $curDecimation = 1;
	while($curDecimation <= $decimationBound) {
		my $curFileSize = $fileSize - ($fileSize % ($curDecimation * 1000000));
		my $curMaxRounds = int($curFileSize / 1000000);

		if($minRounds > $curMaxRounds) {
			$minRounds = $curMaxRounds;
		}

		$curDecimation++;
	}

	return $minRounds;
}

#This conducts IID testing for one (possibly decimated) block and reports the failures.
sub conductBlockIIDTesting {
	my ($fileName, $blockNum, @bad) = @_;
	die "Extra argument" if @bad;
	my @nistTestNames = ("excursion","numDirectionalRuns","lenDirectionalRuns","numIncreasesDecreases","numRunsMedian","lenRunsMedian","avgCollision","maxCollision","periodicity(1)","periodicity(2)","periodicity(8)","periodicity(16)","periodicity(32)","covariance(1)","covariance(2)","covariance(8)","covariance(16)","covariance(32)","compression");
	my @failureArray = (-1)x$testCount;

	open(my $results, '-|', "ea_iid -vv -l$blockNum,1000000 $fileName") || die "Can't run ea_iid";

	while (my $curLine = <$results>) {
		my $testIndex = 0;

		while( $testIndex < scalar(@nistTestNames)) {
			my $permTestName = quotemeta($nistTestNames[$testIndex]);
			#print "Looking for $permTestName\n";
			if($curLine =~ /^\s+$permTestName\*\s/) {
				#print $curLine;
				#print "Found test failure for $nistTestNames[$testIndex]\n";
				$failureArray[$testIndex] = 1;
			} elsif($curLine =~ /^\s+$permTestName\s/) {
				#print $curLine;
				#print "Found test pass for $nistTestNames[$testIndex]\n";
				$failureArray[$testIndex] = 0;
			}
			$testIndex++;
		}

		if($curLine =~ /Chi square independence: P-value = ([-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?)$/) {
			my $pvalue = $curLine;
			chomp $pvalue;
			
			$pvalue =~ s/^Chi square independence: P-value = //;
			if($pvalue < 0.001) {
				$failureArray[19] = 1;
			} else {
				$failureArray[19] = 0;
			}
		} elsif($curLine =~ /Chi square goodness of fit: P-value = ([-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?)$/) {
			my $pvalue = $curLine;
			chomp $pvalue;
			$pvalue =~ s/^Chi square goodness of fit: P-value = //;
			if($pvalue < 0.001) {
				$failureArray[20] = 1;
			} else {
				$failureArray[20] = 0;
			}
		} elsif($curLine =~ /Length of longest repeated substring test: (Passed|Failed)$/) {
			my $LRSTestResult = $1;
			if($LRSTestResult eq "Passed") {
				$failureArray[21] = 0;
			} elsif($LRSTestResult eq "Failed") {
				$failureArray[21] = 1;
			}
		}
	}

	close($results);

	my $testIndex = 0;
	while($testIndex < $testCount) {
		if(($failureArray[$testIndex] != 0) && ($failureArray[$testIndex] != 1)) {
			die "Didn't find a result for $testList[$testIndex]";
		}
		$testIndex++;
	}

	return @failureArray;
}

#This decimates the data at the requested level and performs an IID test on the 
#resulting blocks.
#This can short circuit and report a failure in the event that a pass isn't possible.
#This returns an overall assessment, the number of blocks tested, and the number of failures observed.
sub conductDecimatedTesting {
	my ($fileName, $decimationRate, $testRounds, $allowedFailures, @bad) = @_;
	die "Extra argument" if @bad;
	my $decimatedFileName = "$fileName-dec-$decimationRate";
	my @testFailures = (0)x$testCount;
	my $testPassing = 1;

	0 == system("downsample $decimationRate $fileName > $decimatedFileName 2> /dev/null") || die "Can't decimate dataset";
	die "Decimated file is smaller than expected" if (((-s $decimatedFileName)/1000000) < $testRounds);

	my $currentBlock = 0;
	while(($currentBlock < $testRounds) && ($testPassing == 1)) {
		my @roundResults = &conductBlockIIDTesting($decimatedFileName, $currentBlock);
		my $testIndex = 0;
		while($testIndex < $testCount) {
			$testFailures[$testIndex] += $roundResults[$testIndex];
			if($testFailures[$testIndex] > $allowedFailures) {
				$testPassing = 0;
			}
			$testIndex++;
		}
		$currentBlock++;
	}

	unlink($decimatedFileName) || die "Can't delete temporary file.";

	return ($testPassing, $currentBlock, @testFailures);
}

#Output a summary of the testing. This testing may or may not have passed.
sub reportTestResults {
	my ($totalRounds, $expectedRounds, $failureCutoff, @failureResults) = @_;
	my $testIndex = 0;
	my $passString;

	if($totalRounds != $expectedRounds) {
		print "Testing short-circuited after $totalRounds of $expectedRounds data blocks.\n";
		$passString = "Currently Passing";
	} else {
		$passString = "Passed";
	}

	while($testIndex < $testCount) {
		my $curPassCount = $totalRounds - $failureResults[$testIndex];
		my $curResult = ($failureResults[$testIndex] <= $failureCutoff)?$passString:"Failed";
		print "$testList[$testIndex]: Passed $curPassCount / $totalRounds: $curResult\n";
		$testIndex ++;
	}
}

#Output a summary of a failure.
#Only report on the non-passing tests.
sub reportFailingTestResults {
	my ($totalRounds, $expectedRounds, $failureCutoff, @failureResults) = @_;
	my $testIndex = 0;
	my $resultString = "Tests Failed: ";

	while($testIndex < $testCount) {
		if($failureResults[$testIndex] > $failureCutoff) {
			$resultString .= "$testList[$testIndex] ($failureResults[$testIndex] / $totalRounds) ";
		}
		$testIndex ++;
	}

	print "$resultString\n";
}

my ($fileName, $decimationBound, @badArgs) = @ARGV;
die "Extra argument" if @badArgs;

#first, calculate the number of blocks we'll be testing and the number of failures we can tolerate.
my $roundCount = &minDecimatedRounds($fileName, $decimationBound);
my $allowedTestFailures = &permittedFailures($roundCount);

print "Performing test using $roundCount rounds, allowing up to $allowedTestFailures per-test failures.\n";

my $passingIIDDecimation;
my $failingIIDDecimation;
my @lastPassingResults;
my $lastPassingRoundsRun;

#For the binary search, our invariants are that testing with $passingIIDDecimation passes
#and $failingIIDDecimation fails.
#We then conduct a binary search for the first passing value.
print "Verifying invariants:\n";
my ($maxDecimationResult, $testsRun, @curFailures) = &conductDecimatedTesting($fileName, $decimationBound, $roundCount, $allowedTestFailures);
if($maxDecimationResult == 1) {
	print "Invariant passed: Max decimation passes IID testing.\n";
	$passingIIDDecimation = $decimationBound;
	@lastPassingResults = @curFailures;
	$lastPassingRoundsRun = $testsRun;
} else {
	print "Invariant failed: Max decimation fails IID testing. Proceed with the non-decimated data using empirical approach, dividing the statistical assessments by $decimationBound.\n";
	&reportFailingTestResults($testsRun, $roundCount, $allowedTestFailures, @curFailures);
	exit(0);
}

my $minDecimationResult;
($minDecimationResult, $testsRun, @curFailures) = &conductDecimatedTesting($fileName, 1, $roundCount, $allowedTestFailures);
if($minDecimationResult == 0) {
	print "Invariant passed: No decimation Fails IID testing.\n";
	&reportFailingTestResults($testsRun, $roundCount, $allowedTestFailures, @curFailures);
	$failingIIDDecimation = 1;
} else {
	print "Invariant failed: No decimation passes IID testing. No decimation required to test using the essentially IID approach.\n";
	&reportTestResults($testsRun, $roundCount, $allowedTestFailures, @curFailures);
	exit(0);
}

#The invariants are verified. We now perform the binary search.
while($passingIIDDecimation - $failingIIDDecimation > 1) {
	my $nextDecimation = int(($passingIIDDecimation + $failingIIDDecimation)/2);
	die "Integer math produced a smaller than expected result.\n" if $nextDecimation <= $failingIIDDecimation;
	die "Integer math produced a larger than expected result.\n" if $nextDecimation >= $passingIIDDecimation;

	print "Testing Decimation $nextDecimation: ";
	my $curDecimationResult;
	($curDecimationResult, $testsRun, @curFailures) = &conductDecimatedTesting($fileName, $nextDecimation, $roundCount, $allowedTestFailures);
	if($curDecimationResult == 1) {
		print "Passed\n";
		$passingIIDDecimation = $nextDecimation;
		@lastPassingResults = @curFailures;
		$lastPassingRoundsRun = $testsRun;
	} elsif($curDecimationResult == 0) {
		&reportFailingTestResults($testsRun, $roundCount, $allowedTestFailures, @curFailures);
		$failingIIDDecimation = $nextDecimation;
	} else {
		die "Unexpected IID test result";
	}
}

#We now  have found the first passing decimation rate. Report on it.
print "The first passing decimation rate is $passingIIDDecimation. Decimate the data at this rate and proceed using the essentially IID approach.\n";
&reportTestResults($lastPassingRoundsRun, $roundCount, $allowedTestFailures, @lastPassingResults);
