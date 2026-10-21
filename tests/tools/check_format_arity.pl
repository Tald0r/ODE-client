#!/usr/bin/env perl
#----------------------------------------------------------------------
# check_format_arity.pl - the check SafeFormat cannot make for itself
#----------------------------------------------------------------------
#
# SafeFormat::Format (basic/SafeFormat.h) makes a data-file format string
# safe: a conversion it cannot satisfy is printed as text instead of
# performed. What it cannot do is tell that the entry MEANS what the call
# site passes. A converted site whose entry carries one %s more than the
# call site hands over is no longer a memory-safety defect - but it now
# puts a literal "%s" in the user interface, which is a visible bug where
# the old code merely printed rubbish.
#
# So every conversion done for docs/reviews/RESTRUCTURING.md task 5.4 is checked
# here, against the built-in English table in Client/MGameStringTable.cpp.
# That table is not a sample: InitGameStringTable() installs it over the
# file data on the English path, so it is exactly what the default build
# formats with.
#
# What fails this check:
#
#   * an entry with MORE conversions than its call site passes arguments.
#     This is the direction that mattered - it is what made sprintf read a
#     stack word as a char* - and it is the direction that now shows a
#     specifier to the player. It is also the only check here that needs
#     no judgement: both numbers are countable from the text.
#   * a conversion whose argument is provably of the other kind: a %s
#     handed a numeric literal or an arithmetic expression, or a %d handed
#     something with the unmistakable shape of a string (.c_str(),
#     GetString(), a quoted literal).
#
# Deliberately NOT a failure: a %s whose argument this cannot classify.
# The first draft failed those, and reported 29 problems of which every
# one was a false positive - weapon_speed_string[i], szString, grade_string[i]
# are all char arrays that no amount of pattern matching on an expression
# can recognise without a type. A check that guesses cannot be a gate; the
# summary says how many positions went unclassified so the number is at
# least visible.
#
# What is reported but does NOT fail: an entry with FEWER conversions than
# the call site passes. A surplus argument is harmless in both the old and
# the new code, and there are real sites where the English entry takes no
# conversion while the localised one it was written against takes a %s -
# dropping the argument would break the localised build to tidy this one.
#
# What this cannot see: LANGUAGE != 3. No String.inf ships in this
# repository, so for a localised build the entries are unknowable here.
# That build is the one the load-time gate protects and the one the
# formatter's run-time refusal matters most in.
#
#----------------------------------------------------------------------

use strict;
use warnings;
use File::Basename;
use File::Spec;
use File::Find;

my $root = File::Spec->rel2abs(
	File::Spec->catdir(dirname(__FILE__), File::Spec->updir, File::Spec->updir));

my $table = File::Spec->catfile($root, 'Client', 'MGameStringTable.cpp');

#----------------------------------------------------------------------
# The built-in table.
#----------------------------------------------------------------------
my %entry;
{
	open my $fh, '<:raw', $table
		or die "check_format_arity: cannot read $table: $!\n";
	local $/;
	my $t = <$fh>;
	close $fh;

	while ($t =~ /g_pGameStringTable->Set\(\s*([A-Za-z_]\w*)\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)\s*;/g)
	{
		$entry{$1} = $2;
	}
}

if (!%entry)
{
	print "FAIL: no built-in string table entries found in Client/MGameStringTable.cpp\n";
	print "      (the parse broke, which would make every check below vacuous)\n";
	exit 1;
}

