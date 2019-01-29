/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "DeviceManager.h"

#include <utility>
#include <Audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#include "windows-wrappers/WaveFormatWrapper.h"
#include "windows-wrappers/GenericComWrapper.h"
#include "windows-wrappers/PropVariantWrapper.h"

#include "undef.h"

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);

const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);


static constexpr long long REF_TIMES_PER_SEC = 1000'000'0; // 1 sec in ns

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

rxaa::DeviceManager::SilentRenderer::SilentRenderer(IMMDevice *audioDeviceHandle) {
	if (audioDeviceHandle == nullptr) {
		return;
	}

	HRESULT hr;

	// get an extra audio client for the dummy silent channel
	hr = audioDeviceHandle->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient));
	if (hr != S_OK) {
		releaseHandles();
		return;
	}

	utils::WaveFormatWrapper waveFormatWrapper;
	hr = audioClient->GetMixFormat(&waveFormatWrapper);
	if (hr != S_OK) {
		releaseHandles();
		return;
	}

	hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, REF_TIMES_PER_SEC, 0, waveFormatWrapper.getPointer(), nullptr);
	if (hr != S_OK) {
		if (hr == AUDCLNT_E_DEVICE_IN_USE) {
			// exclusive mode
			releaseHandles();
			return;
		}
		releaseHandles();
		return;
	}

	static_assert(std::is_same<UINT32, uint32_t>::value);

	// get the frame count
	uint32_t bufferFramesCount;
	hr = audioClient->GetBufferSize(&bufferFramesCount);
	if (hr != S_OK) {
		releaseHandles();
		return;
	}

	// create a render client
	hr = audioClient->GetService(IID_IAudioRenderClient, reinterpret_cast<void**>(&renderer));
	if (hr != S_OK) {
		releaseHandles();
		return;
	}

	static_assert(std::is_same<BYTE, uint8_t>::value);

	// get the buffer
	uint8_t *buffer;
	hr = renderer->GetBuffer(bufferFramesCount, &buffer);
	if (hr != S_OK) {
		releaseHandles();
		return;
	}

	// release it
	hr = renderer->ReleaseBuffer(bufferFramesCount, AUDCLNT_BUFFERFLAGS_SILENT);
	if (hr != S_OK) {
		releaseHandles();
		return;
	}

	// start the stream
	hr = audioClient->Start();
	if (hr != S_OK) {
		releaseHandles();
		return;
	}
}

rxaa::DeviceManager::SilentRenderer::SilentRenderer(SilentRenderer&& other) noexcept {
	releaseHandles();

	audioClient = other.audioClient;
	renderer = other.renderer;

	other.audioClient = nullptr;
	other.renderer = nullptr;
}

rxaa::DeviceManager::SilentRenderer& rxaa::DeviceManager::SilentRenderer::operator=(SilentRenderer&& other) noexcept {
	if (this == &other)
		return *this;

	releaseHandles();

	audioClient = other.audioClient;
	renderer = other.renderer;

	other.audioClient = nullptr;
	other.renderer = nullptr;

	return *this;
}

rxaa::DeviceManager::SilentRenderer::~SilentRenderer() {
	releaseHandles();
}

bool rxaa::DeviceManager::SilentRenderer::isError() const {
	return audioClient == nullptr || renderer == nullptr;
}

void rxaa::DeviceManager::SilentRenderer::releaseHandles() {
	if (renderer != nullptr) {
		renderer->Release();
		renderer = nullptr;
	}

	if (audioClient != nullptr) {
		audioClient->Stop();
		audioClient->Release();
		audioClient = nullptr;
	}
}



