#!/usr/bin/env perl
#----------------------------------------------------------------------
# check_includes.pl - include-graph architecture rules
# (docs/RESTRUCTURING.md, tasks 0.3 and 2.4)
#----------------------------------------------------------------------
#
# Walks the quote-include closure of every packetwire member and fails
# on forbidden edges. Perl, not python: the dev machines have Git for
# Windows' perl, and the wire-inventory generator is already perl.
#
# Membership is tests/arch/packetwire_files.txt - the same file the
# CMake target and the ratchet script read, so the three cannot
# disagree about what "the library" is.
#
# Rules:
#   W0  Every .cpp under Client/Packet is listed in exactly one of
#       tests/arch/packetwire_files.txt and
#       tests/arch/packetwire_holdouts.txt, and every listed file
#       exists. The directory is wire-only by construction: a new
#       packet source is a library member unless a holdout line says
#       why not, and a deleted one cannot linger in a list.
#   W1  A file in the packetwire closure may include only files under
#       Client/Packet/ or basic/, or Client/Client_PCH.h (the shared
#       precompiled header - its own includes are walked too). Anything
#       else means game code is reachable from the wire layer. Angle
#       includes are checked too: the library's include path carries
#       Client/, so `#include <MZone.h>` would compile - an angle include
#       that resolves inside the repository is treated exactly like a
#       quoted one; only one that resolves nowhere in the tree is a
#       system header and ignored.
#   W2  The debug facilities whose definitions live in the executable
#       or VS_UI (MinTr.h, DebugInfo.h, DebugKit.h) may not be included
#       anywhere in the closure, so the library stays linkable into a
#       test binary without game stubs.
#   DC1 The vendored de-core copy (third_party/decore/domain, the
#       server's rules - third_party/decore/README.md) quote-includes
#       only "domain/X.h" headers that exist in the copy, and
#       angle-includes only <algorithm> and <cmath>: the server's D1,
#       held on this side too, so the copy stays freestanding and every
#       client toolchain can build it. Every line is checked, dead
#       branches included. gamemodel may include its headers (M1).
#
# W1/W2 look at the lines the library's compile actually sees. The
# packet sources were the once-shared client/server copies and carried
# the server halves behind #ifdef __GAME_SERVER__ /
# #ifndef __GAME_CLIENT__, halves that included server headers which
# do not exist in this repo; task 5.2's ninth slice (2026-09-13)
# evaluated them out of Client/Packet, the tenth evaluated the client
# halves out of the rest of the tree and retired __GAME_CLIENT__ from
# the build, and ratchet R15 holds the sources at zero live tokens of
# all five macros and of the four spellings the eleventh slice added to
# the count (three taken out there, __GUILD_MANAGER_TOOL__ with the
# tenth). The walk still evaluates conditionals on them, all
# as undefined now, which is what the build makes them - so a half
# behind one of them that came back through a copy from the server
# repo is skipped here and reported by R15 rather than as a live
# include; a half behind any other macro is unknown here, and BOTH
# branches are checked, which is what catches it. Nothing else is
# evaluated - an include that is dead only under some other macro is
# still a violation.
#
# An include that cannot be resolved against the search path at all is
# reported as a violation too - a silent skip is how a rule rots.
#
# Grandfathered violations, if ever needed, live one per line in
# tests/arch/baseline.txt as "RULE|includer|include" - the list is
# frozen shrink-only: an entry that stops matching fails the run until
# it is deleted. It is currently empty by design.
#
#----------------------------------------------------------------------
use strict;
use warnings;
use File::Basename qw(dirname);
use File::Find qw(find);

my $root = dirname(__FILE__) . "/../..";
chdir $root or die "cannot chdir to repo root: $!";

my $members_file  = 'tests/arch/packetwire_files.txt';
my $holdouts_file = 'tests/arch/packetwire_holdouts.txt';
my $gamemodel_file = 'tests/arch/gamemodel_files.txt';

my @violations;

