/*
 * MapLocationTable.cpp
 *
 *  Created on: 24/06/2010
 *      Author: victor
 */

#include "MapLocationTable.h"
#include "templates/manager/PlanetMapCategory.h"
#include "templates/manager/PlanetMapSubCategory.h"
#include "server/zone/objects/scene/SceneObject.h"

static String getMapCategoryName(SceneObject* object) {
	const PlanetMapSubCategory* subCategory = object->getPlanetMapSubCategory();

	if (subCategory != nullptr)
		return subCategory->getName();

	const PlanetMapCategory* category = object->getPlanetMapCategory();
	return category != nullptr ? category->getName() : "";
}

void MapLocationTable::transferObject(SceneObject* object) {
	const String categoryName = getMapCategoryName(object);

	if (categoryName.isEmpty())
		return;

	int index = locations.find(categoryName);

	if (index == -1) {
		SortedVector<MapLocationEntry> sorted;
		sorted.setNoDuplicateInsertPlan();

		MapLocationEntry entry(object);
		sorted.put(entry);

		locations.put(categoryName, sorted);
	} else {
		SortedVector<MapLocationEntry>& vector = locations.elementAt(index).getValue();

		MapLocationEntry entry(object);
		vector.put(entry);
	}
}

void MapLocationTable::dropObject(SceneObject* object) {
	const String categoryName = getMapCategoryName(object);

	if (categoryName.isEmpty())
		return;

	int index = locations.find(categoryName);

	if (index != -1) {
		SortedVector<MapLocationEntry>& vector = locations.elementAt(index).getValue();

		MapLocationEntry entry(object);
		vector.drop(entry);

		if (vector.isEmpty())
			locations.remove(index);
	}
}

bool MapLocationTable::containsObject(SceneObject* object) const {
	const String categoryName = getMapCategoryName(object);

	if (categoryName.isEmpty())
		return false;

	int index = locations.find(categoryName);

	if (index != -1) {
		const SortedVector<MapLocationEntry>& vector = locations.elementAt(index).getValue();

		for (int i = 0; i < vector.size(); i++) {
			const auto& entry = vector.get(i);

			if (entry.getObjectID() == object->getObjectID()) {
				return true;
			}
		}
	}

	return false;
}

void MapLocationTable::updateObjectsIcon(SceneObject* object, byte icon) {
	const String categoryName = getMapCategoryName(object);

	if (categoryName.isEmpty())
		return;

	int index = locations.find(categoryName);

	if (index != -1) {
		SortedVector<MapLocationEntry>& vector = locations.elementAt(index).getValue();

		for (int i = 0; i < vector.size(); i++) {
			MapLocationEntry entry = vector.get(i);

			if (entry.getObjectID() == object->getObjectID()) {
				vector.drop(entry);
				entry.setIcon(icon);
				vector.put(entry);
				return;
			}
		}
	}
}

const SortedVector<MapLocationEntry>& MapLocationTable::getLocation(const String& name) const {
	return locations.get(name);
}

int MapLocationTable::findLocation(const String& name) const {
	return locations.find(name);
}
