/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <utility>
#include "Channel.h"
#include "AudioChildHelper.h"
#include "ChannelProcessingHelper.h"
#include "../ParamParser.h"
#include <chrono>

namespace rxtd::audio_analyzer {

	class DataSupplierImpl : public DataSupplier {
		array_view<float> wave { };

	public:
		mutable utils::Rainmeter::Logger logger;

		void setWave(array_view<float> value) {
			wave = value;
		}

		array_view<float> getWave() const override {
			return wave;
		}

		void _log(const wchar_t* message) const override {
			logger.error(message);
		}
	};

	class SoundAnalyzer {
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		using ChannelData = std::map<istring, std::unique_ptr<SoundHandler>, std::less<>>;
		using Logger = utils::Rainmeter::Logger;

		// Following two fields are used for updating .channels field.
		// They can contain info about handlers that doesn't exist because of channel layout
		std::set<Channel> channelSetRequested;
		const ParamParser::HandlerPatcherMap *handlerPatchers = nullptr;
		std::vector<istring> handlerOrder;

		std::map<Channel, ChannelData> channels;
		DataSupplierImpl dataSupplier;

		ChannelProcessingHelper cph;
		Logger logger;

		index sourceSampleRate{ };
		double granularity{ };
		ChannelLayout layout;

		
		class HandlerFinderImpl : public HandlerFinder {
			const ChannelData& channelData;

		public:
			explicit HandlerFinderImpl(const ChannelData& channelData) : channelData(channelData) { }

			[[nodiscard]]
			SoundHandler* getHandler(isview id) const override {
				const auto iter = channelData.find(id);
				return iter == channelData.end() ? nullptr : iter->second.get();
			}
		};
		
	public:
		SoundAnalyzer() = default;

		SoundAnalyzer(Logger logger) noexcept : logger(std::move(logger)) {
		}

		~SoundAnalyzer() = default;
		/** This class is non copyable */
		SoundAnalyzer(const SoundAnalyzer& other) = delete;
		SoundAnalyzer(SoundAnalyzer&& other) = default;
		SoundAnalyzer& operator=(const SoundAnalyzer& other) = delete;
		SoundAnalyzer& operator=(SoundAnalyzer&& other) = default;

		// depends on system format only
		void setFormat(index sampleRate, ChannelLayout layout);

		[[nodiscard]]
		AudioChildHelper getAudioChildHelper() const {
			return AudioChildHelper { channels };
		}

		/**
		 * Handlers aren't completely recreated when measure is reloaded.
		 * Instead, they are patched. Patches are created in ParamParser class.
		 * Patch is a function that takes old handler as a parameter and returns new handler.
		 * This new handler may be completely new if it didn't exist before or if class of handler with this name changed,
		 * but usually this is the same handler with updated parameters.
		 */
		 // depends on options only
		void setParams(std::set<Channel> channelSetRequested, const ParamParser::HandlerPatcherInfo &patchersInfo, double granularity);

		[[nodiscard]]
		ChannelProcessingHelper& getCPH() {
			return cph;
		}

		// returns true when killed on timeout
		bool process(const ChannelMixer& mixer, clock::time_point killTime);
		// returns true when killed on timeout
		bool finishStandalone(clock::time_point killTime);
		void resetValues() noexcept;

	private:
		void patchCH() {
			patchChannels();
			patchHandlers();
		}

		void patchChannels();
		void patchHandlers();
	};
}
