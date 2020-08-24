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
#include "sound-processing/ProcessingManager.h"
#include "sound-processing/device-management/DeviceManager.h"
#include "sound-processing/ChannelProcessingHelper.h"
#include "sound-processing/ProcessingOrchestrator.h"
#include "windows-wrappers/IMMNotificationClientImpl.h"

namespace rxtd::audio_analyzer {
	class AudioParent : public utils::ParentBase {
		ParamParser paramParser;
		ChannelMixer channelMixer;
		DeviceManager deviceManager;

		utils::GenericComWrapper<utils::CMMNotificationClient> notificationClient;
		ProcessingOrchestrator orchestrator;
		ProcessingOrchestrator::DataSnapshot dataSnapshot;
		DeviceManager::DeviceInfoSnapshot diSnapshot;
		MyWaveFormat currentFormat;
		AudioEnumeratorHelper enumerator;

		index legacyNumber{ };

		struct RequestedDeviceDescription {
			DeviceManager::DataSource sourceType{ };
			string id;

			friend bool operator==(const RequestedDeviceDescription& lhs, const RequestedDeviceDescription& rhs) {
				return lhs.sourceType == rhs.sourceType
					&& lhs.id == rhs.id;
			}

			friend bool operator!=(const RequestedDeviceDescription& lhs, const RequestedDeviceDescription& rhs) {
				return !(lhs == rhs);
			}
		} requestedSource;

	public:
		explicit AudioParent(utils::Rainmeter&& rain);
		~AudioParent() = default;

		/** This class is non copyable */
		AudioParent(const AudioParent& other) = delete;
		AudioParent(AudioParent&& other) = delete;
		AudioParent& operator=(const AudioParent& other) = delete;
		AudioParent& operator=(AudioParent&& other) = delete;

	protected:
		void vReload() override;
		double vUpdate() override;
		void vCommand(isview bangArgs) override;
		void vResolve(array_view<isview> args, string& resolveBufferString) override;

	public:
		[[nodiscard]]
		double getValue(isview proc, isview id, Channel channel, index ind) const;

		index getLegacyNumber() const {
			return paramParser.getLegacyNumber();
		}

		// returns error message or empty string
		string checkHandler(isview procName, Channel channel, isview handlerName) const;

		isview legacy_findProcessingFor(isview handlerName);

	private:
		RequestedDeviceDescription readRequest() const;
		void resolveProp(array_view<isview> args, string& resolveBufferString);
	};
}
