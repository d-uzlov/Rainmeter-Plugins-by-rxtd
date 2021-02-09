/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "SkinHandle.h"
#include "InstanceKeeper.h"
#include "Logger.h"
#include "option-parsing/Option.h"

namespace rxtd::common::rainmeter {
	class Rainmeter {
		DataHandle dataHandle{};
		SkinHandle skin{};
		string measureName{};
		InstanceKeeper instanceKeeper;

		// Used by #makeNullTerminated()
		mutable string optionNameBuffer;

	public:
		Rainmeter() = default;
		explicit Rainmeter(void* rm);

		// Read named option from description linked to "void* rm" pointer from ctor
		[[nodiscard]]
		options::Option read(sview optionName, bool replaceVariables = true) const;

		// Replaces Rainmeter variables with its values
		// See https://docs.rainmeter.net/developers/plugin/cpp/api/#ReplaceVariables
		[[nodiscard]]
		sview replaceVariables(sview string) const;

		// Replaces relative paths using skin path as a base
		[[nodiscard]]
		sview transformPathToAbsolute(sview path) const;

		// See #executeCommandAsync(sview, Skin)
		void executeCommandAsync(sview command) {
			executeCommandAsync(command, skin);
		}

		// Executed Rainmeter command
		// Unlike raw Rainmeter API, this function doesn't block until the command has finished
		void executeCommandAsync(sview command, SkinHandle skin);

		[[nodiscard]]
		DataHandle getHandle() const {
			return dataHandle;
		}

		[[nodiscard]]
		SkinHandle getSkin() const {
			return skin;
		}

		[[nodiscard]]
		sview getMeasureName() const {
			return measureName;
		}
		
		Logger createLogger() const {
			return Logger{ dataHandle };
		}

	private:
		// Rainmeter API only accepts null terminated C strings.
		// This class uses std::string_view instead, which aren't required to be null terminated.
		// This function returns data of input string_view if it was already null terminated
		// or a copy of the view with a '\0' symbol at the end
		[[nodiscard]]
		const wchar_t* makeNullTerminated(sview view) const;
	};
}
