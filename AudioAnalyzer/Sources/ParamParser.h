/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Channel.h"
#include <functional>
#include "sound-handlers/SoundHandler.h"
#include <set>
#include "RainmeterWrappers.h"
#include "OptionParser.h"

namespace rxaa {
	class ParamParser {
	private:
		utils::Rainmeter& rain;
		utils::Rainmeter::Logger& log;

		std::map<Channel, std::vector<istring>> handlers;
		std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> handlerPatchersMap;

	public:
		explicit ParamParser(utils::Rainmeter& rain);

		~ParamParser() = default;
		/** This class is non copyable */
		ParamParser(const ParamParser& other) = delete;
		ParamParser(ParamParser&& other) = delete;
		ParamParser& operator=(const ParamParser& other) = delete;
		ParamParser& operator=(ParamParser&& other) = delete;

		void parse();
		const std::map<Channel, std::vector<istring>>& getHandlers() const;
		const std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>>& getPatches() const;

	private:
		std::set<Channel> parseChannels(utils::OptionParser::OptionList channelsStringList) const;
		void cacheHandlers(utils::OptionParser::OptionList indices);
		std::function<SoundHandler*(SoundHandler*)> parseHandler(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl);

		template<typename T>
		std::function<SoundHandler*(SoundHandler*)> parseHandlerT(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl) {
			auto paramsOpt = T::parseParams(optionMap, cl);
			if (!paramsOpt.has_value()) {
				return nullptr;
			}

			return[params = paramsOpt.value()](SoundHandler *old) {
				auto *handler = dynamic_cast<T*>(old);
				if (handler == nullptr) {
					handler = new T();
				}

				handler->setParams(params);

				return handler;
			};
		}
		template<typename T>
		std::function<SoundHandler*(SoundHandler*)> parseHandlerT2(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl) {
			auto paramsOpt = T::parseParams(optionMap, cl, rain);
			if (!paramsOpt.has_value()) {
				return nullptr;
			}

			return[params = paramsOpt.value()](SoundHandler *old) {
				auto *handler = dynamic_cast<T*>(old);
				if (handler == nullptr) {
					handler = new T();
				}

				handler->setParams(params);

				return handler;
			};
		}
	};
}
