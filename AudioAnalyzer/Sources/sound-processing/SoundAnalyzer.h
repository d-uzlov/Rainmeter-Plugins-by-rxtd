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
#include "HelperClasses.h"
#include "ChannelProcessingHelper.h"
#include "../ParamParser.h"

namespace rxtd::audio_analyzer {
	class SoundAnalyzer {
		// Following two fields are used for updating .channels field.
		// They can contain info about handlers that doesn't exist because of channel layout
		std::set<Channel> channelSetRequested;
		std::vector<ParamParser::HandlerInfo> handlerPatchers;

		std::map<Channel, ChannelData> channels;
		DataSupplierImpl dataSupplier;

		ChannelProcessingHelper cph;
		utils::Rainmeter::Logger logger;
		AudioChildHelper audioChildHelper{ };

		index sampleRate{ };
		double granularity{ };
		ChannelLayout layout;

	public:
		SoundAnalyzer() = default;

		SoundAnalyzer(const ChannelMixer& channelMixer, utils::Rainmeter::Logger logger) noexcept :
			cph(channelMixer), logger(std::move(logger)), audioChildHelper(channels, dataSupplier) {
		}

		~SoundAnalyzer() = default;
		/** This class is non copyable */
		SoundAnalyzer(const SoundAnalyzer& other) = delete;
		SoundAnalyzer(SoundAnalyzer&& other) = default;
		SoundAnalyzer& operator=(const SoundAnalyzer& other) = delete;
		SoundAnalyzer& operator=(SoundAnalyzer&& other) = default;

		void setLayout(ChannelLayout layout);
		void setSourceRate(index value);
		void setGranularity(double value) {
			granularity = value;
		}

		const AudioChildHelper& getAudioChildHelper() const;

		/**
		 * Handlers aren't completely recreated when measure is reloaded.
		 * Instead, they are patched. Patches are created in ParamParser class.
		 * Patch is a function that takes old handler as a parameter and returns new handler.
		 * This new handler may be completely new if it didn't exist before or if class of handler with this name changed,
		 * but usually this is the same handler with updated parameters.
		 */
		void setHandlers(
			std::set<Channel> channelSetRequested,
			std::vector<ParamParser::HandlerInfo> handlerPatchers
		);

		ChannelProcessingHelper& getCPH() {
			return cph;
		}

		void process();
		void resetValues() noexcept;
		void finishStandalone() noexcept;

		bool needChannelAuto() const {
			const auto iter = channels.find(Channel::eAUTO);
			if (iter == channels.end()) {
				return false;
			}
			return !iter->second.empty();
		}

	private:
		void patch() {
			patchChannels();
			patchHandlers();
		}

		void updateHandlerSampleRate() noexcept;
		void patchChannels();
		void patchHandlers();
	};
}
