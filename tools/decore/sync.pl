#!/usr/bin/env perl
#----------------------------------------------------------------------
# sync.pl - the vendored copy of the server's de-core subset
# (third_party/decore, docs/RESTRUCTURING.md task 4.12)
#----------------------------------------------------------------------
#
#   perl tools/decore/sync.pl <server-root>          resync the copy
#   perl tools/decore/sync.pl --check <server-root>  compare, change nothing
#   perl tools/decore/sync.pl --verify-manifest      offline self-check
#
# The server (bound2/opendarkeden-server) is the one implementation of
# the rules both sides compute. This client carries a byte-identical
# copy of part of its src/domain at third_party/decore/domain/, which
# nobody edits by hand: this script writes it.
#
# The subset is defined by the server, exactly as its
# tests/tools/decore_client_diff.sh defines it, so the two cannot
# disagree: the sources listed in DECORE_VENDORED_SOURCES in
# src/domain/CMakeLists.txt, every header those include in quotes,
# directly or through another header (a bare "X.h" is read as
# domain/X.h, as that script reads it), and every *.tsv file in
# src/domain/vectors/ (only .tsv files are vectors, so an editor backup
# or a Finder .DS_Store on the server side is never vendored).
#
# A sync copies the subset byte for byte, deletes anything under
# domain/ that left it, rewrites third_party/decore/MANIFEST (one
# "<sha256>  domain/<file>" line per file, bytewise sorted, nothing
# else - the server's diff script reads every two-field line as a
# file) and the "Last synced from server commit:" line of
# third_party/decore/README.md. Always pass the server root: no default
# is right for every pair of checkouts. To try an unmerged server edit,
# sync from that working tree and do not commit the result; the README
# line says so when src/domain had uncommitted changes.
#
# --check compares the copy with a server checkout without writing
# anything: the subset against MANIFEST, MANIFEST against the files
# under domain/, and every file byte for byte. It never compares the
# README's commit, because the server's master moves on for changes
# outside the subset. The decore-upstream job in
# .github/workflows/linux.yml runs it against server master.
#
# --verify-manifest needs no server (the decore_vendored ctest): every
# MANIFEST line is well formed, sorted and names a file whose sha256
# matches, nothing under domain/ is unlisted, every listed .cpp is in
# third_party/decore/CMakeLists.txt's explicit source list and the other
# way round, and the README names the commit. A hand edit of the copy
# fails it.
#
# Exit status: 0 clean, 1 on any difference, 2 on a usage error.
#----------------------------------------------------------------------
use strict;
use warnings;
use Digest::SHA;
use File::Basename qw(dirname);
use File::Find qw(find);
use File::Path qw(make_path);

# On Windows perls, the script's own path with forward slashes, which
# they all accept: under Git for Windows' msys perl, dirname() does not
# split on a backslash, so a backslash path to the script would lose its
# directory. Elsewhere a backslash is an ordinary filename character.
my $self = __FILE__;
$self =~ s{\\}{/}g if $^O =~ /^(?:MSWin32|msys|cygwin)$/;
my $client_root = dirname($self) . "/../..";
my $vendor      = "$client_root/third_party/decore";
my $manifest    = "$vendor/MANIFEST";
my $readme      = "$vendor/README.md";
my $cmakelists  = "$vendor/CMakeLists.txt";
my $sha_line    = qr/^Last synced from server commit: (.*)$/m;

sub usage {
	print STDERR "usage: perl tools/decore/sync.pl <server-root>\n"
		. "       perl tools/decore/sync.pl --check <server-root>\n"
		. "       perl tools/decore/sync.pl --verify-manifest\n";
	exit 2;
}

sub read_bytes {
	my ($path) = @_;
	open my $fh, '<:raw', $path or die "$path: $!\n";
	local $/;
	my $bytes = <$fh>;
	close $fh;
	return defined $bytes ? $bytes : '';
}

sub write_bytes {
	my ($path, $bytes) = @_;
	make_path(dirname($path));
	open my $fh, '>:raw', $path or die "$path: $!\n";
	print $fh $bytes;
	close $fh or die "$path: $!\n";
}

