/*
 * HairStyleInfo.h
 *
 *  Created on: Apr 22, 2011
 *      Author: crush
 */

#ifndef HAIRSTYLEINFO_H_
#define HAIRSTYLEINFO_H_

#include "templates/IffTemplate.h"
#include "engine/lua/LuaObject.h"

namespace server {
namespace zone {
namespace managers {
namespace player {
namespace creation {

class HairStyleInfo : public IffTemplate {
	String playerTemplate;
	Vector<String> styles;

public:
	HairStyleInfo() {
	}

	~HairStyleInfo() {
	}

	HairStyleInfo& operator= (const HairStyleInfo& hsi) {
		if (this == &hsi)
			return *this;

		playerTemplate = hsi.playerTemplate;
		styles = hsi.styles;

		return *this;
	}

	/**
	 * @pre: iffStream advanced to a PTMP form.
	 */
	void readObject(IffStream* iffStream) {
		if (iffStream->getNextFormType() != 'PTMP')
			return;

		Chunk* ptmp = iffStream->openForm('PTMP');

		Vector<Chunk*> children;
		ptmp->getChildren(children);

		for (int i = 0; i < children.size(); ++i) {
			Chunk* chunk = children.get(i);

			switch (chunk->getChunkID()) {
			case 'NAME':
				chunk->readString(playerTemplate);
				break;
			case 'ITMS':
			{
				while (chunk->hasData()) {
					String hairTemplate;
					chunk->readString(hairTemplate);
					styles.add(hairTemplate);
				}
			}
				break;
			}
		}

		iffStream->closeForm('PTMP');
	}

	inline bool hasStyle(const String& style) const {
		for (int i = 0; i < styles.size(); ++i) {
			if (styles.get(i) == style)
				return true;
		}

		return false;
	}

	inline int getTotalStyles() const {
		return styles.size();
	}

	inline const String& getFirstStyle() const {
		return styles.get(0);
	}

	inline const Vector<String>& getStyles() const {
		return styles;
	}

	inline const String& getPlayerTemplate() const {
		return playerTemplate;
	}

	void readObjectFromLua(const String& templateName, LuaObject& stylesArray) {
		playerTemplate = templateName;

		if (stylesArray.getLuaState() != nullptr) {
			int size = stylesArray.getTableSize();
			for (int i = 1; i <= size; ++i) {
				String style = stylesArray.getStringAt(i);
				if (!style.isEmpty()) {
					styles.add(style);
				}
			}
		}
	}
};

}
}
}
}
}

using namespace server::zone::managers::player::creation;

#endif /* HAIRSTYLEINFO_H_ */
