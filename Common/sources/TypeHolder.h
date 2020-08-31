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

namespace rxtd::utils {
	enum class MeasureState {
		eWORKING,
		eTEMP_BROKEN,
		eBROKEN
	};

	class TypeHolder : NonMovableBase, VirtualDestructorBase {
	protected:
		Rainmeter rain;
		Rainmeter::Logger logger;

	private:
		MeasureState measureState = MeasureState::eWORKING;

		double resultDouble = 0.0;
		string resultString{ };
		string resolveString{ };
		bool useResultString = false;

		std::vector<isview> resolveVector;

	public:
		TypeHolder(Rainmeter&& rain);

		double update();
		void reload();
		const wchar_t* getString() const;
		void command(const wchar_t* bangArgs);
		const wchar_t* resolve(int argc, const wchar_t* argv[]);
		const wchar_t* resolve(array_view<isview> args);

		MeasureState getState() const {
			return measureState;
		}

	protected:
		virtual void vReload() = 0;
		virtual double vUpdate() = 0;

		virtual void vUpdateString(string& resultStringBuffer) {
		}

		virtual void vCommand(isview bangArgs) {
			logger.warning(L"Measure does not have commands");
		}

		virtual void vResolve(array_view<isview> args, string& resolveBufferString) {
		}

		void setMeasureState(MeasureState value) {
			measureState = value;
		}

		void setUseResultString(bool value) {
			useResultString = value;
		}
	};

	class ParentBase : public TypeHolder {
		using ParentMeasureName = istring;
		using SkinMap = std::map<ParentMeasureName, ParentBase*, std::less<>>;
		static std::map<Rainmeter::Skin, SkinMap> globalMeasuresMap;

	public:
		explicit ParentBase(Rainmeter&& _rain);

		~ParentBase();

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