sub sha256_of { return Digest::SHA::sha256_hex(read_bytes($_[0])); }

# The server's subset, as paths relative to its src/ (domain/<file>):
# a line-for-line port of the perl block in the server's
# tests/tools/decore_client_diff.sh.
sub server_subset {
	my ($root) = @_;
	my $text = read_bytes("$root/src/domain/CMakeLists.txt");
	$text =~ /set\(DECORE_VENDORED_SOURCES\s+([^)]*)\)/
		or die "no DECORE_VENDORED_SOURCES list in $root/src/domain/CMakeLists.txt\n";
	my @queue = map { "domain/$_" } split " ", $1;
	my %seen;
	while (defined(my $path = shift @queue)) {
		next if $seen{$path}++;
		open my $src, '<', "$root/src/$path" or die "$root/src/$path: $!\n";
		while (<$src>) {
			next unless /^\s*#\s*include\s*"([^"]+)"/;
			my $inc = $1;
			$inc = "domain/$inc" unless $inc =~ m{/};
			push @queue, $inc;
		}
		close $src;
	}
	opendir my $vd, "$root/src/domain/vectors" or die "$root/src/domain/vectors: $!\n";
	# Only .tsv files are vectors, the server's decore_client_diff.sh rule,
	# so a stray editor backup or .DS_Store is never vendored.
	$seen{"domain/vectors/$_"} = 1 for grep { /\.tsv\z/ && -f "$root/src/domain/vectors/$_" } readdir $vd;
	closedir $vd;
	return sort keys %seen;
}

# Every file under the client's domain/, as domain/<file>.
sub client_files {
	my @out;
	return @out unless -d "$vendor/domain";
	find({ no_chdir => 1, wanted => sub {
		return unless -f $_;
		(my $p = $_) =~ s{\\}{/}g;
		$p =~ s{^\Q$vendor\E/}{};
		push @out, $p;
	} }, "$vendor/domain");
	return sort @out;
}

# MANIFEST as a path -> sha256 map, plus the problems found reading it.
sub read_manifest {
	my (%hash, @problems, @order);
	if (!-f $manifest) {
		push @problems, "$manifest is missing";
		return (\%hash, \@problems, \@order);
	}
	my $n = 0;
	for my $line (split /\n/, read_bytes($manifest)) {
		$n++;
		if ($line !~ m{^([0-9a-f]{64})  (domain/[^\s]+)$}) {
			push @problems, "MANIFEST line $n is not \"<sha256>  domain/<file>\": $line";
			next;
		}
		push @problems, "MANIFEST lists $2 twice" if exists $hash{$2};
		$hash{$2} = $1;
		push @order, $2;
	}
	my @sorted = sort @order;
	push @problems, "MANIFEST is not sorted bytewise" if "@order" ne "@sorted";
	push @problems, "MANIFEST lists no file" unless @order;
	return (\%hash, \@problems, \@order);
}

my $failed = 0;

sub report {
	my ($label, @lines) = @_;
	if (@lines) {
		print "[FAIL] $label:\n";
		print "         $_\n" for @lines;
		$failed = 1;
	} else {
		print "[OK]   $label: none\n";
	}
}

sub minus {
	my ($from, $take) = @_;
	my %taken = map { $_ => 1 } @$take;
	return grep { !$taken{$_} } @$from;
}