std::variant<rxaa::DeviceManager::CaptureManager, rxaa::DeviceManager::CaptureManager::Error>
rxaa::DeviceManager::CaptureManager::create(IMMDevice& audioDeviceHandle, bool loopback) {
	utils::GenericComWrapper<IAudioClient> audioClient { };
	utils::GenericComWrapper<IAudioCaptureClient> audioCaptureClient { };

	HRESULT hr = audioDeviceHandle.Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient));
	if (hr != S_OK) {
		return Error { true, L"Can't create AudioClient", hr };
	}

	utils::WaveFormatWrapper waveFormatWrapper;
	hr = audioClient->GetMixFormat(&waveFormatWrapper);
	if (hr != S_OK) {
		return Error { true, L"GetMixFormat() fail", hr };
	}

	auto formatOpt = parseStreamFormat(waveFormatWrapper.getPointer());
	if (!formatOpt.has_value()) {
		return Error { true, L"Invalid sample format", waveFormatWrapper.getPointer()->wFormatTag };
	}

	hr = audioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0,
		REF_TIMES_PER_SEC,
		0,
		waveFormatWrapper.getPointer(),
		nullptr);
	if (hr != S_OK) {
		if (hr == AUDCLNT_E_DEVICE_IN_USE) {
			// If device is in inclusive mode, then call to Initialize above leads to commit leak
			// Tested on LTSB 1607, last updates as of 2019-01-10
			// Google "WASAPI exclusive memory leak"
			// I consider this error unrecoverable to prevent leak
			return Error { false, L"Device operates in exclusive mode", hr };
		}
		return Error { true, L"AudioClient.Initialize() fail", hr };
	}

	hr = audioClient->GetService(IID_IAudioCaptureClient, reinterpret_cast<void**>(&audioCaptureClient));
	if (hr != S_OK) {
		return Error { true, L"Can't create AudioCaptureClient", hr };
	}

	hr = audioClient->Start();
	if (hr != S_OK) {
		return Error { true, L"Can't start stream", hr };
	}

	CaptureManager captureManager { };
	captureManager.audioClient = std::move(audioClient);
	captureManager.audioCaptureClient = std::move(audioCaptureClient);
	captureManager.waveFormat = formatOpt.value();
	return captureManager;
}

rxaa::DeviceManager::CaptureManager::~CaptureManager() {
	releaseHandles();
}

rxaa::MyWaveFormat rxaa::DeviceManager::CaptureManager::getWaveFormat() const {
	return waveFormat;
}

bool rxaa::DeviceManager::CaptureManager::isEmpty() const {
	return !audioCaptureClient.isValid() || !audioClient.isValid();
}

bool rxaa::DeviceManager::CaptureManager::isValid() const {
	return !isEmpty() && waveFormat.format != Format::INVALID;
}

utils::BufferWrapper rxaa::DeviceManager::CaptureManager::readBuffer() {
	return utils::BufferWrapper(audioCaptureClient.getPointer());
}

void rxaa::DeviceManager::CaptureManager::releaseHandles() {
	waveFormat = { };
	audioCaptureClient = { };
	audioClient = { };
}



rxaa::DeviceManager::DeviceManager(utils::Rainmeter::Logger& logger, std::function<void(MyWaveFormat waveFormat)> waveFormatUpdateCallback)
	: logger(logger), waveFormatUpdateCallback(std::move(waveFormatUpdateCallback)) {
	// create the enumerator
	HRESULT res = CoCreateInstance(
		CLSID_MMDeviceEnumerator,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_IMMDeviceEnumerator,
		reinterpret_cast<void**>(&audioEnumeratorHandle)
	);
	if (res != S_OK) {
		audioEnumeratorHandle.release();
		logger.error(L"Can't create device enumerator, error {error}", res);
		objectIsValid = false;
		return;
	}
}

rxaa::DeviceManager::~DeviceManager() {
	deviceRelease();
}

void rxaa::DeviceManager::deviceInit() {
	lastDevicePollTime = clock::now();

	const bool handleAcquired = acquireDeviceHandle();
	if (!handleAcquired) {
		deviceRelease();
		return;
	}

	if (!createSilentRenderer()) {
		return;
	}

	if (!createCaptureManager()) {
		return;
	}

	if (!captureManager.isValid()) {
		return;
	}

	readDeviceInfo();

	waveFormatUpdateCallback(captureManager.getWaveFormat());
}

bool rxaa::DeviceManager::acquireDeviceHandle() {
	HRESULT resultCode;
	// if a specific ID was requested, search for that one, otherwise get the default
	if (!deviceID.empty()) {
		resultCode = audioEnumeratorHandle->GetDevice(deviceID.c_str(), &audioDeviceHandle);
		if (resultCode != S_OK) {
			logger.error(L"Audio {} device '{}' not found (error {error}).", port == Port::OUTPUT ? L"output" : L"input", deviceID, resultCode);
			return false;
		}
	} else {
		resultCode = audioEnumeratorHandle->GetDefaultAudioEndpoint(port == Port::OUTPUT ? eRender : eCapture, eConsole, &audioDeviceHandle);
		if (resultCode != S_OK) {
			logger.error(L"Can't get Default {} Audio device (error {error}).", port == Port::OUTPUT ? L"output" : L"input", resultCode);
			return false;
		}
	}

	return true;
}

