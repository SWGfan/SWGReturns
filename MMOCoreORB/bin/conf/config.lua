-- Core3 Config File
-- 0 = false, 1 = true

Core3 = {
	MakeLogin = 1,
	MakeZone = 1,
	MakePing = 1,
	MakeStatus = 1,
	MakeWeb = 0,

	ORB = "",
	ORBPort = 44419,

	DBHost = "localhost",
	DBPort = 3306,
	DBName = "swgemu",
	DBUser = "swgemu",
	DBPass = "123456",
	DBInstances = 3,
	DBSecret = "swgemus3cr37!", -- Change this! This value should be unique and of reasonable length.

	LoginPort = 44453,
	LoginProcessingThreads = 1,
	LoginAllowedConnections = 3000,
	LoginRequiredVersion = "20050408-18:00",

	MantisHost = "127.0.0.1",
	MantisPort = 3306,
	MantisName = "swgemu",
	MantisUser = "swgemu",
	MantisPass = "123456",
	MantisPrfx = "mantis_", -- The prefix for your mantis tables.

	MetricsHost = "localhost",
	MetricsPort = 8125,
	MetricsPrefix = "",

	AutoReg = 0,

	ProgressMonitors = "true",

	PingPort = 44462,
	PingAllowedConnections = 3000,

	ZoneProcessingThreads = 10,
	ZoneAllowedConnections = 30000,
	ZoneGalaxyID = 2, --The actual zone server's galaxyID. Should coordinate with your login server.

	ZonesEnabled = {
		"chandrila",
		"corellia",
		"coruscant",
		"dungeon1",
		"dungeon2",
		"dantooine",
		"dathomir",
		"endor",
		"geonosis",
		"hoth",
		"hutta",
		"jakku",
		"kaas",
		"kashyyyk",
		"korriban",
		"lok",
		"mandalore",
		"mustafar",
		"naboo",
		"rori",
		"taanab",
		"talus",
		"tatooine",
		"tutorial",
		"yavin4"
	},

	TrePath = "/home/ubuntu/SWGEmu/",

	TreFiles = {
	"top_house_assets.tre",
	"returns_patch12.tre",
	"returns_custom2.tre",
	"patch_zzz_01.tre",
	"returns11.tre",	
	"returns10.tre",
	"returns9.tre",
	"returns8.tre",
	"returns7.tre",
	"returns6.tre",
	"returns5.tre",
	"returns4.tre",
	"returns3.tre",
	"returns2.tre",
	"returns1.tre",
	"mtg_patch_022.tre",
	"mtg_planets.tre",
	"mtg_patch_021.tre",
	"mtg_patch_019.tre",
	"mtg_patch_018.tre",
	"mtg_patch_017.tre",
	"mtg_patch_016.tre",
	"mtg_patch_015.tre",
	"mtg_patch_014.tre",
	"mtg_patch_013_configurable_02.tre",
	"mtg_patch_012_configurable_01.tre",
	"mtg_patch_011_files_01.tre",
	"mtg_patch_010_object_01.tre",
	"mtg_patch_009_Shader_01.tre",
	"mtg_patch_008_texture_04.tre",
	"mtg_patch_007_texture_03.tre",
	"mtg_patch_006_texture_02.tre",
	"mtg_patch_005_texture_01.tre",
	"mtg_patch_004_appearance_04.tre",
	"mtg_patch_003_appearance_03.tre",
	"mtg_patch_002_appearance_02.tre",
	"mtg_patch_001_appearance_01.tre",
	"bottom_house_assets.tre"
	},

	StatusPort = 44455,
	StatusAllowedConnections = 500,
	StatusInterval = 30,

	WebPorts = 44460,
	WebAccessLog = "../log/webaccess.log",
	WebErrorLog = "../log/weberror.log",
	WebSessionTimeout = 600,

	DeleteCharacters = 10,
	MaxNavMeshJobs = 4,
	MaxAuctionSearchJobs = 2,
	DumpObjFiles = 1,

	UnloadContainers = 1,

	LogFile = "log/core3.log",
	LogFileLevel = 2,
	LogJSON = 0,
	LogSync = 0,

	LuaLogJSON = 0,
	PathfinderLogJSON = 0,

	TermsOfServiceVersion = 0,
	TermsOfService = "",

	CleanupMailCount = 25000,

	RESTServerPort = 0,

	InactiveAccountTitle = "Account Disabled",
	InactiveAccountText = "The server administrators have disabled your account.",

	CharacterBuilderEnabled = "false",

	PlayerLogLevel = 2,
	MaxLogLines = 100000,

	structurePackupEnabled = "true",

	inactiveStructurePackupEnabled = "false",
	inactiveStructurePackupDays = 365,
}

-- NOTE: conf/config-local.lua is parsed after this file if it exists
