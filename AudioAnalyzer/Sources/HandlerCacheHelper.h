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
#include "sound-processing/sound-handlers/SoundHandler.h"

namespace rxtd::audio_analyzer {
	class HandlerCacheHelper {
		using Logger = utils::Rainmeter::Logger;
		using Rainmeter = utils::Rainmeter;

		struct HandlerRawInfo {
			bool updated = false;
			string rawDescription;
			string rawDescription2;
			std::unique_ptr<HandlerPatcher> patcher;
		};

		using PatchersMap = std::map<istring, HandlerRawInfo, std::less<>>;
		PatchersMap patchersCache;
		Rainmeter rain;
		bool anythingChanged = false;
		bool unusedOptionsWarning = false;
		index legacyNumber{ };

	public:
		void setRain(Rainmeter value) {
			rain = std::move(value);
		}

		void setUnusedOptionsWarning(bool value) {
			unusedOptionsWarning = value;
		}

		void setLegacyNumber(index value) {
			legacyNumber = value;
		}

		void reset() {
			for (auto& [name, info] : patchersCache) {
				info.updated = false;
			}
			anythingChanged = false;
		}

		bool isAnythingChanged() const {
			return anythingChanged;
		}

		[[nodiscard]]
		HandlerPatcher* getHandler(const istring& name);

		[[nodiscard]]
		HandlerRawInfo parseHandler(sview name, HandlerRawInfo handler);

		[[nodiscard]]
		std::unique_ptr<HandlerPatcher> createHandlerPatcher(const utils::OptionMap& optionMap, Logger& cl) const;

		template <typename T>
		[[nodiscard]]
		std::unique_ptr<HandlerPatcher> createPatcher(const utils::OptionMap& om, Logger& cl) const {
			return std::make_unique<SoundHandler::HandlerPatcherImpl<T>>(om, cl, rain, legacyNumber);
		}

		void readRawDescription2(isview type, const utils::OptionMap& optionMap, string& rawDescription2) const;
	};
}
