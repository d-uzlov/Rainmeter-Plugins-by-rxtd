/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "PatchInfo.h"
#include "rainmeter/Logger.h"
#include "rainmeter/Rainmeter.h"
#include "sound-processing/sound-handlers/SoundHandlerBase.h"

namespace rxtd::audio_analyzer {
	class HandlerCacheHelper {
		using Logger = common::rainmeter::Logger;
		using Rainmeter = common::rainmeter::Rainmeter;
		using OptionMap = common::options::OptionMap;

		struct HandlerRawInfo {
			bool updated = false;
			PatchInfo patchInfo;
		};

		using PatchersMap = std::map<istring, HandlerRawInfo, std::less<>>;
		PatchersMap patchersCache;
		Rainmeter rain;
		bool anythingChanged = false;
		bool unusedOptionsWarning = false;
		Version version{};

	public:
		void setRain(Rainmeter value) {
			rain = std::move(value);
		}

		void setUnusedOptionsWarning(bool value) {
			unusedOptionsWarning = value;
		}

		void setVersion(Version value) {
			version = value;
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
		PatchInfo* getHandler(const istring& name, Logger& cl);

	private:
		[[nodiscard]]
		HandlerRawInfo parseHandler(sview name, HandlerRawInfo handler, Logger& cl);

		[[nodiscard]]
		PatchInfo createHandlerPatcher(const OptionMap& optionMap, Logger& cl) const;

		template<typename T>
		[[nodiscard]]
		PatchInfo createPatcherT(const OptionMap& om, Logger& cl) const {
			T instance{};
			SoundHandlerBase& ref = instance;
			SoundHandlerBase::ParseResult parseResult = ref.parseParams(om, cl, rain, version);

			if (!parseResult.valid) {
				return {};
			}

			PatchInfo result;
			result.params = std::move(parseResult.params);
			result.fun = SoundHandlerBase::patchHandlerImpl<T>;
			result.sources = std::move(parseResult.sources);
			result.externalMethods = parseResult.externalMethods;
			return result;
		}

		void readRawDescription2(isview type, const OptionMap& optionMap, string& rawDescription2) const;
	};
}
