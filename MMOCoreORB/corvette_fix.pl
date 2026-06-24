#!/usr/bin/env perl
# fix_corvette_buildings.pl
# Strips the non-existent extra corvette building IDs, keeping only
# the standard 11 per faction.
# Usage: perl fix_corvette_buildings.pl /path/to/MMOCoreORB

use strict; use warnings;
my $root = shift or die "Usage: perl fix_corvette_buildings.pl /path/to/MMOCoreORB\n";
$root =~ s|/$||;

my $FILE = "$root/bin/scripts/screenplays/dungeon/corellian_corvette/corellianCorvette.lua";
die "ERROR: not found: $FILE\n" unless -f $FILE;

open my $fh, '<', $FILE or die $!;
my $src = do { local $/; <$fh> }; close $fh;

my $old = 'buildings = {
		{ faction = "neutral", buildingIds = { 1945494, 1945561, 1945628, 1945695, 1945762, 1945829, 1945896, 1945963, 1946030, 1946097, 1946164, 4335861, 4335928, 4335995, 4336062, 4336129 } },
		{ faction = "imperial", buildingIds = { 1946231, 1946298, 1946365, 1946432, 1946499, 1946566, 1946633, 1946700, 1946767, 1946834, 1946901, 4336196, 4336263, 4336330, 4336397, 4336464 } },
		{ faction = "rebel", buildingIds = { 1946968, 1947035, 1947102, 1947169, 1947236, 1947303, 1947370, 1947437, 1947504, 1947571, 1947638, 4336531, 4336598, 4336665, 4336732, 4336799 } }
	},';

my $new = 'buildings = {
		{ faction = "neutral",  buildingIds = { 1945494, 1945561, 1945628, 1945695, 1945762, 1945829, 1945896, 1945963, 1946030, 1946097, 1946164 } },
		{ faction = "imperial", buildingIds = { 1946231, 1946298, 1946365, 1946432, 1946499, 1946566, 1946633, 1946700, 1946767, 1946834, 1946901 } },
		{ faction = "rebel",    buildingIds = { 1946968, 1947035, 1947102, 1947169, 1947236, 1947303, 1947370, 1947437, 1947504, 1947571, 1947638 } }
	},';

if ($src !~ /4336799/) {
    print "Already fixed — nothing to do.\n"; exit 0;
}

die "ERROR: could not find buildings block — check file manually.\n"
    unless index($src, $old) >= 0;

rename $FILE, "$FILE.bak";
$src =~ s/\Q$old\E/$new/;

open $fh, '>', $FILE or die $!;
print $fh $src; close $fh;
print "Done — non-existent building IDs removed.\n";
