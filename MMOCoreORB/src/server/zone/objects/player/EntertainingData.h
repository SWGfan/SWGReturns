/*
 * EntertainingData.h
 *
 *  Created on: 20/09/2010
 *      Author: victor
 */

#ifndef ENTERTAININGDATA_H_
#define ENTERTAININGDATA_H_

#include "engine/engine.h"

class EntertainingData : public Serializable {
	int duration;
	int strength;
	int timeStarted;
	bool buffApplied;
public:
	EntertainingData() {
		duration = 0;
		strength = 0;
		timeStarted = time(0);
		buffApplied = false;
		addSerializableVariables();
	}

	EntertainingData(const EntertainingData& d) : Object(), Serializable() {
		duration = d.duration;
		strength = d.strength;
		timeStarted = d.timeStarted;
		buffApplied = d.buffApplied;

		addSerializableVariables();
	}

	EntertainingData& operator=(const EntertainingData& d) {
		if (this == &d)
			return *this;

		duration = d.duration;
		strength = d.strength;
		timeStarted = d.timeStarted;
		buffApplied = d.buffApplied;

		return *this;
	}

	inline void addSerializableVariables() {
		addSerializableVariable("duration", &duration);
		addSerializableVariable("strength", &strength);
		addSerializableVariable("timeStarted", &timeStarted);
		addSerializableVariable("buffApplied", &buffApplied);
	}

	inline int getDuration() {
		return duration;
	}

	inline int getStrength() {
		return strength;
	}
	inline int getTimeStarted() {
		return timeStarted;
	}
	inline bool getBuffApplied() {
		return buffApplied;
	}
	inline void setStrength(int str) {
		strength = str;
	}

	inline void incrementStrength(int incr) {
		strength += incr;
	}

	inline void setDuration(int dur) {
		duration = dur;
	}

	inline void incrementDuration(int incr) {
		duration += incr;
	}

	inline void setBuffApplied(bool applied) {
		buffApplied = applied;
	}
};

#endif /* ENTERTAININGDATA_H_ */
