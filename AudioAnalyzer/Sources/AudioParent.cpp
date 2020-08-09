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
	deviceManager(logger, [this](MyWaveFormat format) {
		channelMixer.setFormat(format);
		for (auto& [name, sa] : saMap) {
			sa.setFormat(format.samplesPerSec, format.channelLayout);
		}
		currentFormat = std::move(format);
	}) {
	setUseResultString(false);

	if (deviceManager.getState() == DeviceManager::State::eFATAL) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	notificationClient = {
		[=](auto ptr) {
			*ptr = new utils::CMMNotificationClient{ deviceManager.getDeviceEnumerator().getWrapper() };
			return true;
		}
	};

	deviceManager.getDeviceEnumerator().updateDeviceStrings();

	paramParser.setRainmeter(rain);
}

void AudioParent::vReload() {
	const auto source = rain.read(L"Source").asIString();
	string id = { };

	using DataSource = DeviceManager::DataSource;
	DataSource sourceEnum;
	if (!source.empty()) {
		if (source == L"Output") {
			sourceEnum = DataSource::eDEFAULT_OUTPUT;
		} else if (source == L"Input") {
			sourceEnum = DataSource::eDEFAULT_INPUT;
		} else {
			logger.debug(L"Using '{}' as source audio device ID.", source);
			sourceEnum = DataSource::eID;
			id = source % csView();
		}
	} else {
		// legacy
		if (auto legacyID = this->rain.read(L"DeviceID").asString();
			!legacyID.empty()) {
			logger.debug(L"Using '{}' as source audio device ID.", legacyID);
			sourceEnum = DataSource::eID;
			id = legacyID;
		} else {
			const auto port = this->rain.read(L"Port").asIString(L"Output");
			if (port == L"Output") {
				sourceEnum = DataSource::eDEFAULT_OUTPUT;
			} else if (port == L"Input") {
				sourceEnum = DataSource::eDEFAULT_INPUT;
			} else {
				logger.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
				sourceEnum = DataSource::eDEFAULT_OUTPUT;
			}
		}
	}

	computeTimeout = rain.read(L"computeTimeout").asFloat(-1.0);

	killTimeout = std::clamp(rain.read(L"killTimeout").asFloat(33.0), 1.0, 33.0);

	deviceManager.setOptions(sourceEnum, id);

	const bool anythingChanged = paramParser.parse();

	if (anythingChanged) {
		patchSA(paramParser.getParseResult());
	}
}

double AudioParent::vUpdate() {
	const auto changes = notificationClient.getPointer()->takeChanges();

	const auto source = deviceManager.getRequesterSourceType();
	if (source == DeviceManager::DataSource::eDEFAULT_INPUT && changes.defaultCapture
		|| source == DeviceManager::DataSource::eDEFAULT_OUTPUT && changes.defaultRender) {
		deviceManager.forceReconnect();
	}

	if (!changes.devices.empty()) {
		const auto requestedSourceId = deviceManager.getRequestedSourceId();
		if (changes.devices.count(requestedSourceId) > 0) {
			deviceManager.checkAndRepair();
		}

		deviceManager.getDeviceEnumerator().updateDeviceStrings();
	}

	if (deviceManager.getState() == DeviceManager::State::eOK) {
		deviceManager.getCaptureManager().capture([&](bool silent, array_view<std::byte> buffer) {
			if (!silent) {
				channelMixer.decomposeFramesIntoChannels(buffer, true);
			} else {
				channelMixer.writeSilence(buffer.size(), true);
			}
		});

		process();
	}

	deviceManager.checkAndRepair();

	if (deviceManager.getState() != DeviceManager::State::eOK) {
		if (deviceManager.getState() == DeviceManager::State::eFATAL) {
			setMeasureState(utils::MeasureState::eBROKEN);
			logger.error(L"Unrecoverable error");
		}

		for (auto& [name, sa] : saMap) {
			sa.resetValues();
		}
	}

	return deviceManager.getDeviceStatus();
}

void AudioParent::vCommand(isview bangArgs) {
	if (bangArgs == L"updateDevList") {
		deviceManager.getDeviceEnumerator().updateDeviceStringLegacy(deviceManager.getCurrentDeviceType());
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
			resolveBufferString = deviceManager.getDeviceStatus() ? L"1" : L"0";
			return;
		}
		if (deviceProperty == L"status string") {
			resolveBufferString = deviceManager.getDeviceStatus() ? L"active" : L"down";
			return;
		}
		if (deviceProperty == L"type") {
			switch (deviceManager.getCurrentDeviceType()) {
			case utils::MediaDeviceType::eINPUT:
				resolveBufferString = L"input";
				return;
			case utils::MediaDeviceType::eOUTPUT:
				resolveBufferString = L"output";
				return;
			default: ;
			}
		}
		if (deviceProperty == L"name") {
			resolveBufferString = deviceManager.getDeviceInfo().fullFriendlyName;
			return;
		}
		if (deviceProperty == L"nameOnly") {
			resolveBufferString = deviceManager.getDeviceInfo().name;
			return;
		}
		if (deviceProperty == L"description") {
			resolveBufferString = deviceManager.getDeviceInfo().desc;
			return;
		}
		if (deviceProperty == L"id") {
			resolveBufferString = deviceManager.getDeviceInfo().id;
			return;
		}
		if (deviceProperty == L"format") {
			resolveBufferString = deviceManager.getCaptureManager().getFormatString();
			return;
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

		auto procIter = saMap.find(procName);
		if (procIter == saMap.end()) {
			cl.error(L"processing '{}' is not found", procName);
			return;
		}

		const auto value = procIter->second.getAudioChildHelper().getValue(channelOpt.value(), handlerName, ind);
		cl.printer.print(value);

		resolveBufferString = cl.printer.getBufferView();
		return;
	}

	if (optionName == L"handler info") {
		if (args.size() < 5) {
			cl.error(L"need >= 5 argc, but only {} found", args.size());
			return;
		}

		const auto procName = args[1];
		const auto channelName = args[2];
		const auto handlerName = args[3];
		const auto propName = args[4];

		auto channelOpt = Channel::channelParser.find(channelName);
		if (!channelOpt.has_value()) {
			cl.error(L"channel '{}' is not recognized", channelName);
			return;
		}

		auto procIter = saMap.find(procName);
		if (procIter == saMap.end()) {
			cl.error(L"processing '{}' is not found", procName);
			return;
		}

		auto handler = procIter->second.getAudioChildHelper().findHandler(channelOpt.value(), handlerName);
		if (handler == nullptr) {
			cl.error(L"handler '{}:{}:{}' is not found", procName, channelName, handlerName);
			return;
		}

		const bool found = handler->vGetProp(propName, cl.printer);
		if (!found) {
			cl.error(L"prop '{}:{}' is not found", handlerName, propName);
			return;
		}

		resolveBufferString = cl.printer.getBufferView();
		return;
	}


	legacy_resolve(args, resolveBufferString);
}

