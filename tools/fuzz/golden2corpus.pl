#!/usr/bin/perl
#----------------------------------------------------------------------
# golden2corpus.pl - build a stream fuzz target's seed corpus from the
# packet goldens and the wire-layout inventory
#----------------------------------------------------------------------
#
#   perl tools/fuzz/golden2corpus.pl <golden-dir> <wire-layout.txt> \
#       <out-dir> <name-regex>
#
# Every seed is one input for a stream target (tests/fuzz/
# client_stream_reader.h): the encrypt code byte, then the stream.
#
#   golden-<base>   one per tests/golden/<Name>[.<variant>].code<N>.hex
#                   whose packet name matches <name-regex>: chr(N), a
#                   7-byte header (id u16, body size u32, sequence 0,
#                   little-endian) with the id looked up in the layout
#                   file, then the golden body. A .framed. golden is
#                   already a whole frame and gets only the code byte.
#                   .datagram. goldens are skipped: they seed a
#                   datagram target, not a stream one.
#   id-<Name>-<len> two per packet the layout file lists under a
#                   matching name: an empty body and a zero-filled one
#                   of min(max body size, 64) bytes (only the first
#                   when the maximum is 0), at code 0, so every
#                   registered parser is reached even without a golden.
#
# <out-dir> is created if missing, and the golden-* and id-* files
# already in it are deleted first, so a removed golden leaves no stale
# seed behind. Prints the number of seeds written.
#
# Exit status: non-zero when an argument is missing, a file cannot be
# read or written, or a matching golden names a packet the layout file
# does not list.
#
#   perl tools/fuzz/golden2corpus.pl --datagram <golden-dir> <out-dir> \
#       <name-regex>
#
# The datagram mode builds the seeds of the datagram target (tests/fuzz/
# fuzz_client_datagram.cpp), whose input is one raw UDP datagram: no
# encrypt code byte, and no layout file, since the Rpackets the client
# receives over UDP are kept out of wire-layout.txt.
#
#   golden-<base>   one per tests/golden/<Name>.datagram.code<N>.hex
#                   whose packet name matches <name-regex>: the golden
#                   (id u16, body size u32, body, as Datagram::write
#                   puts them down) and the one-byte zero pad behind the
#                   body, which both peers count in the datagram's
#                   length and the golden leaves out.
#   id-<Name>-<len> two per packet those goldens name, with the id read
#                   from the golden itself: an empty body and a
#                   zero-filled one of 16 bytes, each with its pad.
#----------------------------------------------------------------------
use strict;
use warnings;
use File::Path qw(make_path);

my $datagramMode = @ARGV && $ARGV[0] eq '--datagram';
if ($datagramMode) {
	shift @ARGV;
	die "usage: $0 --datagram <golden-dir> <out-dir> <name-regex>\n"
		unless @ARGV == 3;
	datagram_seeds(@ARGV);
	exit 0;
}

die "usage: $0 <golden-dir> <wire-layout.txt> <out-dir> <name-regex>\n"
	unless @ARGV == 4;
my ($goldenDir, $layoutFile, $outDir, $namePattern) = @ARGV;
my $nameRegex = qr/$namePattern/;

# id and max body size per packet name, from the inventory's
# "<id>\t<name>\t<max>" rows.
my (%idOf, %maxOf);
open my $layout, '<', $layoutFile or die "cannot read $layoutFile: $!\n";
while (my $line = <$layout>) {
	next if $line =~ /^#/;
	$line =~ s/\r?\n\z//;
	my ($id, $name, $max) = split /\t/, $line;
	next unless defined $max;
	$idOf{$name} = $id;
	$maxOf{$name} = $max;
}
close $layout;
die "no packets in $layoutFile\n" unless %idOf;

make_path($outDir);
-d $outDir or die "cannot create $outDir\n";
opendir my $dir, $outDir or die "cannot list $outDir: $!\n";
for my $stale (grep { /^(?:golden|id)-/ } readdir $dir) {
	unlink "$outDir/$stale" or die "cannot delete $outDir/$stale: $!\n";
}
closedir $dir;

sub write_seed {
	my ($name, $bytes) = @_;
	open my $out, '>:raw', "$outDir/$name" or die "cannot write $outDir/$name: $!\n";
	print $out $bytes;
	close $out or die "cannot write $outDir/$name: $!\n";
}

