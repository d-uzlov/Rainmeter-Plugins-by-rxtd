/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "RainmeterWrappers.h"
#include "array_view.h"

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
		string resultString{ };
		string resolveString{ };
		bool useResultString = false;

		istring resolveBuffer;
		std::vector<isview> resolveVector;

	public:
		TypeHolder(Rainmeter&& rain);
		virtual ~TypeHolder() = default;
		TypeHolder(const TypeHolder& other) = delete;
		TypeHolder(TypeHolder&& other) = delete;
		TypeHolder& operator=(const TypeHolder& other) = delete;
		TypeHolder& operator=(TypeHolder&& other) = delete;

		double update();
		void reload();
		const wchar_t* getString() const;
		void command(const wchar_t* bangArgs);
		const wchar_t* resolve(int argc, const wchar_t* argv[]);
		const wchar_t* resolve(array_view<isview> args);

		MeasureState getState() const;

	protected:
		virtual void _reload() = 0;
		virtual double _update() = 0;

		virtual void _updateString(string& resultStringBuffer) {
		}

		virtual void _command(isview bangArgs);

		virtual void _resolve(array_view<isview> args, string& resolveBufferString) {
		}

		void setMeasureState(MeasureState brokenState);
		void setUseResultString(bool value);

	};

	class ParentBase : public TypeHolder {
		using ParentMeasureName = istring;
		using SkinMap = std::map<ParentMeasureName, ParentBase*, std::less<>>;
		static std::map<Rainmeter::Skin, SkinMap> globalMeasuresMap;

	public:
		explicit ParentBase(Rainmeter&& rain);

		~ParentBase();

		ParentBase(const ParentBase& other) = delete;
		ParentBase(ParentBase&& other) noexcept = delete;
		ParentBase& operator=(const ParentBase& other) = delete;
		ParentBase& operator=(ParentBase&& other) noexcept = delete;

		template <typename T>
		[[nodiscard]]
		static T* find(Rainmeter::Skin skin, isview measureName) {
			static_assert(std::is_base_of<ParentBase, T>::value, "only parent measures can be searched for");

			return dynamic_cast<T*>(findParent(skin, measureName));
		}

	private:
		[[nodiscard]]
		static ParentBase* findParent(Rainmeter::Skin skin, isview measureName);
	};
}
