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
#include "sound-processing/ChannelProcessingHelper.h"
#include "windows-wrappers/IMMNotificationClientImpl.h"
#include <mutex>

namespace rxtd::audio_analyzer {
	class AudioParent : public utils::ParentBase {
		ParamParser paramParser;
		ChannelMixer channelMixer;
		DeviceManager deviceManager;
		std::map<istring, SoundAnalyzer, std::less<>> saMap;
		MyWaveFormat currentFormat{ };
		double computeTimeout = 0.0;
		double finishTimeout = 0.0;

		utils::GenericComWrapper<IMMNotificationClient> notificationClient;
		std::mutex notificationClientMutex;
		bool deviceListChanged = false;

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
		void _resolve(array_view<isview> args, string& resolveBufferString) override;

	public:
		double getValue(isview proc, isview id, Channel channel, index ind) const;
		double legacy_getValue(isview id, Channel channel, index ind) const;

	private:
		void patchSA(ParamParser::ProcessingsInfoMap procs);

		template <typename Callable>
		void callAllSA(Callable lambda) {
			for (auto& [name, analyzer] : saMap) {
				lambda(analyzer);
			}
		}

		std::pair<SoundHandler*, AudioChildHelper> findHandlerByName(isview name, Channel channel) const;

		void legacy_resolve(array_view<isview> args, string& resolveBufferString);

		void notificationCallback(sview deviceId) {
			std::lock_guard<std::mutex> lock { notificationClientMutex };
			deviceListChanged = true;
		}

		bool notificationCheck() {
			std::lock_guard<std::mutex> lock { notificationClientMutex };
			const auto result = deviceListChanged;
			deviceListChanged = false;
			return result;
		}
	};
}
