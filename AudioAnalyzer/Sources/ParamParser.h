/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <string>
#include <map>
#include "Channel.h"
#include <vector>
#include <functional>
#include "sound-handlers/SoundHandler.h"
#include <set>
#include "RainmeterWrappers.h"
#include "OptionParser.h"

namespace rxaa {
	class ParamParser {
	private:
		rxu::Rainmeter& rain;
		rxu::Rainmeter::Logger& log;

		std::map<Channel, std::vector<std::wstring>> handlers;
		std::map<std::wstring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> handlerPatchersMap;

	public:
		explicit ParamParser(rxu::Rainmeter& rain);

		~ParamParser() = default;
		/** This class is non copyable */
		ParamParser(const ParamParser& other) = delete;
		ParamParser(ParamParser&& other) = delete;
		ParamParser& operator=(const ParamParser& other) = delete;
		ParamParser& operator=(ParamParser&& other) = delete;

		void parse();
		const std::map<Channel, std::vector<std::wstring>>& getHandlers() const;
		const std::map<std::wstring, std::function<SoundHandler*(SoundHandler*)>, std::less<>>& getPatches() const;

	private:
		std::set<Channel> parseChannels(rxu::OptionParser::OptionList channelsStringList) const;
		void cacheHandlers(rxu::OptionParser::OptionList indices);
		std::function<SoundHandler*(SoundHandler*)> parseHandler(const rxu::OptionParser::OptionMap& optionMap, rxu::Rainmeter::ContextLogger &cl);

		template<typename T>
		std::function<SoundHandler*(SoundHandler*)> parseHandlerT(const rxu::OptionParser::OptionMap& optionMap, rxu::Rainmeter::ContextLogger &cl) {
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
		std::function<SoundHandler*(SoundHandler*)> parseHandlerT2(const rxu::OptionParser::OptionMap& optionMap, rxu::Rainmeter::ContextLogger &cl) {
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
