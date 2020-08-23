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
	using HandlerPatchingFun = std::unique_ptr<SoundHandler>(*)(std::unique_ptr<SoundHandler> old);

	struct PatchInfo {
		std::any params;
		std::vector<istring> sources;
		HandlerPatchingFun fun = nullptr;
		SoundHandler::Finisher finisher = nullptr;
	};

	class HandlerCacheHelper {
		using Logger = utils::Rainmeter::Logger;
		using Rainmeter = utils::Rainmeter;

		struct HandlerRawInfo {
			bool updated = false;
			string rawDescription;
			string rawDescription2;
			PatchInfo patchInfo;
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
		PatchInfo* getHandler(const istring& name);

	private:
		[[nodiscard]]
		HandlerRawInfo parseHandler(sview name, HandlerRawInfo handler);

		[[nodiscard]]
		PatchInfo createHandlerPatcher(const utils::OptionMap& optionMap, Logger& cl) const;

		template <typename T>
		[[nodiscard]]
		PatchInfo createPatcherT(const utils::OptionMap& om, Logger& cl) const {
			T instance{ };
			SoundHandler& ref = instance;
			SoundHandler::ParseResult parseResult = ref.parseParams(om, cl, rain, legacyNumber);

			if (!parseResult.isValid()) {
				return { };
			}

			PatchInfo result;
			result.params = parseResult.takeParams();
			result.fun = SoundHandler::patchHandlerImpl<T>;
			result.sources = parseResult.takeSources();
			result.finisher = parseResult.takeFinisher();
			return result;
		}

		void readRawDescription2(isview type, const utils::OptionMap& optionMap, string& rawDescription2) const;
	};
}
