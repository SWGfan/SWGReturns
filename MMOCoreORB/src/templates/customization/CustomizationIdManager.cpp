/*
 * CustomizationIdManager.cpp
 *
 *  Created on: 29/03/2012
 *      Author: victor
 */

#include "CustomizationIdManager.h"
#include "templates/datatables/DataTableIff.h"

CustomizationIdManager::CustomizationIdManager() {
	setLoggingName("CustomizationIdManager");
}

void CustomizationIdManager::loadHairAssetsSkillMods(IffStream* iffStream) {
	DataTableIff dataTable;
	dataTable.readObject(iffStream);

	for (int i = 0; i < dataTable.getTotalRows(); ++i) {
		HairAssetData* data = new HairAssetData();
		data->readObject(dataTable.getRow(i));

		hairAssetSkillMods.put(data->getServerTemplate() + "|" + data->getServerPlayerTemplate(), data);

		debug() << "adding " << data->getServerTemplate() << " for " << data->getServerPlayerTemplate();
	}

	info() << "loaded " << hairAssetSkillMods.size() << " hair assets";
}

void CustomizationIdManager::loadAllowBald(IffStream* iffStream) {
	DataTableIff dataTable;
	dataTable.readObject(iffStream);

	for (int i = 0; i < dataTable.getTotalRows(); ++i) {
		String species;
		bool val;

		DataTableRow* row = dataTable.getRow(i);

		row->getValue(0, species);
		row->getValue(1, val);

		allowBald.put(species, val);
	}

	info() << "loaded " << allowBald.size() << " allow bald species data";
}

void CustomizationIdManager::loadHairAssetsFromLua() {
	Lua* lua = new Lua();
	lua->init();

	lua->runFile("scripts/managers/hair_config.lua");

	LuaObject hairConfig = lua->getGlobalObject("hairConfig");

	if (hairConfig.getLuaState() == nullptr) {
		warning("hairConfig Lua not found for hair assets fallback.");
		delete lua;
		return;
	}

	LuaObject stylesTable = hairConfig.getObjectField("hairStyles");

	if (stylesTable.getLuaState() == nullptr) {
		warning("hairConfig.hairStyles not found for hair assets fallback.");
		delete lua;
		return;
	}

	int count = 0;
	lua_State* L = stylesTable.getLuaState();
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		String templateName = lua_tostring(L, -2);

		if (!templateName.isEmpty()) {
			String defaultHair = "";

			// Get the default field from value table
			if (lua_istable(L, -1)) {
				lua_getfield(L, -1, "default");
				if (lua_isstring(L, -1)) {
					defaultHair = lua_tostring(L, -1);
				}
				lua_pop(L, 1);
			}

			if (!defaultHair.isEmpty()) {
				HairAssetData* data = new HairAssetData();
				data->setServerTemplate(defaultHair);
				data->setServerPlayerTemplate(templateName);
				hairAssetSkillMods.put(defaultHair + "|" + templateName, data);
				count++;
			}
		}

		lua_pop(L, 1);
	}

	stylesTable.pop();
	hairConfig.pop();

	delete lua;
	lua = nullptr;

	info() << "loaded " << count << " hair assets from Lua fallback.";
}

void CustomizationIdManager::loadAllowBaldFromLua() {
	Lua* lua = new Lua();
	lua->init();

	lua->runFile("scripts/managers/hair_config.lua");

	LuaObject hairConfig = lua->getGlobalObject("hairConfig");

	if (hairConfig.getLuaState() == nullptr) {
		delete lua;
		return;
	}

	LuaObject allowBaldTable = hairConfig.getObjectField("allowBald");

	if (allowBaldTable.getLuaState() == nullptr) {
		delete lua;
		return;
	}

	lua_State* L = allowBaldTable.getLuaState();
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		String key = lua_tostring(L, -2);
		bool val = lua_toboolean(L, -1);

		if (!key.isEmpty()) {
			allowBald.put(key, val);
		}

		lua_pop(L, 1);
	}

	allowBaldTable.pop();
	hairConfig.pop();

	delete lua;
	lua = nullptr;

	info() << "loaded " << allowBald.size() << " allow bald entries from Lua fallback.";
}

void CustomizationIdManager::loadPaletteColumns(IffStream* iffStream) {
	DataTableIff dataTable;
	dataTable.readObject(iffStream);

	for (int i = 0; i < dataTable.getTotalRows(); ++i) {
		PaletteData* data = new PaletteData();
		data->readObject(dataTable.getRow(i));

		paletteColumns.put(data->getName(), data);

		debug() << "adding " << data->getName();
	}

	info() << "loaded " << paletteColumns.size() << " palette columns";
}

void CustomizationIdManager::readObject(IffStream* iffStream) {
	iffStream->openForm('CIDM');
	iffStream->openForm('0001');

	Chunk* data = iffStream->openChunk('DATA');

	while (data->hasData()) {
		int id = data->readShort();
		String var;
		data->readString(var);

		customizationIds.put(var, id);
		reverseIds.put(id, var);
	}

	iffStream->closeChunk('DATA');

	iffStream->closeForm('0001');
	iffStream->closeForm('CIDM');

	info() << "loaded " << customizationIds.size() << " customization ids";
}

