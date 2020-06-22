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
		Rainmeter::Logger& logger;

	private:
		MeasureState measureState = MeasureState::eWORKING;

		double resultDouble = 0.0;
		const wchar_t *resultString = nullptr;

	public:
		TypeHolder(Rainmeter&& rain);
		virtual ~TypeHolder() = default;
		TypeHolder(const TypeHolder& other) = delete;
		TypeHolder(TypeHolder&& other) = delete;
		TypeHolder& operator=(const TypeHolder& other) = delete;
		TypeHolder& operator=(TypeHolder&& other) = delete;

	protected:
		virtual void _reload() = 0;
		virtual std::tuple<double, const wchar_t*> _update() = 0;
		virtual void _command(isview bangArgs);
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
		std::map<Rainmeter::Skin, std::map<istring, T*, std::less<>>> skinMap;

	public:
		void add(T& parent);
		void remove(T& parent);
		T* findParent(Rainmeter::Skin skin, isview measureName);
	};

	template <typename T>
	void ParentManager<T>::add(T& parent) {
		skinMap[parent.rain.getSkin()][parent.rain.getMeasureName() % ciView() % own()] = &parent;
	}

	template<typename T>
	void ParentManager<T>::remove(T& parent) {
		const auto skinIter = skinMap.find(parent.rain.getSkin());
		if (skinIter == skinMap.end()) {
			std::terminate();
		}
		auto& measuresMap = skinIter->second;

		const auto measureIter = measuresMap.find(parent.rain.getMeasureName() % ciView());
		if (measureIter == measuresMap.end()) {
			std::terminate();
		}
		measuresMap.erase(measureIter);

		if (measuresMap.empty()) {
			skinMap.erase(skinIter);
		}
	}

	template<typename T>
	T* ParentManager<T>::findParent(Rainmeter::Skin skin, isview measureName) {
		const auto skinIter = skinMap.find(skin);
		if (skinIter == skinMap.end()) {
			return nullptr;
		}
		const auto& measuresMap = skinIter->second;

		const auto measureIter = measuresMap.find(measureName);
		if (measureIter == measuresMap.end()) {
			return nullptr;
		}

		auto result = measureIter->second;
		if (result->getState() == MeasureState::eBROKEN) {
			return nullptr;
		}

		return result;
	}
}
