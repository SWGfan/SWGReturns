#!/usr/bin/env perl
# patch_xp_slicing_medic_imagedesign.pl
# Adds slicingExpMultiplier, medicExpMultiplier, imagedesignExpMultiplier
# to IDL, CPP, and player_manager.lua.
# Usage: perl patch_xp_slicing_medic_imagedesign.pl /path/to/MMOCoreORB

use strict; use warnings;
my $root = shift or die "Usage: perl patch_xp_slicing_medic_imagedesign.pl /path/to/MMOCoreORB\n";
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

    for my $entry (
        [ 'medicExpMultiplier',      'medicExpMultiplier = 1;'      ],
        [ 'slicingExpMultiplier',     'slicingExpMultiplier = 1;'    ],
        [ 'imagedesignExpMultiplier', 'imagedesignExpMultiplier = 1;'],
    ) {
        my ($key, $decl) = @$entry;
        next if $src =~ /\Q$key\E/;
        # Insert after the last private float line we can find
        $src =~ s/(private float medicExpMultiplier[^;]*;|private float shipwrightExpMultiplier[^;]*;)/$1\n\tprivate float $decl/
            or $src =~ s/(private float groupExpMultiplier[^;]*;)/$1\n\tprivate float $decl/;
        $changed++;
    }

    if ($changed) {
        rename $IDL, "$IDL.bak";
        write_file($IDL, $src);
        print "PATCHED IDL: added $changed new multiplier(s)\n";
    } else { print "SKIP IDL: all multipliers already declared\n" }
}

# ── CPP: loadLuaConfig ────────────────────────────────────────────────────────
{
    my $src = read_file($CPP);
    my $anchor = 'medicExpMultiplier        = lua->getGlobalFloat("medicExpMultiplier");';
    my $changed = 0;

    my @new_reads = (
        [ 'slicingExpMultiplier',     "\tslicingExpMultiplier      = lua->getGlobalFloat(\"slicingExpMultiplier\");"     ],
        [ 'imagedesignExpMultiplier', "\timagedesignExpMultiplier  = lua->getGlobalFloat(\"imagedesignExpMultiplier\");" ],
    );

    # If medic read is missing too, add it
    if ($src !~ /medicExpMultiplier\s*=\s*lua/) {
        unshift @new_reads, ['medicExpMultiplier', "\tmedicExpMultiplier        = lua->getGlobalFloat(\"medicExpMultiplier\");"];
    }

    for my $pair (@new_reads) {
        my ($key, $line) = @$pair;
        next if $src =~ /\Q$key\E.*getGlobalFloat/;
        # Append after shipwright read or globalExpMultiplier read
        $src =~ s/(shipwrightExpMultiplier\s*=\s*lua->getGlobalFloat\([^)]+\);)/$1\n$line/
            or $src =~ s/(globalExpMultiplier\s*=\s*lua->getGlobalFloat\([^)]+\);)/$1\n$line/;
        $changed++;
    }

    if ($changed) {
        rename $CPP, "$CPP.bak" unless -f "$CPP.bak";
        write_file($CPP, $src);
        print "PATCHED CPP loadLuaConfig: $changed read(s) added\n";
    } else { print "SKIP CPP loadLuaConfig: all reads already present\n" }
}

# ── CPP: awardExperience branches ─────────────────────────────────────────────
{
    my $src = read_file($CPP);
    my $changed = 0;

    # medic branch (medical + combatmedic)
    if ($src !~ /medicExpMultiplier/ || $src !~ /xpType == "medical".*medicExp/) {
        my $old = 'else if (xpType == "shipwright")' . "\n\t\t\ttypeMultiplier = " .
                  (($src =~ /safeRate/) ? 'safeRate(shipwrightExpMultiplier)' : 'shipwrightExpMultiplier') . ';';
        my $branch = ($src =~ /safeRate/) ? 'safeRate(medicExpMultiplier)' : 'medicExpMultiplier';
        my $new = $old . "\n\t\telse if (xpType == \"medical\" || xpType == \"combatmedic\")\n\t\t\ttypeMultiplier = $branch;";
        if (index($src, $old) >= 0 && $src !~ /medicExpMultiplier/) {
            $src =~ s/\Q$old\E/$new/; $changed++;
        }
    }

    # slicing branch
    if ($src !~ /slicingExpMultiplier/) {
        my $anchor_pat = ($src =~ /xpType == "medical"/) 
            ? '(xpType == "medical"[^;]+;)'
            : ($src =~ /xpType == "shipwright"/)
              ? '(xpType == "shipwright"[^;]+;)'
              : '(typeMultiplier = (?:safeRate\()?scoutExpMultiplier\)?;)';
        my $branch = ($src =~ /safeRate/) ? 'safeRate(slicingExpMultiplier)' : 'slicingExpMultiplier';
        if ($src =~ s/($anchor_pat)/$1\n\t\telse if (xpType == "slicing")\n\t\t\ttypeMultiplier = $branch;/) {
            $changed++;
        }
    }

    # imagedesigner branch
    if ($src !~ /imagedesignExpMultiplier/) {
        my $branch = ($src =~ /safeRate/) ? 'safeRate(imagedesignExpMultiplier)' : 'imagedesignExpMultiplier';
        if ($src =~ s/(xpType == "slicing"[^;]+;)/$1\n\t\telse if (xpType == "imagedesigner")\n\t\t\ttypeMultiplier = $branch;/) {
            $changed++;
        }
    }

    # wrap new multipliers with safeRate() if already using it
    if ($src =~ /safeRate/) {
        for my $m (qw(medicExpMultiplier slicingExpMultiplier imagedesignExpMultiplier)) {
            $src =~ s/typeMultiplier = $m;/typeMultiplier = safeRate($m);/g;
        }
    }

    if ($changed) {
        rename $CPP, "$CPP.bak" unless -f "$CPP.bak";
        write_file($CPP, $src);
        print "PATCHED CPP awardExperience: $changed branch(es) added\n";
    } else { print "SKIP CPP awardExperience: all branches already present\n" }
}

# ── player_manager.lua ────────────────────────────────────────────────────────
{
    my $src = read_file($LUA);
    my @to_add;
    push @to_add, "medicExpMultiplier       = 10" unless $src =~ /medicExpMultiplier/;
    push @to_add, "slicingExpMultiplier     = 10" unless $src =~ /slicingExpMultiplier/;
    push @to_add, "imagedesignExpMultiplier = 10" unless $src =~ /imagedesignExpMultiplier/;

    if (@to_add) {
        rename $LUA, "$LUA.bak" unless -f "$LUA.bak";
        open my $fh, '>>', $LUA or die $!;
        print $fh "\n" . join("\n", @to_add) . "\n";
        close $fh;
        print "PATCHED LUA: added " . scalar(@to_add) . " value(s)\n";
    } else { print "SKIP LUA: all values already present\n" }
}

print "\nDone. Recompile for IDL/CPP changes.\n";
