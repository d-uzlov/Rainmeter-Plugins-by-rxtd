/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "CaptureManager.h"

using namespace audio_analyzer;

bool CaptureManager::setSource(DataSource type, const string& id) {
	auto deviceOpt = getDevice(type, id);

	if (!deviceOpt) {
		valid = false;
		return true;
	}

	auto audioDeviceHandle = std::move(deviceOpt.value());

	auto deviceInfo = audioDeviceHandle.readDeviceInfo();
	snapshot.status = true; // todo ?
	snapshot.id = deviceInfo.id;
	snapshot.description = deviceInfo.desc;
	snapshot.name = legacyNumber < 104 ? deviceInfo.fullFriendlyName : deviceInfo.name;

	auto audioClient = audioDeviceHandle.openAudioClient();
	if (audioDeviceHandle.getLastResult() != S_OK) {
		valid = false;
		logger.error(L"Can't create AudioClient, error code {}", audioDeviceHandle.getLastResult());
		return true;
	}

	audioClient.initShared();
	if (audioClient.getLastResult() != S_OK) {
		if (audioClient.getLastResult() == AUDCLNT_E_DEVICE_IN_USE) {
			// If device is in exclusive mode, then call to Initialize() above leads to leak in Commit memory area
			// Tested on LTSB 1607, last updates as of 2019-01-10
			// Google "WASAPI exclusive memory leak"
			// I consider this error unrecoverable to prevent further leaks
			valid = false;
			logger.error(L"Device operates in exclusive mode, won't recover");
			return false;
		}
		valid = false;
		logger.error(L"AudioClient.Initialize() fail, error code {}", audioClient.getLastResult());
		return true;
	}

	const auto format = audioClient.getFormat();
	if (format.format == utils::WaveDataFormat::eINVALID) {
		logger.error(L"Invalid sample format");
		valid = false;
		return true;
	}

	snapshot.type = audioClient.getType();
	snapshot.format.format = format.format;
	snapshot.format.samplesPerSec = format.samplesPerSec;
	snapshot.format.channelLayout = ChannelLayouts::layoutFromChannelMask(uint32_t(format.channelMask), true);
	if (snapshot.format.channelLayout.getChannelsOrderView().empty()) {
		logger.error(L"zero known channels found in current channel layout");
		valid = false;
		return true;
	}

	snapshot.formatString = makeFormatString(snapshot.format);


	audioCaptureClient = audioClient.openCapture();
	if (audioClient.getLastResult() != S_OK) {
		valid = false;
		logger.error(L"Can't create AudioCaptureClient, error code {}", audioClient.getLastResult());
		return true;
	}

	HRESULT hr = audioClient.getPointer()->Start();
	if (hr != S_OK) {
		valid = false;
		logger.error(L"Can't start stream, error code {}", hr);
		return true;
	}

	lastBufferFillTime = clock::now();

	return true;
}

void CaptureManager::capture(const ProcessingCallback& processingCallback) {
	if (!isValid()) {
		return;
	}

	while (true) {
		audioCaptureClient.readBuffer();

		const auto queryResult = audioCaptureClient.getLastResult();
		const auto now = clock::now();

		switch (queryResult) {
		case S_OK:
			lastBufferFillTime = now;

			processingCallback(audioCaptureClient.getBuffer());
			break;

		case AUDCLNT_S_BUFFER_EMPTY:
			// Windows bug: sometimes when shutting down a playback application, it doesn't zero out the buffer.
			// rxtd: I don't really understand this. I can't reproduce this and I don't know if this workaround do anything useful
			if (now - lastBufferFillTime >= EMPTY_TIMEOUT) {
				const std::chrono::duration<float, std::milli> overheadTime = now - lastBufferFillTime;
				logger.debug(L"poll timeout: {} ms", overheadTime.count());
				invalidate();
			}
			return;

		case AUDCLNT_E_BUFFER_ERROR:
		case AUDCLNT_E_DEVICE_INVALIDATED:
		case AUDCLNT_E_SERVICE_NOT_RUNNING:
			invalidate();
			return;

		default:
			logger.warning(L"Unexpected buffer query error code {error}", queryResult);
			invalidate();
			return;
		}
	}
}

void CaptureManager::invalidate() {
	audioCaptureClient = { };
	valid = false;
}

std::optional<utils::MediaDeviceWrapper> CaptureManager::getDevice(DataSource type, const string& id) {
	switch (type) {
	case DataSource::eDEFAULT_INPUT:
		return enumerator.getDefaultDevice(utils::MediaDeviceType::eINPUT);
	case DataSource::eDEFAULT_OUTPUT:
		return enumerator.getDefaultDevice(utils::MediaDeviceType::eOUTPUT);
	case DataSource::eID:
		return enumerator.getDevice(id);
	}

	return { };
}

string CaptureManager::makeFormatString(MyWaveFormat waveFormat) {
	using Format = utils::WaveDataFormat;

	if (waveFormat.format == Format::eINVALID) {
		return L"<invalid>";
	}

	utils::BufferPrinter bp;

	switch (waveFormat.format) {
	case Format::ePCM_S16:
		bp.append(L"PCM 16b");
		break;
	case Format::ePCM_F32:
		bp.append(L"PCM 32b");
		break;
	case Format::eINVALID: ;
	}

	bp.append(L", {} Hz, ", waveFormat.samplesPerSec);

	if (waveFormat.channelLayout.getName().empty()) {
		bp.append(L"unknown layout: {} recognized channels", waveFormat.channelLayout.getChannelsOrderView().size());
	} else {
		bp.append(L"{}", waveFormat.channelLayout.getName());
	}

	return string{ bp.getBufferView() };
}