sub frame {
	my ($id, $body) = @_;
	return pack('vVC', $id, length $body, 0) . $body;
}

my $count = 0;

opendir my $goldens, $goldenDir or die "cannot list $goldenDir: $!\n";
for my $file (sort grep { /\.hex\z/ } readdir $goldens) {
	my ($base) = $file =~ /^(.+)\.hex\z/;
	my ($name) = split /\./, $base;
	next unless $name =~ $nameRegex;
	next if $base =~ /\.datagram\./;
	my ($code) = $base =~ /\.code(\d+)\z/;
	die "$file: no .code<N> suffix\n" unless defined $code;

	open my $in, '<', "$goldenDir/$file" or die "cannot read $goldenDir/$file: $!\n";
	my $hex = do { local $/; <$in> };
	close $in;
	$hex =~ s/\s+//g;
	die "$file: not an even run of hex digits\n"
		unless $hex =~ /\A(?:[0-9a-fA-F]{2})*\z/;
	my $body = pack('H*', $hex);

	my $stream;
	if ($base =~ /\.framed\./) {
		$stream = $body;
	} else {
		die "$file: $name is not in $layoutFile\n" unless defined $idOf{$name};
		$stream = frame($idOf{$name}, $body);
	}
	write_seed("golden-$base", chr($code) . $stream);
	$count++;
}
closedir $goldens;

for my $name (sort keys %idOf) {
	next unless $name =~ $nameRegex;
	my $zeroLen = $maxOf{$name} < 64 ? $maxOf{$name} : 64;
	for my $len ($zeroLen > 0 ? (0, $zeroLen) : (0)) {
		write_seed("id-$name-$len", chr(0) . frame($idOf{$name}, "\0" x $len));
		$count++;
	}
}

print "$count seeds\n";

#----------------------------------------------------------------------
# The datagram mode; see the file header.
#----------------------------------------------------------------------
sub datagram_seeds {
	my ($goldenDir, $outDir, $namePattern) = @_;
	my $nameRegex = qr/$namePattern/;

	make_path($outDir);
	-d $outDir or die "cannot create $outDir\n";
	opendir my $dir, $outDir or die "cannot list $outDir: $!\n";
	for my $stale (grep { /^(?:golden|id)-/ } readdir $dir) {
		unlink "$outDir/$stale" or die "cannot delete $outDir/$stale: $!\n";
	}
	closedir $dir;

	my $write = sub {
		my ($name, $bytes) = @_;
		open my $out, '>:raw', "$outDir/$name" or die "cannot write $outDir/$name: $!\n";
		print $out $bytes;
		close $out or die "cannot write $outDir/$name: $!\n";
	};

	my (%idOf, $count);
	$count = 0;
	opendir my $goldens, $goldenDir or die "cannot list $goldenDir: $!\n";
	for my $file (sort grep { /\.datagram\.code\d+\.hex\z/ } readdir $goldens) {
		my ($base) = $file =~ /^(.+)\.hex\z/;
		my ($name) = split /\./, $base;
		next unless $name =~ $nameRegex;

		open my $in, '<', "$goldenDir/$file" or die "cannot read $goldenDir/$file: $!\n";
		my $hex = do { local $/; <$in> };
		close $in;
		$hex =~ s/\s+//g;
		die "$file: not an even run of hex digits\n"
			unless $hex =~ /\A(?:[0-9a-fA-F]{2})*\z/;
		my $frame = pack('H*', $hex);
		die "$file: shorter than a datagram header\n" if length $frame < 6;
		my ($id, $size) = unpack('vV', $frame);
		die "$file: size field $size disagrees with a body of " . (length($frame) - 6) . " bytes\n"
			unless $size == length($frame) - 6;

		$idOf{$name} = $id;
		$write->("golden-$base", $frame . "\0");
		$count++;
	}
	closedir $goldens;

	for my $name (sort keys %idOf) {
		for my $len (0, 16) {
			$write->("id-$name-$len", pack('vV', $idOf{$name}, $len) . ("\0" x $len) . "\0");
			$count++;
		}
	}

	print "$count seeds\n";
}