# A path line must start in column 1: CMake's file(STRINGS ... REGEX
# "^Client/Packet/") anchors there, so an indented line would be a member
# for this checker and the ratchet script but not for the build - the
# three-readers-one-file guarantee is only as good as this agreement.
sub read_list {
	my ($path) = @_;
	my @out;
	open my $fh, '<', $path or die "$path: $!";
	while (<$fh>) {
		s/\r?\n$//;	# belt and braces beside the .gitattributes eol=lf
		s/#.*//;
		next unless /\S/;
		die "$path line $.: a path must start in column 1 (CMake would not read it)\n"
			if /^\s/;
		s/\s+$//;
		push @out, $_;
	}
	close $fh;
	return @out;
}

my @members  = read_list($members_file);
my @holdouts = read_list($holdouts_file);
# 524 today; a truncated list must not pass as "the library".
die "$members_file lists only " . scalar(@members) . " files - truncated?" unless @members >= 400;

#----------------------------------------------------------------------
# W0 - membership completeness
#----------------------------------------------------------------------
# Keys are lower-cased: the Windows filesystem is case-insensitive, so
# "Dup.cpp" and "DUP.cpp" are one file to -f and to the compiler, and
# must be one file to "listed exactly once" as well.
{
	my %listed;
	for my $f (@members, @holdouts) {
		push @violations, "W0|$f|listed twice" if $listed{lc $f}++;
		push @violations, "W0|$f|listed but missing" unless -f $f;
		push @violations, "W0|$f|listed but not under Client/Packet"
			unless $f =~ m{^Client/Packet/};
	}
	my @unlisted;
	find({ no_chdir => 1, wanted => sub {
		return unless -f $_ && /\.cpp$/i;
		(my $p = $_) =~ s{\\}{/}g;
		push @unlisted, $p unless $listed{lc $p};
	} }, 'Client/Packet');
	push @violations, "W0|$_|not in $members_file or $holdouts_file" for sort @unlisted;
}

#----------------------------------------------------------------------
# gamemodel (docs/RESTRUCTURING.md task 4.1) - the same discipline for
# the second library. Its membership file lists .cpp members (what
# CMake compiles) AND the .h files the closure may include, because
# Client/ is not a library-only directory the way Client/Packet is.
#
#   M0  Every listed file exists, lives under Client/, and is listed
#       once; the list is not truncated.
#   M1  A file in the gamemodel closure may include only files under
#       basic/, Client/framelib/ or Client/Packet/, Client/Client_PCH.h,
#       a de-core header (third_party/decore/domain/*.h, held to DC1),
#       or a file listed in the membership file. Anything else - an
#       SDL, UI, dxlib or unlisted game header - is a reach out of the
#       model.
#   M2  As W2: no MinTr.h / DebugInfo.h / DebugKit.h.
#----------------------------------------------------------------------
my @gm_listed = read_list($gamemodel_file);
die "$gamemodel_file lists only " . scalar(@gm_listed) . " files - truncated?" unless @gm_listed >= 10;
my %gm_listed;
my @gm_members;
{
	for my $f (@gm_listed) {
		push @violations, "M0|$f|listed twice" if $gm_listed{lc $f}++;
		push @violations, "M0|$f|listed but missing" unless -f $f;
		push @violations, "M0|$f|listed but not under Client/" unless $f =~ m{^Client/};
		push @gm_members, $f if $f =~ /\.cpp$/i;
	}
}

#----------------------------------------------------------------------
# Include resolution: the file's own directory first (how the compiler
# treats quote-includes), then the include path the library compiles
# with, IN THE COMPILER'S ORDER - target_include_directories(packetwire)
# in CMakeLists.txt lists the repository root, basic, Client, then
# Client/Packet. The order matters for a basename present in two of
# them: mirroring the compiler makes the checker resolve to the same
# file the build does (and a Client/ shadow of a Client/Packet header
# is then a W1 violation, not a silent pass). Keep this list in step
# with CMakeLists.txt.
#----------------------------------------------------------------------
# Client/framelib is gamemodel's extra include dir; it comes last so
# packetwire's resolution order is unchanged. third_party/decore is the
# include dir gamemodel gets from linking decore ("domain/X.h"); it comes
# after that, so nothing that resolved before resolves anywhere else now.
my @searchdirs = ('.', 'basic', 'Client', 'Client/Packet', 'Client/framelib', 'third_party/decore');

