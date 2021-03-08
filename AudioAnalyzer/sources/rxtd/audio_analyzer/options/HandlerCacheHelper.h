// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "HandlerInfo.h"
#include "rxtd/Logger.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/HandlerBase.h"
#include "rxtd/rainmeter/Rainmeter.h"

namespace rxtd::audio_analyzer::options {
	class HandlerCacheHelper {
	public:
		using Rainmeter = rainmeter::Rainmeter;
		using OptionMap = option_parsing::OptionMap;

		struct MapValue {
			HandlerInfo info;
			bool valid = false;
			bool changed = false;
			bool updated = false;
		};

	private:
		using PatchersMap = std::map<istring, MapValue, std::less<>>;

		mutable PatchersMap patchersCache;
		Rainmeter rain;
		bool unusedOptionsWarning = false;
		Version version{};
		option_parsing::OptionParser* parserPtr = nullptr;

	public:
		void setParser(option_parsing::OptionParser& parser) {
			parserPtr = &parser;
		}

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
				info.changed = false;
			}
		}

		[[nodiscard]]
		const MapValue& getHandlerInfo(const istring& name, isview source, Logger& cl) const;

	private:
		[[nodiscard]]
		// returns true when something has changed, false otherwise
		bool parseHandler(sview name, isview source, HandlerInfo& val, Logger& cl) const;

		/// <summary>
		/// Can throw HandlerBase::InvalidOptionsException.
		/// </summary>
		/// <param name="optionMap"></param>
		/// <param name="cl"></param>
		/// <returns></returns>
		[[nodiscard]]
		handler::HandlerBase::HandlerMetaInfo createHandlerPatcher(isview type, const OptionMap& optionMap, Logger& cl) const noexcept(false);
	};
}