void rxaa::DeviceManager::deviceRelease() {
	silentRenderer = SilentRenderer();
	captureManager = CaptureManager();

	if (audioDeviceHandle != nullptr) {
		audioDeviceHandle->Release();
		audioDeviceHandle = nullptr;
	}

	deviceInfo.reset();
}

void rxaa::DeviceManager::readDeviceName() {
	deviceInfo.name.clear();

	if (audioDeviceHandle == nullptr) {
		return;
	}

	utils::GenericComWrapper<IPropertyStore>	props;
	if (audioDeviceHandle->OpenPropertyStore(STGM_READ, &props) != S_OK) {
		return;
	}

	utils::PropVariantWrapper	prop;
	if (props->GetValue(PKEY_Device_FriendlyName, &prop) != S_OK) {
		return;
	}

	deviceInfo.name = prop.getCString();
}

void rxaa::DeviceManager::readDeviceId() {
	deviceInfo.id.clear();

	if (audioDeviceHandle == nullptr) {
		return;
	}

	wchar_t *resultCString = nullptr;
	if (audioDeviceHandle->GetId(&resultCString) != S_OK) {
		return;
	}
	deviceInfo.id = resultCString;

	CoTaskMemFree(resultCString);
}

void rxaa::DeviceManager::readDeviceFormat() {
	const auto& waveFormat = captureManager.getWaveFormat();
	auto &format = deviceInfo.format;

	if (waveFormat.format == Format::INVALID) {
		format = waveFormat.format.toString();
		return;
	}

	format.clear();

	format.reserve(64);

	format += waveFormat.format.toString();
	format += L", "sv;

	format += std::to_wstring(waveFormat.samplesPerSec);
	format += L"Hz, "sv;

	if (waveFormat.channelLayout == nullptr) {
		format += L"unknown layout: "sv;
		format += std::to_wstring(waveFormat.channelsCount);
		format += L"ch"sv;
	} else {
		format += waveFormat.channelLayout->getName();
	}
}

bool rxaa::DeviceManager::createSilentRenderer() {
	return true;
	// TODO remove function or uncomment
	if (port == Port::OUTPUT) {
		silentRenderer = SilentRenderer { audioDeviceHandle };
		if (silentRenderer.isError()) {
			logger.warning(L"Can't create silent render client");
		}
	}
}

bool rxaa::DeviceManager::createCaptureManager() {
	auto result = CaptureManager::create(*audioDeviceHandle, port == Port::OUTPUT);
	
	if (result.index() == 1) {
		const auto error = std::get<CaptureManager::Error>(result);
		logger.error(L"{}, {error}", error.explanation, error.errorCode);
		if (!error.temporary) {
			objectIsValid = false;
		}
		return false;
	}
	captureManager = std::move(std::get<0>(result));

	if (!captureManager.isValid()) {
		logger.error(L"Can't create capture client");
		return false;
	}

	return true;
}

void rxaa::DeviceManager::ensureDeviceAcquired() {
	if (captureManager.isValid()) {
		return;
	}

	if (clock::now() - lastDevicePollTime < DEVICE_POLL_TIMEOUT) {
		return;
	}

	deviceInit();
}

std::optional<rxaa::MyWaveFormat> rxaa::DeviceManager::parseStreamFormat(WAVEFORMATEX *waveFormatEx) {
	MyWaveFormat waveFormat;
	waveFormat.channelsCount = waveFormatEx->nChannels;
	waveFormat.samplesPerSec = waveFormatEx->nSamplesPerSec;

	if (waveFormatEx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		const auto formatExtensible = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(waveFormatEx);

		if (formatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && waveFormatEx->wBitsPerSample == 16) {
			waveFormat.format = Format::PCM_S16;
		} else if (formatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
			waveFormat.format = Format::PCM_F32;
		} else {
			return std::nullopt;
		}

		waveFormat.channelLayout = layoutKeeper.layoutFromChannelMask(formatExtensible->dwChannelMask);

		return waveFormat;
	}

	if (waveFormatEx->wFormatTag == WAVE_FORMAT_PCM && waveFormatEx->wBitsPerSample == 16) {
		waveFormat.format = Format::PCM_S16;
	} else if (waveFormatEx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
		waveFormat.format = Format::PCM_F32;
	} else {
		return std::nullopt;
	}

	if (waveFormatEx->nChannels == 1) {
		waveFormat.channelLayout = layoutKeeper.getMono();
	} else if (waveFormatEx->nChannels == 2) {
		waveFormat.channelLayout = layoutKeeper.getStereo();
	} else {
		return std::nullopt;
	}

	return waveFormat;
}