sub normalize {
	my ($p) = @_;
	$p =~ s{\\}{/}g;
	my @out;
	for my $part (split m{/}, $p) {
		next if $part eq '.' or $part eq '';
		if ($part eq '..') { pop @out; next; }
		push @out, $part;
	}
	return join('/', @out);
}

sub resolve_include {
	my ($includer, $inc, $angle) = @_;
	# <...> skips the includer's own directory, as the compiler does.
	for my $dir (($angle ? () : dirname($includer)), @searchdirs) {
		my $cand = normalize("$dir/$inc");
		return $cand if -f $cand;
	}
	return undef;
}

#----------------------------------------------------------------------
# Preprocessor conditionals on the macros with one build-wide meaning:
# none of the five is defined anywhere since the tenth slice, and the
# four spellings the eleventh slice added to R15 never were, so each
# is 0 here. Three-valued: 1 = live, 0 = dead, undef = unknown (both
# branches checked). Expressions are the forms this tree uses -
# defined(X), !defined(X), bare X, !, &&, || and parentheses; anything
# else is unknown.
#----------------------------------------------------------------------
my %defined = (
	'__GAME_CLIENT__'   => 0,
	'__GAME_SERVER__'   => 0,
	'__LOGIN_SERVER__'  => 0,
	'__SHARED_SERVER__' => 0,
	'__UPDATE_SERVER__' => 0,
	'__UPDATE_CLIENT__' => 0,
	'__EXPO_CLIENT__'   => 0,
	'__FULLSCREEN_MODE__' => 0,
	'__GUILD_MANAGER_TOOL__' => 0,
);

sub tv_not { my ($v) = @_; return defined $v ? ($v ? 0 : 1) : undef; }
sub tv_and {
	my ($a, $b) = @_;
	return 0 if (defined $a && !$a) || (defined $b && !$b);
	return 1 if defined $a && defined $b;
	return undef;
}
sub tv_or {
	my ($a, $b) = @_;
	return 1 if (defined $a && $a) || (defined $b && $b);
	return 0 if defined $a && defined $b;
	return undef;
}

