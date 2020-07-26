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
	class ParamParser {
	public:
		using Patcher = std::function<SoundHandler*(SoundHandler*, Channel)>;

		struct HandlerInfo {
			istring name;
			Patcher patcher;
		};

		struct ProcessingData {
			index targetRate;
			audio_utils::FilterCascadeCreator fcc;
			std::set<Channel> channels;
			std::vector<HandlerInfo> handlerInfo;
		};

	private:
		utils::Rainmeter& rain;
		utils::Rainmeter::Logger& log;
		bool unusedOptionsWarning;
		index defaultTargetRate{ };

	public:
		explicit ParamParser(utils::Rainmeter& rain, bool unusedOptionsWarning);

		~ParamParser() = default;
		/** This class is non copyable */
		ParamParser(const ParamParser& other) = delete;
		ParamParser(ParamParser&& other) = delete;
		ParamParser& operator=(const ParamParser& other) = delete;
		ParamParser& operator=(ParamParser&& other) = delete;

		void setTargetRate(index value) {
			defaultTargetRate = value;
		}

		std::vector<ProcessingData> parse();

	private:
		static bool checkListUnique(const utils::OptionList& list);

		std::optional<ProcessingData> parseProcessing(sview name);
		std::set<Channel> parseChannels(utils::OptionList channelsStringList) const;
		std::vector<HandlerInfo> parseHandlers(const utils::OptionList& indices);

		Patcher parseHandler(sview name, array_view<HandlerInfo> prevHandlers);
		Patcher getHandlerPatcher(
			const utils::OptionMap& optionMap,
			utils::Rainmeter::Logger& cl,
			array_view<HandlerInfo> prevHandlers
		);

		template <typename T>
		Patcher parseHandlerT(
			const utils::OptionMap& optionMap,
			utils::Rainmeter::Logger& cl
		) {
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
		Patcher parseHandlerT2(
			const utils::OptionMap& optionMap,
			utils::Rainmeter::Logger& cl
		) {
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
