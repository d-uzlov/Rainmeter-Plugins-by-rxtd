/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "rainmeter/Rainmeter.h"

namespace rxtd::utils {
	//
	// Convenient parent class for all Rainmeter Measure=Plugin classes.
	// Implements all the needed functions,
	// provides a convenient way for derived classes to get called.
	//
	// See rainmeter plugin API documentations
	// for information on when and why functions are called and what they should do.
	//	https://docs.rainmeter.net/developers/plugin/plugin-anatomy/
	//
	class TypeHolder : NonMovableBase, VirtualDestructorBase {
	protected:
		using Rainmeter = common::rainmeter::Rainmeter;
		using Logger = common::rainmeter::Logger;

		Rainmeter rain;
		Logger logger;

	private:
		bool objectIsValid = true;

		double resultDouble = 0.0;
		string resultString{};
		string resolveString{};
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

		bool isValid() const {
			return objectIsValid;
		}

	protected:
		// all functions in derived classes are allowed to throw std::runtime_error

		virtual void vReload() = 0;
		virtual double vUpdate() = 0;

		virtual void vUpdateString(string& resultStringBuffer) { }

		virtual void vCommand(isview bangArgs) {
			logger.warning(L"Measure does not have commands");
		}

		virtual void vResolve(array_view<isview> args, string& resolveBufferString) { }

		// Sets object state to invalid until next reload
		void setInvalid() {
			objectIsValid = false;
		}

		// When false, number is used as a string value.
		// See Rainmeter documentation for details.
		void setUseResultString(bool value) {
			useResultString = value;
		}
	};

	class ParentBase : public TypeHolder {
		using SkinHandle = ::rxtd::common::rainmeter::SkinHandle;
		using ParentMeasureName = istring;
		using SkinMap = std::map<ParentMeasureName, ParentBase*, std::less<>>;
		static std::map<SkinHandle, SkinMap> globalMeasuresMap;

	public:
		explicit ParentBase(Rainmeter&& _rain);

		~ParentBase();

		template<typename T>
		[[nodiscard]]
		static T* find(SkinHandle skin, isview measureName) {
			static_assert(std::is_base_of<ParentBase, T>::value, "only parent measures can be searched for");

			return dynamic_cast<T*>(findParent(skin, measureName));
		}

	private:
		[[nodiscard]]
		static ParentBase* findParent(SkinHandle skin, isview measureName);
	};
}
