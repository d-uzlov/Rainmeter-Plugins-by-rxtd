/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "device-management/MyWaveFormat.h"
#include "sound-handlers/SoundHandler.h"
#include <functional>
#include "Channel.h"
#include <Vector2D.h>
#include "Resampler.h"
#include "ChannelMixer.h"
#include "AudioChildHelper.h"
#include "HelperClasses.h"

namespace rxtd::audio_analyzer {
	class SoundAnalyzer {
	public:
	private:
		MyWaveFormat waveFormat;

		std::map<Channel, std::vector<istring>> orderOfHandlers;
		std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> patchers;

		Resampler resampler;
		ChannelMixer channelMixer;
		AudioChildHelper audioChildHelper;

		utils::Vector2D<float> wave;

		std::map<Channel, ChannelData> channels;

		mutable DataSupplierImpl dataSupplier;

	public:

		SoundAnalyzer() noexcept;

		~SoundAnalyzer() = default;
		/** This class is non copyable */
		SoundAnalyzer(const SoundAnalyzer& other) = delete;
		SoundAnalyzer(SoundAnalyzer&& other) = delete;
		SoundAnalyzer& operator=(const SoundAnalyzer& other) = delete;
		SoundAnalyzer& operator=(SoundAnalyzer&& other) = delete;

		void setTargetRate(index value) noexcept;

		AudioChildHelper getAudioChildHelper() const;

		/**
		 * Handlers aren't completely recreated when measure is reloaded.
		 * Instead, they are patched. Patches are created in ParamParser class.
		 * Patch is a function that takes old handler as a parameter and returns new handler.
		 * This new handler may be completely new if it didn't exist before or if class of handler with this name changed,
		 * but usually this is the same handler with updated parameters.
		 */
		void patchHandlers(std::map<Channel, std::vector<istring>> handlersOrder, std::map<istring, std::function<SoundHandler*(SoundHandler*)>, std::less<>> handlerPatchersMap) noexcept;

		void setWaveFormat(MyWaveFormat waveFormat) noexcept;

		void process(const uint8_t* buffer, bool isSilent, index framesCount) noexcept;
		void resetValues() noexcept;
		void finish() noexcept;

	private:
		void updateSampleRate() noexcept;
		index createChannelAuto(index framesCount) noexcept;
	};
}
