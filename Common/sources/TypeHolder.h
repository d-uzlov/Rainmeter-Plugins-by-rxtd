/*
 * Copyright (C) 2020 rxtd
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
		Rainmeter rain;
		Rainmeter::Logger& logger;

	private:
		MeasureState measureState = MeasureState::eWORKING;

		double resultDouble = 0.0;
		string resultString { };
		string resolveString { };
		bool useResultString = false;

	public:
		TypeHolder(Rainmeter&& rain);
		virtual ~TypeHolder() = default;
		TypeHolder(const TypeHolder& other) = delete;
		TypeHolder(TypeHolder&& other) = delete;
		TypeHolder& operator=(const TypeHolder& other) = delete;
		TypeHolder& operator=(TypeHolder&& other) = delete;

	protected:
		virtual void _reload() = 0;
		virtual double _update() = 0;
		virtual void _updateString(string& resultStringBuffer) { }
		virtual void _command(isview bangArgs);
		virtual void _resolve(int argc, const wchar_t* argv[], string& resolveBufferString);

		void setMeasureState(MeasureState brokenState);
		void setUseResultString(bool value);

	public:
		double update();
		void reload();
		const wchar_t* getString() const;
		void command(const wchar_t *bangArgs);
		const wchar_t* resolve(int argc, const wchar_t* argv[]);

		MeasureState getState() const;
	};

	class ParentBase : public TypeHolder {
		// skin -> { name -> parent }
		static std::map<Rainmeter::Skin, std::map<istring, ParentBase*, std::less<>>> skinMap;

	public:
		explicit ParentBase(Rainmeter&& rain);

		~ParentBase();

		ParentBase(const ParentBase& other) = delete;
		ParentBase(ParentBase&& other) noexcept = delete;
		ParentBase& operator=(const ParentBase& other) = delete;
		ParentBase& operator=(ParentBase&& other) noexcept = delete;

		template<typename T>
		static T* find(Rainmeter::Skin skin, isview measureName) {
			static_assert(std::is_base_of<ParentBase, T>::value, "only parent measures can be searched for");

			return dynamic_cast<T*>(findParent(skin, measureName));
		}

	private:
		static ParentBase* findParent(Rainmeter::Skin skin, isview measureName);
	};
}
