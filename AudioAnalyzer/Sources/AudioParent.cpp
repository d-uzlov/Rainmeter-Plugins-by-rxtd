/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioParent.h"

#include "ParamParser.h"

using namespace audio_analyzer;

AudioParent::AudioParent(utils::Rainmeter&& _rain) :
	ParentBase(std::move(_rain)),
	deviceManager([this](MyWaveFormat format) {
		channelMixer.setFormat(format);
		orchestrator.setFormat(format.samplesPerSec, format.channelLayout);
	}) {
	setUseResultString(false);

	if (deviceManager.getState() == DeviceManager::State::eFATAL) {
		logger.error(L"Fatal error: can't get IMMDeviceEnumerator");
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	legacyNumber = rain.read(L"MagicNumber").asInt(0);
	switch (legacyNumber) {
	case 0:
	case 104:
		break;
	default:
		logger.error(L"Fatal error: unknown magic number {}", legacyNumber);
		setMeasureState(utils::MeasureState::eBROKEN);
	}

	deviceManager.setLogger(logger);
	deviceManager.setLegacyNumber(legacyNumber);

	notificationClient = {
		[=](auto ptr) {
			*ptr = new utils::CMMNotificationClient{ deviceManager.getDeviceEnumerator().getWrapper() };
			return true;
		}
	};

	deviceManager.getDeviceEnumerator().updateDeviceStrings();

	paramParser.setRainmeter(rain);
	orchestrator.setLogger(logger);
}

void AudioParent::vReload() {
	if (auto request = readRequest();
		request != requestedSource) {
		requestedSource = std::move(request);

		deviceManager.reconnect(requestedSource.sourceType, requestedSource.id);
	}

	const double computeTimeout = rain.read(L"computeTimeout").asFloat(-1.0);
	const double killTimeout = std::clamp(rain.read(L"killTimeout").asFloat(33.0), 1.0, 33.0);

	orchestrator.setComputeTimeout(computeTimeout);
	orchestrator.setKillTimeout(killTimeout);

	const bool anythingChanged = paramParser.parse(legacyNumber);

	if (anythingChanged) {
		orchestrator.patch(paramParser.getParseResult(), legacyNumber, snapshot);
	}
}

double AudioParent::vUpdate() {
	const auto changes = notificationClient.getPointer()->takeChanges();

	if (const auto source = requestedSource.sourceType;
		source == DeviceManager::DataSource::eDEFAULT_INPUT && changes.defaultCapture
		|| source == DeviceManager::DataSource::eDEFAULT_OUTPUT && changes.defaultRender) {
		deviceManager.reconnect(requestedSource.sourceType, requestedSource.id);
	} else if (!changes.devices.empty()) {
		if (changes.devices.count(diSnapshot.id) > 0) {
			deviceManager.reconnect(requestedSource.sourceType, requestedSource.id);
		}

		deviceManager.getDeviceEnumerator().updateDeviceStrings();
	}

	if (deviceManager.getState() != DeviceManager::State::eOK) {
		if (deviceManager.getState() == DeviceManager::State::eFATAL) {
			setMeasureState(utils::MeasureState::eBROKEN);
			logger.error(L"Unrecoverable error");
		}

		// todo
		// for (auto& [name, sa] : saMap) {
		// 	sa.resetValues();
		// }

		return 0.0;
	}

	deviceManager.updateDeviceInfoSnapshot(diSnapshot);

	bool any = false;
	deviceManager.getCaptureManager().capture([&](utils::array2d_view<float> channelsData) {
		channelMixer.saveChannelsData(channelsData, true);
		any = true;
	});

	if (any) {
		orchestrator.process(channelMixer);
		orchestrator.exchangeData(snapshot);
		channelMixer.reset();

		for (const auto& [procName, procInfo] : paramParser.getParseResult()) {
			auto& processingSnapshot = snapshot[procName];
			for (const auto& [handlerName, finisher] : procInfo.finishers) {
				for (auto& [channel, channelSnapshot] : processingSnapshot) {
					finisher(channelSnapshot[handlerName].handlerSpecificData);
				}
			}
		}
	}

	// todo if we are here, then status in true?
	return diSnapshot.status ? 1.0 : 0.0;
}

void AudioParent::vCommand(isview bangArgs) {
	if (bangArgs == L"updateDevList") {
		deviceManager.getDeviceEnumerator().updateDeviceStringLegacy(diSnapshot.type);
		return;
	}

	logger.error(L"unknown command '{}'", bangArgs);
}

void AudioParent::vResolve(array_view<isview> args, string& resolveBufferString) {
	if (args.empty()) {
		logger.error(L"Invalid section variable: args needed");
		return;
	}

	const isview optionName = args[0];
	auto cl = logger.context(L"Invalid section variable '{}': ", optionName);

	if (optionName == L"current device") {
		if (args.size() < 2) {
			cl.error(L"need >= 2 argc, but only {} found", args.size());
			return;
		}

		const isview deviceProperty = args[1];

		if (deviceProperty == L"status") {
			resolveBufferString = diSnapshot.status ? L"1" : L"0";
		} else if (deviceProperty == L"status string") {
			resolveBufferString = diSnapshot.status ? L"active" : L"down";
		} else if (deviceProperty == L"type") {
			resolveBufferString = diSnapshot.type == utils::MediaDeviceType::eINPUT ? L"input" : L"output";
		} else if (deviceProperty == L"name") {
			resolveBufferString = diSnapshot.name;
		} else if (deviceProperty == L"description") {
			resolveBufferString = diSnapshot.description;
		} else if (deviceProperty == L"id") {
			resolveBufferString = diSnapshot.id;
		} else if (deviceProperty == L"format") {
			resolveBufferString = diSnapshot.format;
		} else {
			cl.warning(L"unknown device property '{}'", deviceProperty);
		}

		return;
	}

	if (optionName == L"device list input") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListInput();
		return;
	}
	if (optionName == L"device list output") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListOutput();
		return;
	}

	if (optionName == L"value") {
		if (args.size() < 5) {
			cl.error(L"need >= 5 argc, but only {} found", args.size());
			return;
		}

		const auto procName = args[1];
		const auto channelName = args[2];
		const auto handlerName = args[3];
		const auto ind = utils::Option{ args[4] }.asInt(0);

		auto channelOpt = Channel::channelParser.find(channelName);
		if (!channelOpt.has_value()) {
			cl.error(L"channel '{}' is not recognized", channelName);
			return;
		}

		// auto procIter = saMap.find(procName);
		// if (procIter == saMap.end()) {
		// 	cl.error(L"processing '{}' is not found", procName);
		// 	return;
		// }
		//
		// const auto value = procIter->second.getAudioChildHelper().getValue(channelOpt.value(), handlerName, ind);

		const auto value = getValue(procName, handlerName, channelOpt.value(), ind);
		cl.printer.print(value);

		resolveBufferString = cl.printer.getBufferView();
		return;
	}

	if (optionName == L"handler info" || optionName == L"prop") {
		resolveProp(args, resolveBufferString);
		return;
	}

	if (optionName == L"device list") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListLegacy();
		return;
	}
}

