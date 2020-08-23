/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <chrono>

#include "Channel.h"
#include "AudioChildHelper.h"
#include "ChannelProcessingHelper.h"
#include "../ParamParser.h"
#include "ChannelMixer.h"

namespace rxtd::audio_analyzer {
	class ProcessingManager {
	public:
		using ChannelSnapshot = std::map<istring, SoundHandler::Snapshot, std::less<>>;
		using Snapshot = std::map<Channel, ChannelSnapshot, std::less<>>;

	private:
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		using ChannelData = std::map<istring, std::unique_ptr<SoundHandler>, std::less<>>;
		using Logger = utils::Rainmeter::Logger;

		// The following two fields are used for updating .channels field.
		// They can contain info about handlers that doesn't exist because of channel layout
		std::set<Channel> channelSetRequested;
		ParamParser::HandlerPatchersInfo patchersInfo;
		std::vector<istring> realOrder;

		std::map<Channel, ChannelData> channels;

		ChannelProcessingHelper cph;
		Logger logger;

		index legacyNumber = 0;

	public:
		void setLogger(Logger value) {
			logger = std::move(value);
		}

		// depends on system format only
		void updateFormat(index sampleRate, ChannelLayout layout);

		[[nodiscard]]
		AudioChildHelper getAudioChildHelper() const {
			return AudioChildHelper{ channels };
		}

		void updateSnapshot(Snapshot& snapshot);

		// todo update documentation
		/**
		 * Handlers aren't completely recreated when measure is reloaded.
		 * Instead, they are patched. Patches are created in ParamParser class.
		 * Patch is a function that takes old handler as a parameter and returns new handler.
		 * This new handler may be completely new if it didn't exist before or if class of handler with this name changed,
		 * but usually this is the same handler with updated parameters.
		 */
		// depends on options only
		void setParams(
			const ParamParser::ProcessingData& pd,
			index _legacyNumber,
			index sampleRate, ChannelLayout layout
		);

		// returns true when killed on timeout
		bool process(const ChannelMixer& mixer, clock::time_point killTime);
		// returns true when killed on timeout
		bool finishStandalone(clock::time_point killTime);
		void resetValues() noexcept;
		void configureSnapshot(Snapshot& snapshot);

	private:
		void patchHandlers(ChannelLayout layout);
	};
}
