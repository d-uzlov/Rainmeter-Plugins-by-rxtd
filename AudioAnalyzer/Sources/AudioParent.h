/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "TypeHolder.h"
#include "RainmeterWrappers.h"
#include "sound-processing/SoundAnalyzer.h"
#include "sound-processing/device-management/DeviceManager.h"

namespace rxtd::audio_analyzer {
	class AudioParent : public utils::ParentBase {
		SoundAnalyzer soundAnalyzer;
		DeviceManager deviceManager;

	public:
		explicit AudioParent(utils::Rainmeter&& rain);
		~AudioParent() = default;

		/** This class is non copyable */
		AudioParent(const AudioParent& other) = delete;
		AudioParent(AudioParent&& other) = delete;
		AudioParent& operator=(const AudioParent& other) = delete;
		AudioParent& operator=(AudioParent&& other) = delete;

	protected:
		void _reload() override;
		double _update() override;
		void _command(isview bangArgs) override;
		void _resolve(int argc, const wchar_t* argv[], string& resolveBufferString) override;

	public:
		double getValue(sview id, Channel channel, index index) const;
	};
}
