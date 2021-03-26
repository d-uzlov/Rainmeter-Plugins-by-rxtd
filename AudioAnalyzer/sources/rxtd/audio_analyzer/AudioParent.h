// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

#include "ParentHelper.h"
#include "Version.h"
#include "options/ParamHelper.h"
#include "rxtd/rainmeter/MeasureBase.h"
#include "sound_processing/LogErrorHelper.h"

namespace rxtd::audio_analyzer {
	class AudioParent : public utils::ParentMeasureBase {
		using DeviceRequest = std::optional<CaptureManager::SourceDesc>;

		struct LogHelpers {
			NoArgLogErrorHelper generic;
			LogErrorHelper<istring> sourceTypeIsNotRecognized;
			LogErrorHelper<istring> unknownCommand;
			LogErrorHelper<istring> currentDeviceUnknownProp;
			LogErrorHelper<istring> unknownSectionVariable;

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

		mutable buffer_printer::BufferPrinter bufferPrinter;
		mutable option_parsing::OptionParser parser = option_parsing::OptionParser::getDefault();

		Version version{};
		options::ParamHelper paramHelper;

		DeviceRequest requestedSource;
		ParentHelper::Callbacks callbacks;
		ParentHelper helper;

		bool cleanersExecuted = false;

		std::map<istring, ProcessingManager, std::less<>> clearProcessings;
		ProcessingOrchestrator::Snapshot clearSnapshot;

	public:
		class InvalidIndexException : public std::runtime_error {
		public:
			explicit InvalidIndexException() : runtime_error("") {}
		};

		explicit AudioParent(Rainmeter&& rain);

	protected:
		void vReload() override;
		double vUpdate() override;
		void vResolve(array_view<isview> args, string& resolveBufferString) override;

	public:
		/// <summary>
		/// Returns value of a handler with specified name.
		/// Function can throw InvalidIndexException when index is out of bounds.
		/// </summary>
		[[nodiscard]]
		double getValue(isview unitName, isview handlerName, Channel channel, index ind);

		Version getVersion() const {
			return version;
		}

		bool checkHandlerShouldExist(isview procName, Channel channel, isview handlerName) const;

		isview findProcessingFor(isview handlerName) const;

	private:
		void initLogHelpers();

		void runFinisher(
			handler::ExternalMethods::FinishMethodType finisher,
			const handler::HandlerBase::ExternalData& handlerData,
			isview procName, Channel channel, isview handlerName
		) const;

		DeviceRequest readRequest() const;
		void resolveProp(
			string& resolveBufferString,
			isview procName, Channel channel, isview handlerName, isview propName
		);

		void runFinishers(ProcessingOrchestrator::Snapshot& snapshot) const;
	};
}
