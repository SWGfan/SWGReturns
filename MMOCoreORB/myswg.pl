#!/usr/bin/env perl
# fix_ent_xp.pl
# 1. Adds = 1 defaults to all XP multiplier floats in the IDL so they
#    never silently zero-out if Lua config fails to set them.
# 2. Adds a floor check in C++ so no multiplier can ever produce 0 XP.
# Usage: perl fix_ent_xp.pl /path/to/MMOCoreORB

use strict; use warnings;
my $root = shift or die "Usage: perl fix_ent_xp.pl /path/to/MMOCoreORB\n";
$root =~ s|/$||;

my $IDL = "$root/src/server/zone/managers/player/PlayerManager.idl";
my $CPP = "$root/src/server/zone/managers/player/PlayerManagerImplementation.cpp";
my $LUA = "$root/bin/scripts/managers/player_manager.lua";

# ── IDL: add = 1 defaults so unset multipliers fall back to normal rate ───────
{
    open my $fh, '<', $IDL or die "Cannot open $IDL: $!";
    my $src = do { local $/; <$fh> }; close $fh;

    my %fixes = (
        'craftingExpMultiplier;'     => 'craftingExpMultiplier = 1;',
        'entertainingExpMultiplier;' => 'entertainingExpMultiplier = 1;',
        'scoutExpMultiplier;'        => 'scoutExpMultiplier = 1;',
        'shipwrightExpMultiplier;'   => 'shipwrightExpMultiplier = 1;',
        'medicExpMultiplier;'        => 'medicExpMultiplier = 1;',
    );

    my $changed = 0;
    for my $old (keys %fixes) {
        if ($src =~ /\Q$old\E/ && $src !~ /\Q$fixes{$old}\E/) {
            $src =~ s/\Q$old\E/$fixes{$old}/;
            $changed++;
        }
    }

    if ($changed) {
        rename $IDL, "$IDL.bak";
        open $fh, '>', $IDL or die $!;
        print $fh $src; close $fh;
        print "PATCHED IDL: added = 1 defaults to $changed multiplier(s)\n";
    } else {
        print "SKIP IDL: defaults already set\n";
    }
}

# ── CPP: floor check — if multiplier is 0 or unset, use 1.1 (original rate) ──
{
    open my $fh, '<', $CPP or die "Cannot open $CPP: $!";
    my $src = do { local $/; <$fh> }; close $fh;

    my $old = "\t\tfloat typeMultiplier = 1.1f;";
    my $new = "\t\t// Clamp: if a multiplier was never set (0.0f), fall back to 1.1\n" .
              "\t\tauto safeRate = [](float r) { return r > 0.f ? r : 1.1f; };\n" .
              "\t\tfloat typeMultiplier = 1.1f;";

    if ($src =~ /\QsafeRate\E/) {
        print "SKIP CPP: floor check already present\n";
    } elsif (index($src, $old) >= 0) {
        rename $CPP, "$CPP.bak";
        $src =~ s/\Q$old\E/$new/;

        # Wrap each multiplier assignment with safeRate()
        for my $m (qw(entertainingExpMultiplier craftingExpMultiplier
                       scoutExpMultiplier shipwrightExpMultiplier medicExpMultiplier)) {
            $src =~ s/typeMultiplier = $m;/typeMultiplier = safeRate($m);/g;
        }

        open $fh, '>', $CPP or die $!;
        print $fh $src; close $fh;
        print "PATCHED CPP: floor check + safeRate() wrappers added\n";
    } else {
        print "WARN CPP: typeMultiplier pattern not found — check manually\n";
    }
}

# ── Lua: ensure entertainingExpMultiplier is present ─────────────────────────
{
    open my $fh, '<', $LUA or die "Cannot open $LUA: $!";
    my $src = do { local $/; <$fh> }; close $fh; close $fh;

    if ($src =~ /entertainingExpMultiplier/) {
        print "SKIP LUA: entertainingExpMultiplier already set\n";
    } else {
        rename $LUA, "$LUA.bak";
        open $fh, '>>', $LUA or die $!;
        print $fh "\nentertainingExpMultiplier = 10\n";
        close $fh;
        print "PATCHED LUA: entertainingExpMultiplier = 10 appended\n";
    }
}

print "\nDone. Recompile for IDL/CPP changes to take effect.\n";
