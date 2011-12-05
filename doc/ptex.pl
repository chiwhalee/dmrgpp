#!/usr/bin/perl -w

use warnings;
use strict;

=pod
Usage:
perl ptex.pl < source.ptex > destination.tex
=cut

my $ptexStartKeyword = "!PTEX-START";
my $ptexEndKeyword = "!PTEX-END";
my $ptexThisClassKeyword="!PTEX_THISCLASS";
my $ptexRefKeyword="!PTEX_REF";
my $ptexLabelKeyword="!PTEX_LABEL";

my %GlobalMacros=();
my %GlobalRefs=();
my $GlobalPtexOpen=0;
my $GlobalLabel="";
my $GlobalBuffer="";

loadMacros(0);
loadMacros(1);

printHeader();
replaceMacros();

sub replaceMacros
{
	while(<STDIN>) {
		if (/\\ptexPaste\{([^\}]+)\}/) {
			die "Undefined macro for label $1\n" if (!defined($GlobalMacros{$1}));
		}
		s/\\ptexPaste\{([^\}]+)\}/$GlobalMacros{$1}/;
		if (/\\ptexLabel\{([^\}]+)\}/) {
			die "Undefined ref for label $1\n" if (!defined($GlobalRefs{$1}));
		}
		s/\\ptexLabel\{([^\}]+)\}/$GlobalRefs{$1}/g;
		next if (/\!PTEX\-/);
		print;
	}
}

sub printHeader
{
my $date=`date`;
chomp($date);
print<<EOF;
%DO NOT edit this file
%Changes will be lost next time you run $0
%Created by $0 by G.A.
%$date
EOF
}


sub loadMacros
{
	my ($passNumber)=@_;
	open(PIPE,"find ../src -iname \"*.h\" |") or die "Cannot open pipe: $!\n";
	while(<PIPE>) {
		chomp;
		procThisFile($_,$passNumber);
	}
	close(PIPE);
}

sub procThisFile
{
	my ($file,$passNumber)=@_;
	$GlobalPtexOpen=0;
	$GlobalLabel="";
	$GlobalBuffer="";
	open(FILE,$file) or die "Cannot open file $file: $!\n";
	while(<FILE>) {
		procThisLine($_,$file,$passNumber);
	}
	close(FILE);
}

sub procThisLine
{
	my ($line,$file,$passNumber)=@_;
	my $class = removeTrailingDirs($file);
	
	if ($passNumber==0) {
		if ($line=~/$ptexLabelKeyword\{([^\}]+)\}/) {
			my $tmp = $1;
			$GlobalRefs{$tmp}=removeTrailingDirs("$file:$.");
		}
		return;
	}

	if ($GlobalPtexOpen) {
		if ($line=~/$ptexRefKeyword\{([^\}]+)\}/) {
			$GlobalRefs{"HERE"} = removeTrailingDirs("$file:$.");
			
			die "Undefined reference $1\n" if (!defined($GlobalRefs{$1}));
		}
		$line=~s/$ptexRefKeyword\{([^\}]+)\}/\\verb!$GlobalRefs{$1}!/g;

		$line=~s/$ptexThisClassKeyword/$class/g;
		if ($line=~/$ptexEndKeyword/) {
			$GlobalPtexOpen=0;
			$GlobalMacros{"$GlobalLabel"}=$GlobalBuffer;
# 			print STDERR "Macro found for $GlobalLabel\n";
			$GlobalBuffer="";
			return;
		}
		$GlobalBuffer .= $line;
		return;
	}

	if ($line=~/$ptexStartKeyword +(.*$)/) {
		$GlobalLabel = $1;
# 		print STDERR "GL=$1\n";
		$GlobalLabel=~s/ //g;
		die "Empty level on line $.\n" if ($GlobalLabel eq "");
		$GlobalPtexOpen=1;
		$GlobalBuffer="";
	}
}

sub removeTrailingDirs
{
	my ($x)=@_;
	if ($x=~/\/([^\/]+$)/) {
		$x = $1;
	}
	$x=~s/\.h$//;
	return $x;
}
