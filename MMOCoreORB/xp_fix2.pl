#!/usr/bin/env perl
# patch_xp_jedi_bioengineer.pl
# Adds jediExpMultiplier (jedi_general) and
# bioEngineerExpMultiplier (bio_engineer + bio_engineer_dna_harvesting)
# Usage: perl patch_xp_jedi_bioengineer.pl /path/to/MMOCoreORB

use strict; use warnings;
my $root = shift or die "Usage: perl patch_xp_jedi_bioengineer.pl /path/to/MMOCoreORB\n";
$root =~ s|/$||;

my $IDL = "$root/src/server/zone/managers/player/PlayerManager.idl";
my $CPP = "$root/src/server/zone/managers/player/PlayerManagerImplementation.cpp";
my $LUA = "$root/bin/scripts/managers/player_manager.lua";

for my $p ($IDL, $CPP, $LUA) { die "ERROR: not found: $p\n" unless -f $p; }

sub read_file  { open my $f, '<', $_[0] or die $!; local $/; <$f> }
sub write_file { open my $f, '>', $_[0] or die $!; print $f $_[1] }

# ── IDL ───────────────────────────────────────────────────────────────────────
{
    my $src = read_file($IDL);
    my $changed = 0;

    for my $decl ('jediExpMultiplier = 1;', 'bioEngineerExpMultiplier = 1;') {
        my $key = ($decl =~ /(\w+Multiplier)/)[0];
        next if $src =~ /\Q$key\E/;
        $src =~ s/(private float (?:imagedesignExpMultiplier|medicExpMultiplier|shipwrightExpMultiplier)[^;]*;)/$1\n\tprivate float $decl/
            or $src =~ s/(private float groupExpMultiplier[^;]*;)/$1\n\tprivate float $decl/;
        $changed++;
    }

    if ($changed) {
        rename $IDL, "$IDL.bak" unless -f "$IDL.bak";
        write_file($IDL, $src);
        print "PATCHED IDL: $changed multiplier(s) added\n";
    } else { print "SKIP IDL: already present\n" }
}

# ── CPP: loadLuaConfig ────────────────────────────────────────────────────────
{
    my $src = read_file($CPP);
    my $changed = 0;

    for my $pair (
        [ 'jediExpMultiplier',        "\tjediExpMultiplier         = lua->getGlobalFloat(\"jediExpMultiplier\");"        ],
        [ 'bioEngineerExpMultiplier', "\tbioEngineerExpMultiplier  = lua->getGlobalFloat(\"bioEngineerExpMultiplier\");" ],
    ) {
        my ($key, $line) = @$pair;
        next if $src =~ /\Q$key\E.*getGlobalFloat/;
        $src =~ s/((?:imagedesignExpMultiplier|shipwrightExpMultiplier|medicExpMultiplier)\s*=\s*lua->getGlobalFloat\([^)]+\);)/$1\n$line/
            or $src =~ s/(globalExpMultiplier\s*=\s*lua->getGlobalFloat\([^)]+\);)/$1\n$line/;
        $changed++;
    }

    if ($changed) {
        rename $CPP, "$CPP.bak" unless -f "$CPP.bak";
        write_file($CPP, $src);
        print "PATCHED CPP loadLuaConfig: $changed read(s) added\n";
    } else { print "SKIP CPP loadLuaConfig: already present\n" }
}

# ── CPP: awardExperience branches ─────────────────────────────────────────────
{
    my $src = read_file($CPP);
    my $safe = $src =~ /safeRate/;
    my $changed = 0;

    # jedi_general branch — insert before the scout/existing branches
    if ($src !~ /jediExpMultiplier/) {
        my $rate  = $safe ? 'safeRate(jediExpMultiplier)' : 'jediExpMultiplier';
        # insert after entertainer branch since that's early in the chain
        if ($src =~ s/(typeMultiplier = (?:safeRate\()?entertainingExpMultiplier\)?;)/$1\n\t\telse if (xpType == "jedi_general" || xpType == "combat_jedi_novice")\n\t\t\ttypeMultiplier = $rate;/) {
            $changed++;
        }
    }

    # bio engineer branch
    if ($src !~ /bioEngineerExpMultiplier/) {
        my $rate = $safe ? 'safeRate(bioEngineerExpMultiplier)' : 'bioEngineerExpMultiplier';
        if ($src =~ s/(typeMultiplier = (?:safeRate\()?jediExpMultiplier\)?;|typeMultiplier = (?:safeRate\()?entertainingExpMultiplier\)?;)/$1\n\t\telse if (xpType == "bio_engineer" || xpType == "bio_engineer_dna_harvesting")\n\t\t\ttypeMultiplier = $rate;/) {
            $changed++;
        }
    }

    # wrap with safeRate if needed
    if ($safe) {
        $src =~ s/typeMultiplier = (jediExpMultiplier|bioEngineerExpMultiplier);/typeMultiplier = safeRate($1);/g;
    }

    if ($changed) {
        rename $CPP, "$CPP.bak" unless -f "$CPP.bak";
        write_file($CPP, $src);
        print "PATCHED CPP awardExperience: $changed branch(es) added\n";
    } else { print "SKIP CPP awardExperience: already present\n" }
}

# ── player_manager.lua ────────────────────────────────────────────────────────
{
    my $src = read_file($LUA);
    my @to_add;
    push @to_add, "jediExpMultiplier        = 10" unless $src =~ /jediExpMultiplier/;
    push @to_add, "bioEngineerExpMultiplier = 10" unless $src =~ /bioEngineerExpMultiplier/;

    if (@to_add) {
        rename $LUA, "$LUA.bak" unless -f "$LUA.bak";
        open my $fh, '>>', $LUA or die $!;
        print $fh "\n" . join("\n", @to_add) . "\n";
        close $fh;
        print "PATCHED LUA: " . scalar(@to_add) . " value(s) added\n";
    } else { print "SKIP LUA: already present\n" }
}

print "\nDone. Recompile for IDL/CPP changes.\n";
