// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "HandlerCacheHelper.h"
#include "ProcessingData.h"
#include "rxtd/Logger.h"
#include "rxtd/audio_analyzer/sound_processing/Channel.h"
#include "rxtd/rainmeter/Rainmeter.h"

namespace rxtd::audio_analyzer::options {
	class ParamParser {
		using Rainmeter = rainmeter::Rainmeter;
		using OptionMap = option_parsing::OptionMap;
		using OptionList = option_parsing::OptionList;
		using OptionSequence = option_parsing::OptionSequence;
		using Parser = option_parsing::OptionParser;

	public:
		class InvalidOptionsException : public std::runtime_error {
		public:
			explicit InvalidOptionsException() : runtime_error("") {}
		};

		using ProcessingsInfoMap = std::map<istring, ProcessingData, std::less<>>;

	private:
		Rainmeter rain;

		bool unusedOptionsWarning = true;
		index defaultTargetRate = 44100;
		ProcessingsInfoMap parseResult;
		Version version{};
		HandlerCacheHelper hch;
		mutable std::set<istring> handlerNames;
		mutable Parser parser = option_parsing::OptionParser::getDefault();

	public:
		void setRainmeter(Rainmeter value) {
			rain = std::move(value);
			hch.setRain(rain);
			parser.setLogger(rain.createLogger());
			hch.setParser(parser);
		}

		// return true if there were any changes since last update, false if there were none
		// can throw InvalidOptionsException
		bool readOptions(Version version, bool suppressLogger);

		[[nodiscard]]
		const auto& getParseResult() const {
			return parseResult;
		}

		[[nodiscard]]
		Version getVersion() const {
			return version;
		}

		auto& getParser() const {
			return parser;
		}

	private:
		// returns true when something changed, false otherwise
		// can throw InvalidOptionsException
		[[nodiscard]]
		bool parseFilter(const OptionMap& optionMap, ProcessingData::FilterInfo& fi, Logger& cl) const;

		// returns true when something changed, false otherwise
		// can throw InvalidOptionsException
		[[nodiscard]]
		bool parseTargetRate(const OptionMap& optionMap, index& rate, Logger& cl) const;

		[[nodiscard]]
		static bool checkListUnique(const OptionList& list);

		[[nodiscard]]
		static bool checkNameAllowed(sview name);

		// returns true when something changed, false otherwise
		// can throw InvalidOptionsException
		[[nodiscard]]
		bool parseProcessing(sview name, Logger cl, ProcessingData& data) const;

		[[nodiscard]]
		std::vector<Channel> parseChannels(const OptionList& channelsStringList, Logger& logger) const;

		// returns true when something changed, false otherwise
		// can throw InvalidOptionsException
		[[nodiscard]]
		bool parseHandlers(const OptionSequence& names, ProcessingData& data, const Logger& cl) const;
	};
}
