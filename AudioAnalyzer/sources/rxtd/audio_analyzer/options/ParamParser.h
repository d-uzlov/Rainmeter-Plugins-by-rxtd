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
#include "ProcessingData.h"
#include "rxtd/rainmeter/Logger.h"
#include "rxtd/rainmeter/Rainmeter.h"
#include "rxtd/audio_analyzer/sound_processing/Channel.h"

namespace rxtd::audio_analyzer::options {
	class ParamParser {
		using Logger = rainmeter::Logger;
		using Rainmeter = rainmeter::Rainmeter;
		using OptionMap = option_parsing::OptionMap;
		using OptionList = option_parsing::OptionList;

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
		void parseFilter(const OptionMap& optionMap, ProcessingData::FilterInfo& fi, Logger& cl) const;
		void parseTargetRate(const OptionMap& optionMap, ProcessingData& data, Logger& cl) const;

		[[nodiscard]]
		static bool checkListUnique(const OptionList& list);

		[[nodiscard]]
		static bool checkNameAllowed(sview name);

		[[nodiscard]]
		ProcessingData parseProcessing(sview name, Logger cl, ProcessingData data);

		[[nodiscard]]
		std::vector<Channel> parseChannels(const OptionList& channelsStringList, Logger& logger) const;

		// returns true on success, false otherwise
		[[nodiscard]]
		bool parseHandlers(const OptionList& names, ProcessingData& data, const Logger& cl);
	};
}
