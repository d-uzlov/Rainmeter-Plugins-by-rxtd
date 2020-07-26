/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "sound-handlers/SoundHandler.h"
#include <functional>
#include "Channel.h"
#include "AudioChildHelper.h"
#include "HelperClasses.h"
#include "ChannelProcessingHelper.h"

namespace rxtd::audio_analyzer {
	class SoundAnalyzer {
		// Following two fields are used for updating .channels field.
		// They can contain info about handlers that doesn't exist because of channel layout
		std::map<Channel, std::vector<istring>> orderOfHandlers;
		std::map<istring, std::function<SoundHandler*(SoundHandler*, Channel)>, std::less<>> patchers;

		ChannelProcessingHelper cph;
		AudioChildHelper audioChildHelper{ };

		std::map<Channel, ChannelData> channels;

		mutable DataSupplierImpl dataSupplier;

		utils::Rainmeter::Logger logger;

		index sampleRate{ };
		ChannelLayout layout;

	public:
		SoundAnalyzer() = default;

		SoundAnalyzer(const ChannelMixer& channelMixer) noexcept :
			cph(channelMixer), audioChildHelper(channels, dataSupplier) {
		}

		~SoundAnalyzer() = default;
		/** This class is non copyable */
		SoundAnalyzer(const SoundAnalyzer& other) = delete;
		SoundAnalyzer(SoundAnalyzer&& other) = delete;
		SoundAnalyzer& operator=(const SoundAnalyzer& other) = delete;
		SoundAnalyzer& operator=(SoundAnalyzer&& other) = delete;

		void setLayout(ChannelLayout layout);
		void setSourceRate(index value);

		void setLogger(utils::Rainmeter::Logger logger) {
			this->logger = logger;
		}

		AudioChildHelper getAudioChildHelper() const;

		/**
		 * Handlers aren't completely recreated when measure is reloaded.
		 * Instead, they are patched. Patches are created in ParamParser class.
		 * Patch is a function that takes old handler as a parameter and returns new handler.
		 * This new handler may be completely new if it didn't exist before or if class of handler with this name changed,
		 * but usually this is the same handler with updated parameters.
		 */
		void setHandlerPatchers(
			std::map<Channel, std::vector<istring>> handlersOrder,
			std::map<istring, std::function<SoundHandler*(SoundHandler*, Channel)>, std::less<>> patchers
		);

		ChannelProcessingHelper& getCPH() {
			return cph;
		}

		void process(bool isSilent);
		void resetValues() noexcept;
		void finishStandalone() noexcept;

		bool needChannelAuto() const {
			const auto iter = channels.find(Channel::eAUTO);
			if (iter == channels.end()) {
				return false;
			}
			return !iter->second.handlers.empty();
		}

	private:
		void updateHandlerSampleRate() noexcept;
		void removeNonexistentChannelsFromMap();
		void patchHandlers();
	};
}
