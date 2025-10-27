-- Consolidate strong melee/ranged templates so bosses feel distinct
bossAttacks = {
  lightsaber_heavy = merge(lightsabermaster, forcepowermaster, forcewielder),
  saber_duelist    = merge(lightsabermaster, fencermaster, pikemanmaster),
  force_controller = merge(forcepowermaster, tkmaster),
  droid_assassin   = merge(bountyhuntermid, marksmanmaster, brawlernovice)
}
