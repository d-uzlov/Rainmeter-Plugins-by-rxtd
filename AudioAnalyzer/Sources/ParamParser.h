/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "sound-processing/Channel.h"
#include <functional>
#include "sound-processing/sound-handlers/SoundHandler.h"
#include <set>
#include "RainmeterWrappers.h"
#include "option-parser/OptionMap.h"

namespace rxtd::audio_analyzer {
	class ParamParser {
	private:
		utils::Rainmeter& rain;
		utils::Rainmeter::Logger& log;
		bool unusedOptionsWarning;

		std::map<Channel, std::vector<istring>> handlers;
		std::map<istring, std::function<SoundHandler*(SoundHandler*, Channel)>, std::less<>> handlerPatchersMap;

	public:
		explicit ParamParser(utils::Rainmeter& rain, bool unusedOptionsWarning);

		~ParamParser() = default;
		/** This class is non copyable */
		ParamParser(const ParamParser& other) = delete;
		ParamParser(ParamParser&& other) = delete;
		ParamParser& operator=(const ParamParser& other) = delete;
		ParamParser& operator=(ParamParser&& other) = delete;

		void parse();
		const std::map<Channel, std::vector<istring>>& getHandlers() const;
		const std::map<istring, std::function<SoundHandler*(SoundHandler*, Channel)>, std::less<>>& getPatches() const;

	private:
		static bool checkListUnique(const utils::OptionList& list);
		std::set<Channel> parseChannels(utils::OptionList channelsStringList) const;
		void cacheHandlers(const utils::OptionList& indices);
		std::function<SoundHandler*(SoundHandler*, Channel)> parseHandler(const utils::OptionMap& optionMap, utils::Rainmeter::Logger &cl);

		template<typename T>
		std::function<SoundHandler*(SoundHandler*, Channel)> parseHandlerT(const utils::OptionMap& optionMap, utils::Rainmeter::Logger &cl) {
			auto paramsOpt = T::parseParams(optionMap, cl);
			if (!paramsOpt.has_value()) {
				return nullptr;
			}

			return[params = paramsOpt.value()](SoundHandler *old, Channel channel) {
				auto *handler = dynamic_cast<T*>(old);
				if (handler == nullptr) {
					handler = new T();
				}

				handler->setParams(params, channel);

				return handler;
			};
		}
		template<typename T>
		std::function<SoundHandler*(SoundHandler*, Channel)> parseHandlerT2(const utils::OptionMap& optionMap, utils::Rainmeter::Logger &cl) {
			auto paramsOpt = T::parseParams(optionMap, cl, rain);
			if (!paramsOpt.has_value()) {
				return nullptr;
			}

			return[params = paramsOpt.value()](SoundHandler *old, Channel channel) {
				auto *handler = dynamic_cast<T*>(old);
				if (handler == nullptr) {
					handler = new T();
				}

				handler->setParams(params, channel);

				return handler;
			};
		}
	};
}