void rxaa::DeviceManager::readDeviceInfo() {
	readDeviceName();
	readDeviceId();
	readDeviceFormat();
}

bool rxaa::DeviceManager::isObjectValid() const {
	return objectIsValid;
}

void rxaa::DeviceManager::setOptions(Port port, sview deviceID) {
	if (!objectIsValid) {
		return;
	}

	this->port = port;
	this->deviceID = deviceID;
}

void rxaa::DeviceManager::init() {
	if (!objectIsValid) {
		return;
	}

	deviceInit();
}

rxaa::DeviceManager::BufferFetchResult rxaa::DeviceManager::nextBuffer() {
	if (!objectIsValid) {
		return BufferFetchResult::invalidState();
	}

	ensureDeviceAcquired();

	if (!captureManager.isValid()) {
		return BufferFetchResult::deviceError();
	}

	auto bufferWrapper = captureManager.readBuffer();

	const auto queryResult = bufferWrapper.getResult();
	const auto now = clock::now();

	// detect device disconnection
	switch (queryResult) {
	case S_OK:
		lastBufferFillTime = now;

		return bufferWrapper;

	case AUDCLNT_S_BUFFER_EMPTY:
		// Windows bug: sometimes when shutting down a playback application, it doesn't zero
		// out the buffer.  Detect this by checking the time since the last successful fill
		// and resetting the volumes if past the threshold.
		if (now - lastBufferFillTime >= EMPTY_TIMEOUT) {
			return BufferFetchResult::deviceError();
		}
		return BufferFetchResult::noData();

	case AUDCLNT_E_BUFFER_ERROR:
	case AUDCLNT_E_DEVICE_INVALIDATED:
	case AUDCLNT_E_SERVICE_NOT_RUNNING:
		logger.debug(L"Audio device disconnected");
		deviceRelease();
		return BufferFetchResult::deviceError();

	default:
		logger.warning(L"Unexpected buffer query error code {error}", queryResult);
		deviceRelease();
		return BufferFetchResult::deviceError();
	}
}

const string& rxaa::DeviceManager::getDeviceName() const {
	return deviceInfo.name;
}

const string& rxaa::DeviceManager::getDeviceId() const {
	return deviceInfo.id;
}

const string& rxaa::DeviceManager::getDeviceList() const {
	return deviceList;
}

void rxaa::DeviceManager::updateDeviceList() {
	deviceList.clear();

	utils::GenericComWrapper<IMMDeviceCollection> collection;
	if (audioEnumeratorHandle->EnumAudioEndpoints(port == Port::OUTPUT ? eRender : eCapture,
		DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &collection) != S_OK) {
		return;
	}

	static_assert(std::is_same<UINT, uint32_t>::value);
	uint32_t devicesCount;
	collection->GetCount(&devicesCount);

	deviceList.reserve(devicesCount * 120LL);
	bool first = true;

	for (index deviceIndex = 0; deviceIndex < index(devicesCount); ++deviceIndex) {
		utils::GenericComWrapper<IMMDevice> device;
		utils::GenericComWrapper<IPropertyStore> props;
		if (collection->Item(UINT(deviceIndex), &device) != S_OK || device->OpenPropertyStore(STGM_READ, &props) != S_OK) {
			continue;
		}

		wchar_t *idCString = nullptr;
		utils::PropVariantWrapper	prop;

		if (device->GetId(&idCString) == S_OK &&
			props->GetValue(PKEY_Device_FriendlyName, &prop) == S_OK) {
			if (first) {
				first = false;
			} else {
				deviceList += L"\n"sv;
			}
			deviceList += idCString;
			deviceList += L" "sv;
			deviceList += prop.getCString();
		}

		CoTaskMemFree(idCString);
	}
}

bool rxaa::DeviceManager::getDeviceStatus() const {
	if (audioDeviceHandle == nullptr) {
		return false;
	}
	// static_assert(std::is_same<DWORD, uint32_t>::value); // ...
	DWORD state;
	const auto result = audioDeviceHandle->GetState(&state);
	if (result != S_OK) {
		return false;
	}
	return state == DEVICE_STATE_ACTIVE;
}

const string& rxaa::DeviceManager::getDeviceFormat() const {
	return deviceInfo.format;
}
