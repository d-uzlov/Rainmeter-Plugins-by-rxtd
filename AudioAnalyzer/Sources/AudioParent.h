/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "LogErrorHelper.h"
#include "TypeHolder.h"
#include "ParentHelper.h"

namespace rxtd::audio_analyzer {
	class AudioParent : public utils::ParentBase {
		using DeviceRequest = std::optional<CaptureManager::SourceDesc>;

		struct CleanerData {
			SoundHandlerBase::ExternalData data;
			SoundHandlerBase::ExternalMethods::FinishMethodType finisher = nullptr;
		};
		
		using ProcessingCleanersMap = std::map<istring, CleanerData, std::less<>>;
		using CleanersMap = std::map<istring, ProcessingCleanersMap, std::less<>>;

		struct LogHelpers {
			NoArgLogErrorHelper generic;
			LogErrorHelper<istring> sourceTypeIsNotRecognized;
			LogErrorHelper<istring> unknownCommand;
			LogErrorHelper<istring> currentDeviceUnknownProp;
			LogErrorHelper<istring> unknownSectionVariable;
			LogErrorHelper<istring> legacy_invalidPort;

			LogErrorHelper<istring> processingNotFound;
			LogErrorHelper<istring> channelNotRecognized;
			LogErrorHelper<istring> noProcessingHaveHandler;
			LogErrorHelper<istring, istring> processingDoesNotHaveHandler;
			LogErrorHelper<istring, istring> processingDoesNotHaveChannel;
			LogErrorHelper<istring> handlerDoesNotHaveProps;
			LogErrorHelper<istring, istring> propNotFound;

			void setLogger(Logger logger);

			void reset();
		};

		mutable LogHelpers logHelpers;

		Version version{};
		ParamParser paramParser;

		DeviceRequest requestedSource;
		ParentHelper::Callbacks callbacks;
		ParentHelper helper;

		bool cleanersExecuted = false;

		CleanersMap cleanersMap;

	public:
		explicit AudioParent(Rainmeter&& rain);

	protected:
		void vReload() override;
		double vUpdate() override;
		void vCommand(isview bangArgs) override;
		void vResolve(array_view<isview> args, string& resolveBufferString) override;

	public:
		[[nodiscard]]
		double getValue(isview proc, isview id, Channel channel, index ind);

		Version getVersion() const {
			return version;
		}

		bool isHandlerShouldExist(isview procName, Channel channel, isview handlerName) const;

		isview findProcessingFor(isview handlerName) const;

	private:
		void initLogHelpers();

		void runFinisher(
			SoundHandlerBase::ExternalMethods::FinishMethodType finisher,
			const SoundHandlerBase::ExternalData& handlerData,
			isview procName, Channel channel, isview handlerName
		) const;

		void updateCleaners();
		DeviceRequest readRequest() const;
		void resolveProp(
			string& resolveBufferString,
			isview procName, Channel channel, isview handlerName, isview propName
		);
		ProcessingCleanersMap createCleanersFor(const ParamParser::ProcessingData& pd) const;
		void runCleaners() const;
	};
}
