/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "TypeHolder.h"
#include "sound-processing/Channel.h"
#include "sound-processing/SoundAnalyzer.h"
#include "sound-processing/device-management/DeviceManager.h"

namespace rxtd::audio_analyzer {
	using namespace std::string_literals;
	using namespace std::literals::string_view_literals;

	class AudioParent : public utils::TypeHolder {
	private:
		static utils::ParentManager<AudioParent> parentManager;

		SoundAnalyzer soundAnalyzer;
		DeviceManager deviceManager;

	public:
		explicit AudioParent(utils::Rainmeter&& rain);
		~AudioParent();
		/** This class is non copyable */
		AudioParent(const AudioParent& other) = delete;
		AudioParent(AudioParent&& other) = delete;
		AudioParent& operator=(const AudioParent& other) = delete;
		AudioParent& operator=(AudioParent&& other) = delete;

		static AudioParent* findInstance(::rxtd::utils::Rainmeter::Skin skin, isview measureName);

	protected:
		void _reload() override;
		std::tuple<double, const wchar_t*> _update() override;
		void _command(const wchar_t* bangArgs) override;
		const wchar_t* _resolve(int argc, const wchar_t* argv[]) override;

	public:
		double getValue(sview id, Channel channel, index index) const;
	private:
	};


}

