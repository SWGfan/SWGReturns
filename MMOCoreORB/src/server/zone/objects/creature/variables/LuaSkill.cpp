#include "LuaSkill.h"

const char LuaSkill::className[] = "LuaSkill";

Luna<LuaSkill>::RegType LuaSkill::Register[] = {
		{ "_setObject", &LuaSkill::_setObject },
		{ "getName", &LuaSkill::getName },
		{ "getMoneyRequired", &LuaSkill::getMoneyRequired },
		{ "getSkillPointsRequired", &LuaSkill::getSkillPointsRequired },
		{ "getSkillsRequired", &LuaSkill::getSkillsRequired },
		{ "getXpType", &LuaSkill::getXpType },
		{ "getXpCost", &LuaSkill::getXpCost },
		{ 0, 0 }
};

LuaSkill::LuaSkill(lua_State *L) {
	realObject = reinterpret_cast<Skill*>(lua_touserdata(L, 1));
}

LuaSkill::~LuaSkill(){
}

int LuaSkill::_setObject(lua_State* L) {
	realObject = reinterpret_cast<Skill*>(lua_touserdata(L, -1));

	return 0;
}

// Companion System (2026-07-15, live SIGSEGV hardening -- see NOTES.md):
// every method below dereferenced realObject unguarded. A Lua caller (e.g.
// the trainer conversation handler) that wraps a NULL Skill -- which
// happens whenever a skill name referenced by script data isn't present in
// the loaded skills.iff (live-confirmed: a server booted without
// companion_patch.tre in its TreFiles crashed in getSkillsRequired() the
// moment the Companion Handler trainer conversation opened) -- was a
// guaranteed server crash. Null-guard everything: scripts get nil/empty
// values instead, and the conversation degrades gracefully.
int LuaSkill::getName(lua_State* L) {
	if (realObject == nullptr) {
		lua_pushstring(L, "");
		return 1;
	}

	String text = realObject->getSkillName();
	lua_pushstring(L, text.toCharArray());
	return 1;
}

int LuaSkill::getMoneyRequired(lua_State* L) {
	lua_pushinteger(L, realObject != nullptr ? realObject->getMoneyRequired() : 0);
	return 1;
}

int LuaSkill::getSkillPointsRequired(lua_State* L) {
	lua_pushinteger(L, realObject != nullptr ? realObject->getSkillPointsRequired() : 0);
	return 1;
}

int LuaSkill::getSkillsRequired(lua_State* L) {
	const Vector<String>* requiredSkills = realObject != nullptr ? realObject->getSkillsRequired() : nullptr;

	if (requiredSkills == nullptr || requiredSkills->size() == 0) {
		lua_pushnil(L);
	} else {
		lua_newtable(L);

		for (int i = 0; i < requiredSkills->size(); i++) {
			lua_pushstring(L, requiredSkills->get(i).toCharArray());

			lua_rawseti(L, -2, i + 1);
		}
	}

	return 1;
}

int LuaSkill::getXpType(lua_State* L) {
	String text = realObject->getXpType();
	lua_pushstring(L, text.toCharArray());
	return 1;
}

int LuaSkill::getXpCost(lua_State* L) {
	int amount = realObject->getXpCost();
	lua_pushinteger(L, amount);
	return 1;
}