sub eval_expr {
	my ($expr) = @_;
	$expr =~ s{/\*.*?\*/}{}g;
	$expr =~ s{//.*}{};
	my @tok = $expr =~ /(\|\||&&|!|\(|\)|defined|[A-Za-z_]\w*|\d+|\S)/g;
	my $i = 0;
	my ($or, $and, $unary, $primary);
	$primary = sub {
		my $t = $tok[$i++];
		return undef unless defined $t;
		if ($t eq '(') { my $v = $or->(); $i++ if defined $tok[$i] && $tok[$i] eq ')'; return $v; }
		if ($t eq 'defined') {
			my $paren = defined $tok[$i] && $tok[$i] eq '(';
			$i++ if $paren;
			my $name = $tok[$i++];
			$i++ if $paren && defined $tok[$i] && $tok[$i] eq ')';
			return defined $name && exists $defined{$name} ? $defined{$name} : undef;
		}
		if ($t =~ /^\d+$/) { return $t ? 1 : 0; }
		if ($t =~ /^[A-Za-z_]\w*$/) { return exists $defined{$t} ? $defined{$t} : undef; }
		return undef;
	};
	$unary = sub {
		if (defined $tok[$i] && $tok[$i] eq '!') { $i++; return tv_not($unary->()); }
		return $primary->();
	};
	$and = sub {
		my $v = $unary->();
		while (defined $tok[$i] && $tok[$i] eq '&&') { $i++; $v = tv_and($v, $unary->()); }
		return $v;
	};
	$or = sub {
		my $v = $and->();
		while (defined $tok[$i] && $tok[$i] eq '||') { $i++; $v = tv_or($v, $and->()); }
		return $v;
	};
	my $v = $or->();
	return undef if $i < @tok;	# trailing junk: do not pretend to understand it
	return $v;
}

sub directive_value {
	my ($kw, $rest) = @_;
	return eval_expr("defined($rest)")  if $kw eq 'ifdef';
	return eval_expr("!defined($rest)") if $kw eq 'ifndef';
	return eval_expr($rest);
}

#----------------------------------------------------------------------
# Baseline (frozen, shrink-only)
#----------------------------------------------------------------------
my %baseline;
if (open my $fh, '<', 'tests/arch/baseline.txt') {
	while (<$fh>) {
		s/\r?\n$//;
		next if /^\s*(#|$)/;
		$baseline{$_} = 0;	# 0 = not yet seen this run
	}
	close $fh;
}

#----------------------------------------------------------------------
# Closure walk
#----------------------------------------------------------------------
my %banned = map { $_ => 1 } qw(MinTr.h DebugInfo.h DebugKit.h);

sub violate {
	my ($key) = @_;
	if (exists $baseline{$key}) {
		$baseline{$key} = 1;
		return;
	}
	push @violations, $key;
}

# Walk one library's closure. $rule is the rule letter ('W' or 'M');
# $allowed->($resolved) says whether a resolved include may be walked
# into (anything else is a <rule>1 violation). Returns the number of
# files walked.
sub walk_closure {
	my ($members, $rule, $allowed) = @_;
	my %seen;
	my @queue = @$members;

while (@queue) {
	my $file = shift @queue;
	next if $seen{$file}++;

	open my $fh, '<', $file or next;
	# Each frame: [value of this branch, has an earlier branch in the
	# group been definitely live]. A line is live when no enclosing
	# frame is definitely dead.
	my @stack;
	while (<$fh>) {
		s/\r?\n$//;
		if (/^\s*#\s*(ifdef|ifndef|if)\b\s*(.*)$/) {
			my $v = directive_value($1, $2);
			push @stack, [ $v, (defined $v && $v) ? 1 : 0 ];
			next;
		}
		if (/^\s*#\s*elif\b\s*(.*)$/) {
			next unless @stack;
			my $v = $stack[-1][1] ? 0 : eval_expr($1);
			$stack[-1][0] = $v;
			$stack[-1][1] ||= (defined $v && $v) ? 1 : 0;
			next;
		}
		if (/^\s*#\s*else\b/) {
			next unless @stack;
			$stack[-1][0] = $stack[-1][1] ? 0 : tv_not($stack[-1][0]);
			next;
		}
		if (/^\s*#\s*endif\b/) { pop @stack; next; }

		my ($inc, $angle);
		if (/^\s*#\s*include\s*"([^"]+)"/) { ($inc, $angle) = ($1, 0); }
		elsif (/^\s*#\s*include\s*<([^>]+)>/) { ($inc, $angle) = ($1, 1); }
		else { next; }
		next if grep { defined $_->[0] && !$_->[0] } @stack;	# dead branch

		my ($base) = $inc =~ m{([^/\\]+)$};
		if ($banned{$base}) {
			violate("${rule}2|$file|$inc");
			next;
		}

		my $resolved = resolve_include($file, $inc, $angle);
		if (!defined $resolved) {
			# An angle include nothing in the tree satisfies is a system
			# header; a quoted one is a broken edge either way.
			next if $angle;
			violate("${rule}1|$file|$inc (unresolved)");
			next;
		}

		if ($allowed->($resolved)) {
			push @queue, $resolved;
		} else {
			violate("${rule}1|$file|$inc -> $resolved");
		}
	}
	close $fh;
}
	return scalar keys %seen;
}

my $count = walk_closure(\@members, 'W', sub {
	my ($r) = @_;
	return $r =~ m{^Client/Packet/} || $r =~ m{^basic/} || $r eq 'Client/Client_PCH.h';
});
my $gm_count = walk_closure(\@gm_members, 'M', sub {
	my ($r) = @_;
	return $r =~ m{^basic/} || $r =~ m{^Client/framelib/} || $r =~ m{^Client/Packet/}
		|| $r eq 'Client/Client_PCH.h' || $gm_listed{lc $r}
		|| $r =~ m{^third_party/decore/domain/[^/]+\.h$};
});

#----------------------------------------------------------------------
# DC1 - the vendored de-core copy. Every .h and .cpp under
# third_party/decore/domain is walked, which covers the closure: a quote
# include may only name another file there.
#----------------------------------------------------------------------
my $decore_dir = 'third_party/decore';
my @decore_files;
find({ no_chdir => 1, wanted => sub {
	return unless -f $_ && /\.(?:h|cpp)$/;
	(my $p = $_) =~ s{\\}{/}g;
	push @decore_files, $p;
} }, "$decore_dir/domain");
@decore_files = sort @decore_files;
# A missing copy must not pass as a clean one: the .cpp files found are
# exactly the ones MANIFEST lists (sync.pl writes both, so a resync that
# adds or drops a file keeps them equal).
my @decore_cpp = map { m{^\Q$decore_dir\E/(.*)$} ? $1 : $_ } grep { /\.cpp$/ } @decore_files;
my @manifest_cpp;
open my $manifest, '<', "$decore_dir/MANIFEST" or die "$decore_dir/MANIFEST: $!";
while (<$manifest>) {
	s/\r?\n$//;
	push @manifest_cpp, $1 if m{^[0-9a-f]{64}\s+(domain/[^/]+\.cpp)$};
}
close $manifest;
@manifest_cpp = sort @manifest_cpp;
die "$decore_dir/MANIFEST lists no .cpp file - is the copy intact?" unless @manifest_cpp;
die "$decore_dir/domain holds .cpp files (@decore_cpp) other than MANIFEST's (@manifest_cpp) - is the copy intact?"
	unless "@decore_cpp" eq "@manifest_cpp";
my %decore_angle = map { $_ => 1 } qw(algorithm cmath);
for my $file (@decore_files) {
	open my $fh, '<', $file or die "$file: $!";
	while (<$fh>) {
		s/\r?\n$//;
		if (/^\s*#\s*include\s*"([^"]+)"/) {
			my $inc = $1;
			if ($inc !~ m{^domain/[^/\\]+\.h$}) {
				violate("DC1|$file|$inc (not spelled \"domain/X.h\")");
			} elsif (!-f "$decore_dir/$inc") {
				violate("DC1|$file|$inc (not in the copy)");
			}
		} elsif (/^\s*#\s*include\s*<([^>]+)>/) {
			violate("DC1|$file|<$1> (only <algorithm> and <cmath>)") unless $decore_angle{$1};
		} elsif (/^\s*#\s*include\b/) {
			violate("DC1|$file|$_ (an include this rule cannot read)");
		}
	}
	close $fh;
}

#----------------------------------------------------------------------
# Report
#----------------------------------------------------------------------
my $fail = 0;

for my $v (@violations) {
	print "VIOLATION $v\n";
	$fail = 1;
}
for my $b (sort keys %baseline) {
	next if $baseline{$b};
	print "STALE BASELINE $b - no longer a violation; delete the line (shrink-only)\n";
	$fail = 1;
}

my $nmembers = scalar @members;
my $gm_nmembers = scalar @gm_members;
if ($fail) {
	print "arch_includes: FAILED (packetwire $nmembers members, $count files walked; gamemodel $gm_nmembers members, $gm_count files walked; de-core " . scalar(@decore_files) . " files)\n";
	exit 1;
}
print "arch_includes: OK (packetwire $nmembers members, $count files walked, W0/W1/W2 clean; gamemodel $gm_nmembers members, $gm_count files walked, M0/M1/M2 clean; de-core " . scalar(@decore_files) . " files, DC1 clean)\n";
exit 0;