double AudioParent::getValue(isview proc, isview id, Channel channel, index ind) const {
	auto procIter = snapshot.find(proc);
	if (procIter == snapshot.end()) {
		return 0.0;
	}

	const auto& processingSnapshot = procIter->second;
	auto channelSnapshotIter = processingSnapshot.find(channel);
	if (channelSnapshotIter == processingSnapshot.end()) {
		return 0.0;

	}

	auto& channelSnapshot = channelSnapshotIter->second;
	auto handlerSnapshotIter = channelSnapshot.find(id);
	if (handlerSnapshotIter == channelSnapshot.end()) {
		return 0.0;

	}

	auto& values = handlerSnapshotIter->second.values;
	const auto layersCount = values.getBuffersCount();
	if (layersCount == 0) {
		return 0.0;
	}
	const auto valuesCount = values.getBufferSize();
	if (ind >= valuesCount) {
		return 0.0;
	}

	return values[0][ind];
}

string AudioParent::checkHandler(isview procName, Channel channel, isview handlerName) const {
	utils::BufferPrinter bp;

	const auto procDataIter = paramParser.getParseResult().find(procName);
	if (procDataIter == paramParser.getParseResult().end()) {
		bp.print(L"processing {} is not found", procName);
		return bp.getBufferPtr();
	}

	auto procData = procDataIter->second;
	auto& channels = procData.channels;
	if (channels.find(channel) == channels.end()) {
		bp.print(L"processing {} doesn't have channel {}", procName, channel.technicalName());
		return bp.getBufferPtr();
	}

	auto& handlerMap = procData.handlersInfo.patchers;
	if (handlerMap.find(handlerName) == handlerMap.end()) {
		bp.print(L"processing {} doesn't have handler {}", procName, handlerName);
		return bp.getBufferPtr();
	}

	return { };
}

