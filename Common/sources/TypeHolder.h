/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "RainmeterWrappers.h"

namespace rxtd::utils {
	enum class MeasureState {
		eWORKING,
		eTEMP_BROKEN,
		eBROKEN
	};
	class TypeHolder {
	protected:
		template<typename T>
		friend class ParentManager;

		Rainmeter rain;
		Rainmeter::Logger& log;

	private:
		MeasureState measureState = MeasureState::eWORKING;

		double resultDouble = 0.0;
		const wchar_t *resultString = nullptr;

	public:
		TypeHolder(Rainmeter&& rain);
		virtual ~TypeHolder();
		TypeHolder(const TypeHolder& other) = delete;
		TypeHolder(TypeHolder&& other) = delete;
		TypeHolder& operator=(const TypeHolder& other) = delete;
		TypeHolder& operator=(TypeHolder&& other) = delete;

	protected:
		virtual void _reload() = 0;
		virtual std::tuple<double, const wchar_t*> _update() = 0;
		virtual void _command(const wchar_t *bangArgs);
		virtual const wchar_t* _resolve(int argc, const wchar_t* argv[]);

		void setMeasureState(MeasureState brokenState);

	public:
		double update();
		void reload();
		const wchar_t* getString() const;
		void command(const wchar_t *bangArgs);
		const wchar_t* resolve(int argc, const wchar_t* argv[]);

		MeasureState getState() const;
	};

	template<typename T>
	class ParentManager {
		static_assert(std::is_base_of<TypeHolder, T>::value);

		// skin -> { name -> parent }
		std::map<void*, std::map<istring, T*, std::less<>>> skinMap;

	public:
		void add(T& parent);
		void remove(T& parent);
		T* findParent(Rainmeter::Skin skin, isview measureName);
	};

	template <typename T>
	void ParentManager<T>::add(T& parent) {
		skinMap[parent.rain.getSkin().getRawPointer()][parent.rain.getMeasureName() % ciView() % own()] = &parent;
	}

	template<typename T>
	void ParentManager<T>::remove(T& parent) {
		const auto iterMap = skinMap.find(parent.rain.getSkin().getRawPointer());
		if (iterMap == skinMap.end()) {
			std::terminate();
		}
		auto& measuresMap = iterMap->second;

		const auto iterMeasure = measuresMap.find(parent.rain.getMeasureName() % ciView());
		if (iterMeasure == measuresMap.end()) {
			std::terminate();
		}
		measuresMap.erase(iterMeasure);

		if (measuresMap.empty()) {
			skinMap.erase(iterMap);
		}
	}

	template<typename T>
	T* ParentManager<T>::findParent(Rainmeter::Skin skin, isview measureName) {
		const auto iterMap = skinMap.find(skin.getRawPointer());
		if (iterMap == skinMap.end()) {
			return nullptr;
		}
		const auto& measuresMap = iterMap->second;

		const auto iterMeasure = measuresMap.find(measureName);
		if (iterMeasure == measuresMap.end()) {
			return nullptr;
		}
		return iterMeasure->second;
	}
}
