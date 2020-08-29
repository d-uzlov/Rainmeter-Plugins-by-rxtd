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
#include "ParentHelper.h"

namespace rxtd::audio_analyzer {
	class AudioParent : public utils::ParentBase {
		using DeviceRequest = std::optional<ParentHelper::RequestedDeviceDescription>;

		index legacyNumber{ };
		ParamParser paramParser;

		DeviceRequest requestedSource;
		ParentHelper helper;
		ParentHelper::Snapshot snapshot;

		bool firstReload = true;

		// handlerName → handlerData for cleanup
		// it's necessary to hold identical blank data for each channel
		// because this data also contains channel name, which is required for writing files
		using ChannelCleanersMap = std::map<istring, std::any, std::less<>>;
		using ProcessingCleanersMap = std::map<Channel, ChannelCleanersMap, std::less<>>;
		using CleanersMap = std::map<istring, ProcessingCleanersMap, std::less<>>;

		bool cleanersExecuted = false;
		CleanersMap cleanersMap;

	public:
		explicit AudioParent(utils::Rainmeter&& rain);

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

		isview legacy_findProcessingFor(isview handlerName) const;

	private:
		DeviceRequest readRequest() const;
		void resolveProp(array_view<isview> args, string& resolveBufferString);
		ProcessingCleanersMap createCleanersFor(const ParamParser::ProcessingData& pd) const;
		void runCleaners() const;
	};
}
