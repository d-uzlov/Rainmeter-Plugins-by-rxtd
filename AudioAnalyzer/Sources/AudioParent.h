/*
 * Copyright (C) 2019-2020 rxtd
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

		index legacyNumber{};
		ParamParser paramParser;

		struct {
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

			void setLogger(utils::Rainmeter::Logger logger) {
				generic.setLogger(logger);
				sourceTypeIsNotRecognized.setLogger(logger);
				unknownCommand.setLogger(logger);
				currentDeviceUnknownProp.setLogger(logger);
				unknownSectionVariable.setLogger(logger);
				legacy_invalidPort.setLogger(logger);
				processingNotFound.setLogger(logger);
				channelNotRecognized.setLogger(logger);
				noProcessingHaveHandler.setLogger(logger);
				processingDoesNotHaveHandler.setLogger(logger);
				processingDoesNotHaveChannel.setLogger(logger);
				handlerDoesNotHaveProps.setLogger(logger);
				propNotFound.setLogger(logger);
			}

			void reset() {
				// helpers that are commented don't need to be reset
				// because it doesn't make sense to repeat their messages after measure is reloaded

				// generic.reset();
				// sourceTypeIsNotRecognized.reset();
				// unknownCommand.reset();
				// currentDeviceUnknownProp.reset();
				// unknownSectionVariable.reset();
				// legacy_invalidPort.reset();

				processingNotFound.reset();
				channelNotRecognized.reset();
				noProcessingHaveHandler.reset();
				processingDoesNotHaveHandler.reset();
				processingDoesNotHaveChannel.reset();
				// handlerDoesNotHaveProps.reset();
				// propNotFound.reset();
			}
		} logHelpers;

		DeviceRequest requestedSource;
		ParentHelper::Callbacks callbacks;
		ParentHelper helper;

		struct CleanerData {
			SoundHandler::ExternalData data;
			SoundHandler::ExternalMethods::FinishMethodType finisher = nullptr;
		};

		// handlerName → handlerData for cleanup
		using ProcessingCleanersMap = std::map<istring, CleanerData, std::less<>>;
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
		double getValue(isview proc, isview id, Channel channel, index ind);

		index getLegacyNumber() const {
			return paramParser.getLegacyNumber();
		}

		bool isHandlerShouldExist(isview procName, Channel channel, isview handlerName) const;

		isview findProcessingFor(isview handlerName) const;

	private:
		void runFinisher(
			SoundHandler::ExternalMethods::FinishMethodType finisher,
			const SoundHandler::ExternalData& handlerData,
			isview procName, Channel channel, isview handlerName
		) const {
			SoundHandler::ExternCallContext context;
			context.legacyNumber = legacyNumber;

			context.channelName =
				legacyNumber < 104
				? ChannelUtils::getTechnicalNameLegacy(channel)
				: ChannelUtils::getTechnicalName(channel);

			string filePrefix;
			filePrefix += procName % csView();
			filePrefix += L'-';
			filePrefix += handlerName % csView();
			filePrefix += L'-';
			filePrefix += context.channelName;

			context.filePrefix = filePrefix;

			finisher(handlerData, context);
		}

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