#----------------------------------------------------------------------
# The conversions in one format string, as a list of kinds.
#----------------------------------------------------------------------
sub conversions
{
	my ($fmt) = @_;
	my @kinds;
	my $i = 0;

	while ($i < length $fmt)
	{
		if (substr($fmt, $i, 1) ne '%') { $i++; next; }
		$i++;
		if ($i < length($fmt) && substr($fmt, $i, 1) eq '%') { $i++; next; }

		$i++ while $i < length($fmt) && substr($fmt, $i, 1) =~ /[-+ 0#]/;
		$i++ while $i < length($fmt) && substr($fmt, $i, 1) =~ /[0-9*]/;

		if ($i < length($fmt) && substr($fmt, $i, 1) eq '.')
		{
			$i++;
			$i++ while $i < length($fmt) && substr($fmt, $i, 1) =~ /[0-9*]/;
		}

		$i++ while $i < length($fmt) && substr($fmt, $i, 1) =~ /[hljztLwI0-9]/;

		last if $i >= length $fmt;

		my $c = substr($fmt, $i, 1);
		$i++;

		if    ($c =~ /[diuoxXc]/)  { push @kinds, 'int' }
		elsif ($c eq 's')          { push @kinds, 'str' }
		elsif ($c =~ /[eEfFgGaA]/) { push @kinds, 'flt' }
		elsif ($c eq 'p')          { push @kinds, 'ptr' }
		else                       { push @kinds, "?$c" }
	}

	return @kinds;
}

#----------------------------------------------------------------------
# Split a call's arguments at top level.
#----------------------------------------------------------------------
sub split_args
{
	my ($s) = @_;

	# Declared one at a time on purpose: a list assignment whose first
	# target is an array gives every scalar after it undef, and the first
	# draft of this file did exactly that - which made every call parse as
	# nothing, so the check ran over 224 sites, verified none of them, and
	# reported OK.
	my @out;
	my $cur   = '';
	my $depth = 0;
	my $instr = 0;
	my $inchr = 0;

	for my $i (0 .. length($s)-1)
	{
		my $c = substr($s, $i, 1);
		my $p = $i ? substr($s, $i-1, 1) : '';

		if ($instr) { $cur .= $c; $instr = 0 if $c eq '"' && $p ne "\\"; next }
		if ($inchr) { $cur .= $c; $inchr = 0 if $c eq "'" && $p ne "\\"; next }

		if ($c eq '"')  { $instr = 1; $cur .= $c; next }
		if ($c eq "'")  { $inchr = 1; $cur .= $c; next }
		if ($c =~ /[([]/) { $depth++; $cur .= $c; next }
		if ($c =~ /[)\]]/) { $depth--; $cur .= $c; next }
		if ($c eq ',' && $depth == 0) { push @out, $cur; $cur = ''; next }

		$cur .= $c;
	}

	push @out, $cur if $cur =~ /\S/;
	return @out;
}

#----------------------------------------------------------------------
# Classify an argument expression, but only when the shape is certain.
# Returns 'str', 'num' or '' for "cannot tell", which is the common case
# and must never be treated as a defect.
#----------------------------------------------------------------------
sub arg_kind
{
	my ($a) = @_;
	$a =~ s/^\s+|\s+$//g;

	return 'str' if $a =~ /\.c_str\s*\(\s*\)$/;
	return 'str' if $a =~ /GetString\s*\(\s*\)$/;
	return 'str' if $a =~ /GetGameString\s*\(/;
	return 'str' if $a =~ /GetHName\s*\(\s*\)$/;
	return 'str' if $a =~ /^"/;

	# A numeric literal, or arithmetic that cannot be pointer arithmetic.
	#
	# '+' and '-' over a bare identifier are excluded on purpose: szName+1
	# is a char*, and an earlier draft that called any operator expression
	# 'num' would have failed such a site against a %s. There is none in
	# the tree today, which is exactly when a rule like that gets written
	# and never tested. '*', '/' and '%' cannot appear in pointer
	# arithmetic, so they stay.
	return 'num' if $a =~ /^-?\d+$/;
	return 'num' if $a =~ /^[A-Za-z_0-9 \t+\-*\/%]+$/ && $a =~ /[*\/%]/;
	return 'num' if $a =~ /^[-+ \t0-9*\/%()]+$/ && $a =~ /\d/;

	return '';
}

#----------------------------------------------------------------------
# Every converted call site.
#----------------------------------------------------------------------
#
# File::Find, not a shell-out to find(1). The first version ran
# `find "$path" -name '*.cpp'` through the shell, which is GNU find under
# bash and C:\Windows\system32\find.exe under PowerShell - where it
# printed "File not found - *.cpp", returned nothing, and this check
# reported OK over zero sites. CLAUDE.md documents ctest as the way to
# run the suite and PowerShell is this machine's primary shell, so that
# was the normal invocation, not an exotic one. tests/arch/
# check_includes.pl already used File::Find; this now does too.
#
my @sources;
for my $dir ('Client', 'VS_UI')
{
	my $path = File::Spec->catdir($root, $dir);

	die "check_format_arity: $path is missing - fix the path list in this script\n"
		unless -d $path;

	find(sub { push @sources, $File::Find::name if /\.cpp$/i }, $path);
}

die "check_format_arity: found no .cpp files under Client or VS_UI - the scan is broken\n"
	unless @sources;

my ($sites, $checked, $unresolved, $failures, $notes, $unclassified)
	= (0, 0, 0, 0, 0, 0);

# Conversion positions seen, and how many of those a kind could be told
# for. Printed, because "0 type failures" over a handful of comparisons
# is a much weaker statement than it sounds.
my ($positions, $compared) = (0, 0);

for my $file (@sources)
{
	open my $fh, '<:raw', $file
		or die "check_format_arity: cannot read $file: $!\n";
	local $/;
	my $text = <$fh>;
	close $fh;

	next unless $text =~ /SafeFormat::Format|AddSafeFormat|FormatChecked/;

	my $rel = $file;
	$rel =~ s/^\Q$root\E[\/\\]?//;
	$rel =~ s/\\/\//g;

	# Both front ends onto the checked formatter, and they do not put the
	# format in the same place: SafeFormat::Format takes a destination
	# first, CMessageArray::AddSafeFormat owns its destination and so
	# takes the format as argument one. Assuming a position is how this
	# audit came to skip a whole family of its own sites once already.
	while ($text =~ /(SafeFormat::Format|AddSafeFormat|FormatChecked)\s*\(/g)
	{
		my $entry = $1;
		my $open  = pos $text;

		# The line this call starts on, and whether it is commented out.
		#
		# String literals are removed from the prefix before looking for
		# a "//", because a URL in an argument would otherwise make the
		# whole call vanish from the audit - silently, taking any defect
		# in it along. Proven by injecting a strcpy of "http://..." above
		# a call with a dropped argument: the failure disappeared and the
		# tool still exited 0.
		my $before = substr($text, 0, $open);
		my $line   = 1 + ($before =~ tr/\n//);
		my ($tail) = ($before =~ /([^\n]*)$/);

		if (defined $tail)
		{
			my $bare = $tail;
			$bare =~ s/"(?:[^"\\]|\\.)*"//g;
			$bare =~ s/'(?:[^'\\]|\\.)*'//g;
			next if $bare =~ m{//};
		}

		my $depth = 1;
		my $i = $open;
		while ($i < length($text) && $depth)
		{
			my $c = substr($text, $i, 1);
			$depth++ if $c eq '(';
			$depth-- if $c eq ')';
			$i++;
		}

		my @a = split_args(substr($text, $open, $i - $open - 1));
		next if @a < ($entry eq 'AddSafeFormat' ? 1 : 2);
		$sites++;

		# Where the format sits. AddSafeFormat takes it first.
		# SafeFormat::Format has two overloads: argument 2 against a real
		# array, argument 3 when the caller states the size. Looking only
		# at one position is the exact blindness that kept ratchet R7 from
		# seeing the offset-append sites, so this finds the format rather
		# than assuming where it is.
		# MString::FormatChecked takes the format first, like
		# AddSafeFormat: it is a method on the destination, so there is
		# no destination argument to precede it.
		my $fmt_at = ($entry eq 'AddSafeFormat'
					|| $entry eq 'FormatChecked') ? 0 : 1;

		$fmt_at = 2 if $fmt_at == 1
					&& @a > 2
					&& $a[1] !~ /GetGameString\s*\(/
					&& $a[1] !~ /^\s*"/;

		# A literal format is not a table site at all - the formatter is
		# used at a few of those purely for the bound.
		if ($a[$fmt_at] =~ /^\s*"/)
		{
			$sites--;
			next;
		}

		my ($id) = ($a[$fmt_at] =~ /GetGameString\s*\(\s*([A-Za-z_]\w*)\s*\)/);

		unless (defined $id && exists $entry{$id})
		{
			# A computed id, or one the English table does not set. Those
			# are the sites GetGameString's range check exists for.
			$unresolved++;
			next;
		}

		$checked++;

		my @conv = conversions($entry{$id});
		my @args = @a[$fmt_at+1 .. $#a];

		if (@conv > @args)
		{
			printf "FAIL %s:%d  %s\n     entry \"%s\" takes %d conversion(s), the call passes %d argument(s)\n",
				$rel, $line, $id, $entry{$id}, scalar(@conv), scalar(@args);
			$failures++;
			next;
		}

		if (@conv < @args)
		{
			printf "note %s:%d  %s: %d surplus argument(s) against the English entry \"%s\"\n",
				$rel, $line, $id, scalar(@args) - scalar(@conv), $entry{$id};
			$notes++;
		}

		for my $j (0 .. $#conv)
		{
			my $wants = $conv[$j] eq 'str' ? 'str'
					  : $conv[$j] eq 'ptr' ? ''
					  :                      'num';

			next unless $wants;

			$positions++;

			my $got = arg_kind($args[$j]);

			unless ($got)
			{
				$unclassified++;
				next;
			}

			$compared++;

			next if $got eq $wants;

			my $shown = $args[$j];
			$shown =~ s/^\s+|\s+$//g;

			printf "FAIL %s:%d  %s\n     entry \"%s\" wants %s for conversion %d, the call passes %s, which is %s\n",
				$rel, $line, $id, $entry{$id},
				($wants eq 'str' ? 'a string' : 'a number'),
				$j+1, $shown,
				($got eq 'str' ? 'a string' : 'arithmetic');
			$failures++;
		}
	}
}

printf "\ncheck_format_arity: %d converted site(s), %d checked against the built-in table, %d with an id it does not set; %d of %d conversion positions had an argument kind that could be told\n",
	$sites, $checked, $unresolved, $compared, $positions;
printf "check_format_arity: %d note(s)\n", $notes;

#----------------------------------------------------------------------
# A check that checked less than it used to must not report OK.
#
# This file has now failed that way three times, which is why the floors
# below are numbers and not a shrug.
#
#   1. A Perl list assignment whose first target was an array left every
#      scalar in split_args undef. No call parsed, all 224 sites fell
#      into the unresolved bucket, and it printed OK.
#   2. It enumerated sources by shelling out to find(1), which under
#      PowerShell is Windows' find.exe. No files, no sites, OK again -
#      and ctest green with a real arity defect in the tree.
#   3. A call was dropped if anything earlier on its line held "//",
#      which a URL in a string literal satisfies.
#
# All three were invisible because nothing pinned the DENOMINATOR. The
# first floor - the site count - was the answer to that, and the review
# round of task 5.4's third slice showed it only half works: renaming
# GetGameString to something else across Client/PacketHandler moved 34
# sites out of "checked" and into "unresolved" while $sites stayed at
# 256, so coverage collapsed and the check still exited 0. Where the
# format sits is decided by spelling (see $fmt_at above), so any future
# call that says `const char* fmt = GetGameString(X); Format(dst, fmt,
# a);` counts toward the site floor and contributes nothing to coverage.
#
# So both numbers are ratcheted. They may rise freely as sites are
# converted; a fall has to be explained by editing these lines in the
# same commit.
#----------------------------------------------------------------------
# 301 = 294 + 7: task 5.4's fifth slice. Three are VS_UI_GameCommon.cpp's
# title arrays, three are MString::FormatChecked in GameUI.cpp, and one is
# AllocAskMessage in VS_UI_ExtraDialog.cpp.
#
# That last one stands in for TWENTY-ONE converted call sites and counts
# as one, because it takes the entry as a parameter - exactly the shape
# the paragraph above predicted would count toward the site floor and
# contribute nothing to coverage. So this number is a floor on what the
# audit can see, and NOT the number of converted call sites, which is
# 315 (321 until 2026-09-17). Do not quote it as one.
#
# $MINIMUM_CHECKED moves by 3 rather than 7: the FormatChecked sites name
# their id through GetGameString and resolve, the other four do not.
#
# 295 and 283 on 2026-09-17: the eleventh clocks slice deleted MTopView's
# quest caption, a block no live event reaches, and six of these with it
# (the hour, minute and second captions, twice); the converted call
# sites are 315.
# 292 and 280 on2026-09-24: R17 cleanup removes the complete obsolete
# UI_NEW_USER_REGISTRATION block. Its three counted GetGameString formats
# were STRING_USER_REGISTER_ID_LENGTH, _PASSWORD_LENGTH and _NAME_LENGTH.
# The scanner still counts block comments; no live call or scan scope changed.
my $MINIMUM_SITES   = 292;
my $MINIMUM_CHECKED = 280;

if ($sites < $MINIMUM_SITES)
{
	printf "FAIL: scanned %d converted site(s), fewer than the %d recorded here.\n", $sites, $MINIMUM_SITES;
	print  "      Either the scan is broken - which is how this check has failed three times\n";
	print  "      before, every time silently - or sites were legitimately removed, in which\n";
	print  "      case lower \$MINIMUM_SITES in this file in the same commit.\n";
	exit 1;
}

if ($checked < $MINIMUM_CHECKED)
{
	printf "FAIL: resolved %d site(s) to a table entry, fewer than the %d recorded here.\n", $checked, $MINIMUM_CHECKED;
	print  "      The site count can hold steady while coverage collapses - a site whose format\n";
	print  "      this cannot resolve still counts as a site. Either the resolution broke, or\n";
	print  "      call sites legitimately moved to a form it cannot read, in which case say so\n";
	print  "      and lower \$MINIMUM_CHECKED in this file in the same commit.\n";
	exit 1;
}

if ($sites && !$checked)
{
	print "FAIL: found converted sites but resolved none of them to a table entry - the parse is broken\n";
	exit 1;
}

if ($failures)
{
	printf "FAILED: %d problem(s)\n", $failures;
	exit 1;
}

print "OK\n";
exit 0;
