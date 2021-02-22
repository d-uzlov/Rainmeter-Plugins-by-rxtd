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
#include "ParentHelper.h"
#include "options/ParamParser.h"
#include "rainmeter/MeasureBase.h"

namespace rxtd::audio_analyzer {
	class AudioParent : public utils::ParentMeasureBase {
		using DeviceRequest = std::optional<CaptureManager::SourceDesc>;

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
			LogErrorHelper<istring, Channel> processingDoesNotHaveChannel;
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

		std::map<istring, ProcessingManager, std::less<>> clearProcessings;
		ProcessingOrchestrator::Snapshot clearSnapshot;

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

		bool checkHandlerShouldExist(isview procName, Channel channel, isview handlerName) const;

		isview findProcessingFor(isview handlerName) const;

	private:
		void initLogHelpers();

		void runFinisher(
			handler::ExternalMethods::FinishMethodType finisher,
			const handler::ExternalData& handlerData,
			isview procName, Channel channel, isview handlerName
		) const;

		DeviceRequest readRequest() const;
		void resolveProp(
			string& resolveBufferString,
			isview procName, Channel channel, isview handlerName, isview propName
		);

		void runFinishers(ProcessingOrchestrator::Snapshot snapshot) const;
	};
}