sub verify_manifest {
	my ($hash, $problems, $order) = read_manifest();
	report("malformed MANIFEST lines", @$problems);

	my @differ;
	for my $path (@$order) {
		my $file = "$vendor/$path";
		if (!-f $file) { push @differ, "$path (missing)"; next; }
		push @differ, $path if sha256_of($file) ne $hash->{$path};
	}
	report("files whose sha256 differs from MANIFEST (edited by hand? resync)", @differ);

	my @files = client_files();
	report("files under domain/ that MANIFEST does not list", minus(\@files, $order));

	# The decore target lists its sources by hand, so a vendored .cpp the
	# list lacks would be copied and never compiled.
	my $text = read_bytes($cmakelists);
	my @built;
	if ($text =~ /add_library\(decore\s+STATIC\s+([^)]*)\)/) {
		@built = sort grep { /\.cpp$/ } split " ", $1;
	} else {
		report("an add_library(decore STATIC ...) list in $cmakelists", "not found");
	}
	my @listed = grep { /\.cpp$/ } @$order;
	report("vendored .cpp files missing from the decore target's source list", minus(\@listed, \@built));
	report("decore target sources that MANIFEST does not list", minus(\@built, \@listed));

	my $readme_text = -f $readme ? read_bytes($readme) : '';
	my @sha;
	push @sha, "no \"Last synced from server commit: <sha>\" line in $readme"
		unless $readme_text =~ $sha_line && $1 =~ /^[0-9a-f]{40}\b/;
	report("README commit line problems", @sha);

	print "MANIFEST: " . scalar(@$order) . " files\n";
}

sub check_server_root {
	my ($root) = @_;
	usage() unless defined $root;
	die "not a server checkout (no src/domain/CMakeLists.txt): $root\n"
		unless -f "$root/src/domain/CMakeLists.txt";
}

sub check_against {
	my ($root) = @_;
	check_server_root($root);
	my @server = server_subset($root);
	my ($hash, undef, $order) = read_manifest();
	my @files = client_files();

	report("vendored on the server, missing from MANIFEST", minus(\@server, $order));
	report("in MANIFEST, not vendored on the server", minus($order, \@server));
	report("under domain/, not vendored on the server", minus(\@files, \@server));

	my @differ;
	for my $path (@server) {
		my $file = "$vendor/$path";
		if (!-f $file) { push @differ, "$path (missing in the client)"; next; }
		push @differ, $path if read_bytes($file) ne read_bytes("$root/src/$path");
	}
	report("files that differ from the server checkout", @differ);
	print "server subset: " . scalar(@server) . " files\n";
}

sub sync_from {
	my ($root) = @_;
	check_server_root($root);
	my @server = server_subset($root);
	die "the server's subset is empty\n" unless @server;

	for my $stale (minus([client_files()], \@server)) {
		unlink "$vendor/$stale" or die "$vendor/$stale: $!\n";
		print "removed $stale\n";
	}
	my $lines = '';
	for my $path (@server) {
		my $bytes = read_bytes("$root/src/$path");
		my $dest = "$vendor/$path";
		if (!-f $dest || read_bytes($dest) ne $bytes) {
			write_bytes($dest, $bytes);
			print "copied $path\n";
		}
		$lines .= Digest::SHA::sha256_hex($bytes) . "  $path\n";
	}
	write_bytes($manifest, $lines);

	my $sha = `git -C "$root" rev-parse HEAD`;
	$sha = defined $sha ? $sha : '';
	chomp $sha;
	die "cannot read the server's commit (is $root a git checkout?)\n" unless $sha =~ /^[0-9a-f]{40}$/;
	my $dirty = `git -C "$root" status --porcelain -- src/domain`;
	$sha .= " (plus uncommitted changes in src/domain: do not commit this sync)"
		if defined $dirty && $dirty =~ /\S/;
	my $text = read_bytes($readme);
	$text =~ s/$sha_line/Last synced from server commit: $sha/
		or die "no \"Last synced from server commit:\" line in $readme\n";
	write_bytes($readme, $text);
	print "synced " . scalar(@server) . " files from $sha\n";
}

my $mode = shift @ARGV;
usage() unless defined $mode;
# A die anywhere below (an unreadable file, a server tree that is not
# one) is a usage error, exit 2, never mistaken for a difference.
eval {
	if ($mode eq '--verify-manifest') {
		usage() if @ARGV;
		verify_manifest();
	} elsif ($mode eq '--check') {
		usage() unless @ARGV == 1;
		check_against($ARGV[0]);
		verify_manifest();
	} elsif ($mode =~ /^-/) {
		usage();
	} else {
		usage() if @ARGV;
		sync_from($mode);
		verify_manifest();
	}
	1;
} or do {
	print STDERR $@;
	exit 2;
};
exit $failed;
