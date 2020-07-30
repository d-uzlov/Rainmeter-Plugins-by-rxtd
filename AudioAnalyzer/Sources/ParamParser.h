/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <set>
#include <functional>

#include "RainmeterWrappers.h"
#include "sound-processing/Channel.h"
#include "sound-processing/sound-handlers/SoundHandler.h"
#include "audio-utils/filter-utils/FilterCascadeParser.h"

namespace rxtd::audio_analyzer {
	class SoundAnalyzer;

	class ParamParser {
		using Logger = utils::Rainmeter::Logger;
		using Rainmeter = utils::Rainmeter;

	public:
		using HandlerPatcher = std::function<SoundHandler*(SoundHandler*, Channel)>;

		struct HandlerInfo {
			string rawDescription;
			string rawDescription2;
			HandlerPatcher patcher;
		};

		struct HandlerPatcherInfo {
			std::map<istring, HandlerInfo, std::less<>> map;
			std::vector<istring> order;
		};

		struct ProcessingData {
			index targetRate{ };
			double granularity{ };
			string rawFccDescription;
			audio_utils::FilterCascadeCreator fcc;
			std::set<Channel> channels;
			HandlerPatcherInfo handlersInfo;
		};

		using ProcessingsInfoMap = std::map<istring, ProcessingData, std::less<>>;

	private:
		Rainmeter rain;
		bool unusedOptionsWarning = true;
		index defaultTargetRate = 44100;
		ProcessingsInfoMap parseResult;
		mutable bool anythingChanged = false;

	public:
		ParamParser() = default;

		~ParamParser() = default;
		/** This class is non copyable */
		ParamParser(const ParamParser& other) = delete;
		ParamParser(ParamParser&& other) = delete;
		ParamParser& operator=(const ParamParser& other) = delete;
		ParamParser& operator=(ParamParser&& other) = delete;

		void setRainmeter(Rainmeter value) {
			rain = std::move(value);
		}

		void parse();

		[[nodiscard]]
		const ProcessingsInfoMap& getParseResult() const {
			return parseResult;
		}

		[[nodiscard]]
		bool isAnythingChanged() const {
			return anythingChanged;
		}

	private:
		[[nodiscard]]
		static bool checkListUnique(const utils::OptionList& list);

		void parseProcessing(sview name, Logger cl, ProcessingData& oldHandlers) const;
		[[nodiscard]]
		std::set<Channel> parseChannels(const utils::OptionList& channelsStringList, Logger& logger) const;
		[[nodiscard]]
		HandlerPatcherInfo parseHandlers(const utils::OptionList& indices, HandlerPatcherInfo oldHandlers) const;

		[[nodiscard]]
		bool parseHandler(sview name, const HandlerPatcherInfo& prevHandlers, HandlerInfo& handler) const;
		[[nodiscard]]
		HandlerPatcher getHandlerPatcher(
			const utils::OptionMap& optionMap,
			Logger& cl,
			const HandlerPatcherInfo& prevHandlers
		) const;

		void readRawDescription2(isview type, const utils::OptionMap& optionMap, string& rawDescription2) const;

		template <typename T>
		[[nodiscard]]
		HandlerPatcher parseHandlerT(const utils::OptionMap& optionMap, Logger& cl) const {
			auto paramsOpt = T::parseParams(optionMap, cl);
			if (!paramsOpt.has_value()) {
				return nullptr;
			}

			return [params = paramsOpt.value()](SoundHandler* old, Channel channel) {
				auto* handler = dynamic_cast<T*>(old);
				if (handler == nullptr) {
					handler = new T();
				}

				handler->setParams(params, channel);

				return handler;
			};
		}

		template <typename T>
		[[nodiscard]]
		HandlerPatcher parseHandlerT2(const utils::OptionMap& optionMap, Logger& cl) const {
			auto paramsOpt = T::parseParams(optionMap, cl, rain);
			if (!paramsOpt.has_value()) {
				return nullptr;
			}

			return [params = paramsOpt.value()](SoundHandler* old, Channel channel) {
				auto* handler = dynamic_cast<T*>(old);
				if (handler == nullptr) {
					handler = new T();
				}

				handler->setParams(params, channel);

				return handler;
			};
		}
	};
}