double AudioParent::getValue(isview proc, isview id, Channel channel, index ind) const {
	auto procIter = saMap.find(proc);
	if (procIter == saMap.end()) {
		return 0.0;
	}
	return procIter->second.getAudioChildHelper().getValue(channel, id, ind);
}

double AudioParent::legacy_getValue(isview id, Channel channel, index ind) const {
	auto [handler, helper] = findHandlerByName(id, channel);
	if (handler == nullptr) {
		return 0.0;
	}
	return helper.getValueFrom(handler, channel, ind);
}

void AudioParent::process() {
	using clock = std::chrono::high_resolution_clock;
	static_assert(clock::is_steady);

	const auto processBeginTime = clock::now();

	const std::chrono::duration<float, std::milli> processMaxDuration{ killTimeout };
	const auto killTime = clock::now() + std::chrono::duration_cast<std::chrono::milliseconds>(processMaxDuration);

	bool killed = false;
	for (auto& [name, sa] : saMap) {
		killed = sa.process(channelMixer, killTime);
		if (killed) {
			break;
		}
	}
	channelMixer.reset();

	const auto processEndTime = clock::now();

	const auto processDuration = std::chrono::duration<double, std::milli>{ processEndTime - processBeginTime }.count();

	if (killed) {
		logger.error(L"handler processing was killed on timeout after {} m, on stage 1", processDuration);
		return;
	}

	for (auto& [name, sa] : saMap) {
		killed = sa.finishStandalone(killTime);
		if (killed) {
			break;
		}
	}

	const auto fullEndTime = clock::now();

	const auto fullDuration = std::chrono::duration<double, std::milli>{ fullEndTime - processBeginTime }.count();

	if (killed) {
		logger.error(L"handler processing was killed on timeout after {} m, on stage 2", fullDuration);
		return;
	}
	if (computeTimeout > 0 && processDuration > computeTimeout) {
		logger.debug(L"processing overhead {} ms over specified {} ms", fullDuration - computeTimeout, computeTimeout);
	}
}

void AudioParent::patchSA(const ParamParser::ProcessingsInfoMap& procs) {
	for (auto iter = saMap.begin();
		iter != saMap.end();) {
		if (procs.find(iter->first) == procs.end()) {
			iter = saMap.erase(iter);
		} else {
			++iter;
		}
	}

	for (auto& [name, data] : procs) {
		auto& sa = saMap[name];
		sa.getCPH().setParams(std::move(data.fcc), data.targetRate);
		sa.setFormat(currentFormat.samplesPerSec, currentFormat.channelLayout);
		sa.setParams(data.channels, data.handlersInfo, data.granularity, paramParser.getLegacyNumber());
	}
}

std::pair<SoundHandler*, AudioChildHelper>
AudioParent::findHandlerByName(isview name, Channel channel) const {
	for (auto& [_, analyzer] : saMap) {
		auto handler = analyzer.getAudioChildHelper().findHandler(channel, name);
		if (handler != nullptr) {
			return { handler, analyzer.getAudioChildHelper() };
		}
	}

	return { };
}

void AudioParent::legacy_resolve(array_view<isview> args, string& resolveBufferString) {
	const isview optionName = args[0];
	auto cl = logger.context(L"Invalid section variable '{}': ", optionName);

	if (optionName == L"device list") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListLegacy();
		return;
	}

	if (optionName == L"prop") {
		if (args.size() < 4) {
			cl.error(L"need >= 4 argc, but only {} found", args.size());
			return;
		}

		const auto channelName = args[1];
		const auto handlerName = args[2];
		const auto propName = args[3];

		auto channelOpt = Channel::channelParser.find(channelName);
		if (!channelOpt.has_value()) {
			cl.error(L"channel '{}' is not recognized", channelName);
			return;
		}

		auto [handler, helper] = findHandlerByName(handlerName, channelOpt.value());
		if (handler == nullptr) {
			cl.error(L"handler '{}:{}' is not found", channelName, handlerName);
			return;
		}

		const bool found = handler->vGetProp(propName, cl.printer);
		if (!found) {
			cl.error(L"prop '{}:{}' is not found", handlerName, propName);
			return;
		}

		resolveBufferString = cl.printer.getBufferView();
		return;
	}

	cl.error(L"option is not supported");
}
