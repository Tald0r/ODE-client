#!/usr/bin/perl
#----------------------------------------------------------------------
# count_tick_reads.pl - live GetTickCount()/timeGetTime() calls
#----------------------------------------------------------------------
#
# Counts calls of the two legacy tick functions in code, per file, with
# comments and string literals removed by a character-level scanner
# rather than by regex: a file-wide s{/\*.*?\*/}{} reads the blitters'
# "//*pDest = ..." line comments as block openers and swallows the code
# up to the next "*/", which is how the clocks work's first counts
# undercounted (docs/history/cpp17-cpp20-compatibility-assessment-2026-09-04.md,
# priority 5). Reads the files in binary, so the NUL bytes in
# VS_UI_GameCommon.cpp do not stop it.
#
#   perl tests/tools/count_tick_reads.pl Client VS_UI basic
#
# Prints one line per file with a count, then a total. The definitions
# and redefinitions in basic/Platform.h and basic/MonotonicClock.cpp are
# listed like any other hit; exclude basic/ to count call sites only.
#
#----------------------------------------------------------------------

use strict;
use warnings;
use File::Find;

my @roots = @ARGV ? @ARGV : ('Client', 'VS_UI');
my %counts;
my $total = 0;

sub strip
{
	my ($s) = @_;
	my $out = '';
	my $i = 0;
	my $n = length $s;
	while ($i < $n) {
		my $c = substr($s, $i, 1);
		my $c2 = $i + 1 < $n ? substr($s, $i + 1, 1) : '';
		if ($c eq '/' && $c2 eq '/') {
			# line comment: to end of line
			my $e = index($s, "\n", $i);
			$i = $e < 0 ? $n : $e;
		}
		elsif ($c eq '/' && $c2 eq '*') {
			my $e = index($s, '*/', $i + 2);
			$i = $e < 0 ? $n : $e + 2;
			$out .= ' ';
		}
		elsif ($c eq '"' || $c eq "'") {
			# string or character literal, with escapes
			my $q = $c;
			$i++;
			while ($i < $n) {
				my $d = substr($s, $i, 1);
				if ($d eq '\\') { $i += 2; next; }
				if ($d eq "\n") { last; }	# unterminated: give up at the line end
				$i++;
				last if $d eq $q;
			}
			$out .= ' ';
		}
		else {
			$out .= $c;
			$i++;
		}
	}
	return $out;
}

find({
	no_chdir => 1,
	wanted => sub {
		return unless -f $_ && /\.(?:cpp|h|inl|c)$/;
		(my $f = $_) =~ s{\\}{/}g;
		open my $h, '<:raw', $_ or return;
		local $/;
		my $s = <$h>;
		close $h;
		my $code = strip($s);
		my $k = () = $code =~ /\b(?:GetTickCount|timeGetTime)\s*\(/g;
		return unless $k;
		$counts{$f} = $k;
		$total += $k;
	},
}, @roots);

for my $f (sort { $counts{$b} <=> $counts{$a} || $a cmp $b } keys %counts) {
	printf "%5d  %s\n", $counts{$f}, $f;
}
printf "%5d  total in %d files\n", $total, scalar keys %counts;