isview AudioParent::legacy_findProcessingFor(isview handlerName) {
	for (auto& [name, pd] : paramParser.getParseResult()) {
		auto& patchers = pd.handlersInfo.patchers;
		if (patchers.find(handlerName) != patchers.end()) {
			return name;
		}
	}

	return { };
}

AudioParent::RequestedDeviceDescription AudioParent::readRequest() const {
	RequestedDeviceDescription result;

	using DataSource = DeviceManager::DataSource;
	if (const auto source = rain.read(L"Source").asIString();
		!source.empty()) {
		if (source == L"Output") {
			result.sourceType = DataSource::eDEFAULT_OUTPUT;
		} else if (source == L"Input") {
			result.sourceType = DataSource::eDEFAULT_INPUT;
		} else {
			logger.debug(L"Using '{}' as source audio device ID.", source);
			result.sourceType = DataSource::eID;
			result.id = source % csView();
		}
	} else {
		// legacy
		if (auto legacyID = this->rain.read(L"DeviceID").asString();
			!legacyID.empty()) {
			result.sourceType = DataSource::eID;
			result.id = legacyID;
		} else {
			const auto port = this->rain.read(L"Port").asIString(L"Output");
			if (port == L"Output") {
				result.sourceType = DataSource::eDEFAULT_OUTPUT;
			} else if (port == L"Input") {
				result.sourceType = DataSource::eDEFAULT_INPUT;
			} else {
				logger.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
				result.sourceType = DataSource::eDEFAULT_OUTPUT;
			}
		}
	}

	return result;
}

void AudioParent::resolveProp(array_view<isview> args, string& resolveBufferString) {
	const isview optionName = args[0];

	isview procName = args[1];
	isview channelName = args[2];
	isview handlerName = args[3];
	isview propName = args[4];

	auto cl = logger.context(L"Invalid section variable '{}': ", optionName);

	if (optionName == L"handler info") {
		if (args.size() < 5) {
			cl.error(L"need >= 5 argc, but only {} found", args.size());
			return;
		}

		procName = args[1];
		channelName = args[2];
		handlerName = args[3];
		propName = args[4];
	} else if (optionName == L"prop") {
		// legacy
		if (args.size() < 4) {
			cl.error(L"need >= 4 argc, but only {} found", args.size());
			return;
		}

		channelName = args[1];
		handlerName = args[2];
		propName = args[3];

		procName = legacy_findProcessingFor(handlerName);
		if (procName.empty()) {
			cl.error(L"handler '{}' is not found", handlerName);
			return;
		}
	}

	auto channelOpt = Channel::channelParser.find(channelName);
	if (!channelOpt.has_value()) {
		cl.error(L"channel '{}' is not recognized", channelName);
		return;
	}

	auto procIter = snapshot.find(procName);
	if (procIter == snapshot.end()) {
		cl.error(L"processing '{}' is not found", procName);
		return;
	}

	auto& processingSnapshot = procIter->second;
	auto channelSnapshotIter = processingSnapshot.find(channelOpt.value());
	if (channelSnapshotIter == processingSnapshot.end()) {
		// "channel not found" is a relaxed error, so no log message for this one
		return;
	}

	auto& channelSnapshot = channelSnapshotIter->second;
	auto handlerSnapshotIter = channelSnapshot.find(handlerName);
	if (handlerSnapshotIter == channelSnapshot.end()) {
		cl.error(L"handler '{}:{}' is not found", procName, handlerName);
		return;
	}

	auto& handlerSnapshot = handlerSnapshotIter->second;
	const SoundHandler::PropGetter propGetter
		= paramParser
		  .getParseResult()
		  .find(procName)->second.handlersInfo.patchers
		  .find(handlerName)->second->propGetter;

	if (propGetter == nullptr) {
		cl.error(L"handler '{}:{}' doesn't have any props", procName, handlerName);
		return;
	}

	const bool found = propGetter(handlerSnapshot.handlerSpecificData, propName, cl.printer);
	if (!found) {
		cl.error(L"prop '{}:{}:{}' is not found", procName, handlerName, propName);
		return;
	}

	resolveBufferString = cl.printer.getBufferView();
}
