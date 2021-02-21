/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "HandlerCacheHelper.h"
#include "HandlerPatchersInfo.h"
#include "ProcessingData.h"
#include "audio-utils/filter-utils/FilterCascadeParser.h"
#include "rainmeter/Logger.h"
#include "rainmeter/Rainmeter.h"
#include "sound-processing/Channel.h"
#include "sound-processing/sound-handlers/SoundHandlerBase.h"

namespace rxtd::audio_analyzer {
	class ParamParser {
		using Logger = common::rainmeter::Logger;
		using Rainmeter = common::rainmeter::Rainmeter;
		using OptionMap = common::options::OptionMap;
		using OptionList = common::options::OptionList;

	public:
		using ProcessingsInfoMap = std::map<istring, ProcessingData, std::less<>>;

	private:
		Rainmeter rain;

		bool unusedOptionsWarning = true;
		index defaultTargetRate = 44100;
		ProcessingsInfoMap parseResult;
		Version version{};
		HandlerCacheHelper hch;
		std::set<istring> handlerNames;

		mutable bool anythingChanged = false;

	public:
		void setRainmeter(Rainmeter value) {
			rain = std::move(value);
			hch.setRain(rain);
		}

		// return true if there were any changes since last update, false if there were none
		bool parse(Version version, bool suppressLogger);

		[[nodiscard]]
		const auto& getParseResult() const {
			return parseResult;
		}

		[[nodiscard]]
		Version getVersion() const {
			return version;
		}

	private:
		void parseFilters(const OptionMap& optionMap, ProcessingData& data, Logger& cl) const;
		void parseTargetRate(const OptionMap& optionMap, ProcessingData& data, Logger& cl) const;

		[[nodiscard]]
		static bool checkListUnique(const OptionList& list);

		[[nodiscard]]
		static bool checkNameAllowed(sview name);

		void parseProcessing(sview name, Logger cl, ProcessingData& oldData);

		[[nodiscard]]
		std::set<Channel> parseChannels(const OptionList& channelsStringList, Logger& logger) const;

		[[nodiscard]]
		std::optional<HandlerPatchersInfo> parseHandlers(const OptionList& indices, const Logger& logger);
	};
}
