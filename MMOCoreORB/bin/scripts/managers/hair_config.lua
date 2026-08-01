--[[
  Hair configuration for character creation.
  Maps player templates to their available hair styles and default hair.
  Used as fallback when hair_assets_skill_mods.iff / default_pc_hairstyles.iff are missing.
]]

hairConfig = {

  -- Maps playerTemplate -> list of available hair style template names (shared templates)
  -- These are the full IFF paths the client would send
  hairStyles = {

    -- Human
    ["object/creature/player/shared_human_male.iff"] = {
      default = "object/tangible/hair/human/hair_human_male_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_male_s01.iff",
        "object/tangible/hair/human/hair_human_male_s02.iff",
        "object/tangible/hair/human/hair_human_male_s03.iff",
        "object/tangible/hair/human/hair_human_male_s04.iff",
        "object/tangible/hair/human/hair_human_male_s05.iff",
        "object/tangible/hair/human/hair_human_male_s06.iff",
        "object/tangible/hair/human/hair_human_male_s07.iff",
        "object/tangible/hair/human/hair_human_male_s08.iff",
        "object/tangible/hair/human/hair_human_male_s09.iff",
        "object/tangible/hair/human/hair_human_male_s10.iff",
        "object/tangible/hair/human/hair_human_male_s11.iff",
        "object/tangible/hair/human/hair_human_male_s12.iff",
        "object/tangible/hair/human/hair_human_male_s13.iff",
        "object/tangible/hair/human/hair_human_male_s14.iff",
        "object/tangible/hair/human/hair_human_male_s15.iff",
        "object/tangible/hair/human/hair_human_male_s16.iff",
        "object/tangible/hair/human/hair_human_male_s17.iff",
        "object/tangible/hair/human/hair_human_male_s18.iff",
        "object/tangible/hair/human/hair_human_male_s19.iff",
        "object/tangible/hair/human/hair_human_male_s20.iff",
        "object/tangible/hair/human/hair_human_male_s21.iff",
        "object/tangible/hair/human/hair_human_male_s22.iff",
        "object/tangible/hair/human/hair_human_male_s23.iff",
        "object/tangible/hair/human/hair_human_male_s24.iff",
        "object/tangible/hair/human/hair_human_male_s25.iff",
        "object/tangible/hair/human/hair_human_male_s26.iff",
        "object/tangible/hair/human/hair_human_male_s27.iff",
        "object/tangible/hair/human/hair_human_male_s28.iff",
        "object/tangible/hair/human/hair_human_male_s29.iff",
        "object/tangible/hair/human/hair_human_male_s30.iff",
        "object/tangible/hair/human/hair_human_male_s31.iff",
        "object/tangible/hair/human/hair_human_male_s32.iff",
        "object/tangible/hair/human/hair_human_male_s33.iff",
        "object/tangible/hair/human/hair_human_male_s34.iff",
        "object/tangible/hair/human/hair_human_male_s35.iff",
        "object/tangible/hair/human/hair_human_male_s37.iff",
        "object/tangible/hair/human/hair_human_male_s38.iff",
        "object/tangible/hair/human/hair_human_male_s39.iff",
        "object/tangible/hair/human/hair_human_male_s40.iff",
        "object/tangible/hair/human/hair_human_male_s41.iff",
      },
    },
    ["object/creature/player/shared_human_female.iff"] = {
      default = "object/tangible/hair/human/hair_human_female_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_female_s01.iff",
        "object/tangible/hair/human/hair_human_female_s02.iff",
        "object/tangible/hair/human/hair_human_female_s03.iff",
        "object/tangible/hair/human/hair_human_female_s04.iff",
        "object/tangible/hair/human/hair_human_female_s05.iff",
        "object/tangible/hair/human/hair_human_female_s06.iff",
        "object/tangible/hair/human/hair_human_female_s07.iff",
        "object/tangible/hair/human/hair_human_female_s08.iff",
        "object/tangible/hair/human/hair_human_female_s09.iff",
        "object/tangible/hair/human/hair_human_female_s10.iff",
        "object/tangible/hair/human/hair_human_female_s11.iff",
        "object/tangible/hair/human/hair_human_female_s12.iff",
        "object/tangible/hair/human/hair_human_female_s14.iff",
        "object/tangible/hair/human/hair_human_female_s15.iff",
        "object/tangible/hair/human/hair_human_female_s16.iff",
        "object/tangible/hair/human/hair_human_female_s17.iff",
        "object/tangible/hair/human/hair_human_female_s18.iff",
        "object/tangible/hair/human/hair_human_female_s19.iff",
        "object/tangible/hair/human/hair_human_female_s20.iff",
        "object/tangible/hair/human/hair_human_female_s21.iff",
        "object/tangible/hair/human/hair_human_female_s22.iff",
        "object/tangible/hair/human/hair_human_female_s23.iff",
        "object/tangible/hair/human/hair_human_female_s24.iff",
        "object/tangible/hair/human/hair_human_female_s25.iff",
        "object/tangible/hair/human/hair_human_female_s26.iff",
        "object/tangible/hair/human/hair_human_female_s27.iff",
        "object/tangible/hair/human/hair_human_female_s30.iff",
        "object/tangible/hair/human/hair_human_female_s31.iff",
        "object/tangible/hair/human/hair_human_female_s32.iff",
        "object/tangible/hair/human/hair_human_female_s34.iff",
        "object/tangible/hair/human/hair_human_female_s35.iff",
        "object/tangible/hair/human/hair_human_female_s36.iff",
        "object/tangible/hair/human/hair_human_female_s37.iff",
        "object/tangible/hair/human/hair_human_female_s38.iff",
        "object/tangible/hair/human/hair_human_female_s39.iff",
        "object/tangible/hair/human/hair_human_female_s40.iff",
        "object/tangible/hair/human/hair_human_female_s41.iff",
        "object/tangible/hair/human/hair_human_female_s42.iff",
      },
    },

    -- Bothan
    ["object/creature/player/shared_bothan_male.iff"] = {
      default = "object/tangible/hair/bothan/hair_bothan_male_s01.iff",
      styles = {
        "object/tangible/hair/bothan/hair_bothan_male_s01.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s02.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s03.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s04.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s05.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s06.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s07.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s08.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s09.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s10.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s11.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s12.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s13.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s14.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s15.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s17.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s18.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s19.iff",
        "object/tangible/hair/bothan/hair_bothan_male_s20.iff",
      },
    },
    ["object/creature/player/shared_bothan_female.iff"] = {
      default = "object/tangible/hair/bothan/hair_bothan_female_s01.iff",
      styles = {
        "object/tangible/hair/bothan/hair_bothan_female_s01.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s02.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s03.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s04.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s05.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s06.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s07.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s08.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s09.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s10.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s11.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s12.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s13.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s14.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s15.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s16.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s17.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s18.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s19.iff",
        "object/tangible/hair/bothan/hair_bothan_female_s20.iff",
      },
    },

    -- Twilek
    ["object/creature/player/shared_twilek_male.iff"] = {
      default = "object/tangible/hair/twilek/hair_twilek_male_s01.iff",
      styles = {
        "object/tangible/hair/twilek/hair_twilek_male_s01.iff",
        "object/tangible/hair/twilek/hair_twilek_male_s02.iff",
        "object/tangible/hair/twilek/hair_twilek_male_s03.iff",
        "object/tangible/hair/twilek/hair_twilek_male_s04.iff",
        "object/tangible/hair/twilek/hair_twilek_male_s05.iff",
        "object/tangible/hair/twilek/hair_twilek_male_s06.iff",
        "object/tangible/hair/twilek/hair_twilek_male_s07.iff",
        "object/tangible/hair/twilek/hair_twilek_male_s08.iff",
      },
    },
    ["object/creature/player/shared_twilek_female.iff"] = {
      default = "object/tangible/hair/twilek/hair_twilek_female_s01.iff",
      styles = {
        "object/tangible/hair/twilek/hair_twilek_female_s01.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s02.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s03.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s04.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s05.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s06.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s07.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s08.iff",
      },
    },

    -- Rodian
    ["object/creature/player/shared_rodian_male.iff"] = {
      default = "object/tangible/hair/rodian/hair_rodian_male_s01.iff",
      styles = {
        "object/tangible/hair/rodian/hair_rodian_male_s01.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s02.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s03.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s04.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s05.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s06.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s07.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s08.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s09.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s10.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s11.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s12.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s13.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s14.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s15.iff",
        "object/tangible/hair/rodian/hair_rodian_male_s16.iff",
      },
    },
    ["object/creature/player/shared_rodian_female.iff"] = {
      default = "object/tangible/hair/rodian/hair_rodian_female_s01.iff",
      styles = {
        "object/tangible/hair/rodian/hair_rodian_female_s01.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s02.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s03.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s04.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s05.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s06.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s07.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s08.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s09.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s10.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s12.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s13.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s14.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s15.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s16.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s17.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s18.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s19.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s20.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s21.iff",
        "object/tangible/hair/rodian/hair_rodian_female_s22.iff",
      },
    },

    -- Zabrak
    ["object/creature/player/shared_zabrak_male.iff"] = {
      default = "object/tangible/hair/zabrak/hair_zabrak_male_s01.iff",
      styles = {
        "object/tangible/hair/zabrak/hair_zabrak_male_s01.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s02.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s03.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s04.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s05.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s06.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s07.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s08.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s09.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s10.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s11.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s12.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s13.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s14.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s15.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s16.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s17.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s18.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s19.iff",
        "object/tangible/hair/zabrak/hair_zabrak_male_s20.iff",
      },
    },
    ["object/creature/player/shared_zabrak_female.iff"] = {
      default = "object/tangible/hair/zabrak/hair_zabrak_female_s01.iff",
      styles = {
        "object/tangible/hair/zabrak/hair_zabrak_female_s01.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s02.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s03.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s04.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s05.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s06.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s07.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s08.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s09.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s10.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s11.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s12.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s13.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s14.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s15.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s16.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s17.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s18.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s19.iff",
        "object/tangible/hair/zabrak/hair_zabrak_female_s20.iff",
      },
    },

    -- Trandoshan
    ["object/creature/player/shared_trandoshan_male.iff"] = {
      default = "object/tangible/hair/trandoshan/hair_trandoshan_male_s01.iff",
      styles = {
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s01.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s02.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s03.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s04.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s05.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s06.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s07.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s08.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s09.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s10.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s11.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s12.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s13.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_male_s14.iff",
      },
    },
    ["object/creature/player/shared_trandoshan_female.iff"] = {
      default = "object/tangible/hair/trandoshan/hair_trandoshan_female_s01.iff",
      styles = {
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s01.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s02.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s03.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s04.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s05.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s06.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s07.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s08.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s09.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s10.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s11.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s12.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s13.iff",
        "object/tangible/hair/trandoshan/hair_trandoshan_female_s14.iff",
      },
    },

    -- Sullustan
    ["object/creature/player/shared_sullustan_male.iff"] = {
      default = "object/tangible/hair/sullustan/sul_hair_s01_m.iff",
      styles = {
        "object/tangible/hair/sullustan/sul_hair_s01_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s02_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s03_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s04_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s05_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s06_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s07_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s08_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s09_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s10_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s11_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s12_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s13_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s14_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s15_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s16_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s17_m.iff",
        "object/tangible/hair/sullustan/sul_hair_s18_m.iff",
      },
    },
    ["object/creature/player/shared_sullustan_female.iff"] = {
      default = "object/tangible/hair/sullustan/sul_hair_s01_f.iff",
      styles = {
        "object/tangible/hair/sullustan/sul_hair_s01_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s02_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s03_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s04_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s05_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s06_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s07_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s08_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s09_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s10_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s11_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s12_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s13_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s14_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s15_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s16_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s17_f.iff",
        "object/tangible/hair/sullustan/sul_hair_s18_f.iff",
      },
    },

    -- Wookiee (no hair templates available - bald by design)
    ["object/creature/player/shared_wookiee_male.iff"] = {
      default = "",
      styles = {},
    },
    ["object/creature/player/shared_wookiee_female.iff"] = {
      default = "",
      styles = {},
    },

    -- Mon Calamari (no hair templates available - bald by design)
    ["object/creature/player/shared_moncal_male.iff"] = {
      default = "",
      styles = {},
    },
    ["object/creature/player/shared_moncal_female.iff"] = {
      default = "",
      styles = {},
    },

    -- Aqualish (no hair templates available - bald by design)
    ["object/creature/player/shared_aqualish_male.iff"] = {
      default = "",
      styles = {},
    },

    -- Bith (no hair templates available - bald by design)
    ["object/creature/player/shared_bith_male.iff"] = {
      default = "",
      styles = {},
    },
    ["object/creature/player/shared_bith_female.iff"] = {
      default = "",
      styles = {},
    },

    -- Ithorian (no hair templates available - bald by design)
    ["object/creature/player/shared_ithorian_male.iff"] = {
      default = "",
      styles = {},
    },
    ["object/creature/player/shared_ithorian_female.iff"] = {
      default = "",
      styles = {},
    },

    -- Gran (no hair templates available - bald by design)
    ["object/creature/player/shared_gran_male.iff"] = {
      default = "",
      styles = {},
    },

    -- Nightsister (use twilek hair as base)
    ["object/creature/player/shared_nightsister_female.iff"] = {
      default = "object/tangible/hair/twilek/hair_twilek_female_s01.iff",
      styles = {
        "object/tangible/hair/twilek/hair_twilek_female_s01.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s02.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s03.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s04.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s05.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s06.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s07.iff",
        "object/tangible/hair/twilek/hair_twilek_female_s08.iff",
      },
    },

    -- Chiss
    ["object/creature/player/shared_chiss_male.iff"] = {
      default = "object/tangible/hair/human/hair_human_male_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_male_s01.iff",
        "object/tangible/hair/human/hair_human_male_s02.iff",
        "object/tangible/hair/human/hair_human_male_s03.iff",
        "object/tangible/hair/human/hair_human_male_s04.iff",
        "object/tangible/hair/human/hair_human_male_s05.iff",
      },
    },
    ["object/creature/player/shared_chiss_female.iff"] = {
      default = "object/tangible/hair/human/hair_human_female_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_female_s01.iff",
        "object/tangible/hair/human/hair_human_female_s02.iff",
        "object/tangible/hair/human/hair_human_female_s03.iff",
        "object/tangible/hair/human/hair_human_female_s04.iff",
        "object/tangible/hair/human/hair_human_female_s05.iff",
      },
    },

    -- SMC
    ["object/creature/player/shared_smc_female.iff"] = {
      default = "object/tangible/hair/singing_mountain_clan/hair_singing_mountain_clan_s01.iff",
      styles = {
        "object/tangible/hair/singing_mountain_clan/hair_singing_mountain_clan_s01.iff",
        "object/tangible/hair/singing_mountain_clan/hair_singing_mountain_clan_s02.iff",
        "object/tangible/hair/singing_mountain_clan/hair_singing_mountain_clan_s03.iff",
        "object/tangible/hair/singing_mountain_clan/singing_mountain_clan_bangs_s01.iff",
      },
    },

    -- Custom races
    ["object/creature/player/shared_devaronian_male.iff"] = {
      default = "object/tangible/hair/human/hair_human_male_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_male_s01.iff",
        "object/tangible/hair/human/hair_human_male_s02.iff",
        "object/tangible/hair/human/hair_human_male_s03.iff",
      },
    },
    ["object/creature/player/shared_gotal_male.iff"] = {
      default = "object/tangible/hair/human/hair_human_male_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_male_s01.iff",
        "object/tangible/hair/human/hair_human_male_s02.iff",
        "object/tangible/hair/human/hair_human_male_s03.iff",
      },
    },
    ["object/creature/player/shared_ishi_tib_male.iff"] = {
      default = "object/tangible/hair/human/hair_human_male_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_male_s01.iff",
        "object/tangible/hair/human/hair_human_male_s02.iff",
        "object/tangible/hair/human/hair_human_male_s03.iff",
      },
    },
    ["object/creature/player/shared_nautolan_male.iff"] = {
      default = "object/tangible/hair/human/hair_human_male_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_male_s01.iff",
        "object/tangible/hair/human/hair_human_male_s02.iff",
        "object/tangible/hair/human/hair_human_male_s03.iff",
      },
    },
    ["object/creature/player/shared_nikto_male.iff"] = {
      default = "object/tangible/hair/human/hair_human_male_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_male_s01.iff",
        "object/tangible/hair/human/hair_human_male_s02.iff",
        "object/tangible/hair/human/hair_human_male_s03.iff",
      },
    },
    ["object/creature/player/shared_quarren_male.iff"] = {
      default = "",
      styles = {},
    },
    ["object/creature/player/shared_talz_male.iff"] = {
      default = "",
      styles = {},
    },
    ["object/creature/player/shared_togruta_female.iff"] = {
      default = "object/tangible/hair/human/hair_human_female_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_female_s01.iff",
        "object/tangible/hair/human/hair_human_female_s02.iff",
        "object/tangible/hair/human/hair_human_female_s03.iff",
      },
    },
    ["object/creature/player/shared_weequay_male.iff"] = {
      default = "object/tangible/hair/human/hair_human_male_s01.iff",
      styles = {
        "object/tangible/hair/human/hair_human_male_s01.iff",
        "object/tangible/hair/human/hair_human_male_s02.iff",
        "object/tangible/hair/human/hair_human_male_s03.iff",
      },
    },

    -- Hutt (no hair)
    ["object/creature/player/shared_hutt_male.iff"] = {
      default = "",
      styles = {},
    },
    ["object/creature/player/shared_hutt_female.iff"] = {
      default = "",
      styles = {},
    },
  },

  -- Species that are allowed to be bald
  allowBald = {
    ["moncal"] = true,
    ["bothan"] = true,
    ["wookiee"] = true,
    ["rodian"] = true,
    ["human"] = true,
    ["trandoshan"] = false,
    ["twilek"] = false,
    ["zabrak"] = false,
    ["sullustan"] = true,
    ["bith"] = true,
    ["ithorian"] = true,
    ["aqualish"] = true,
    ["gran"] = true,
  },
}
