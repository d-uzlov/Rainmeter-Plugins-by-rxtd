/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include "IAudioClientWrapper.h"
#include "winapi-wrappers/GenericComWrapper.h"
#include "winapi-wrappers/GenericCoTaskMemWrapper.h"

namespace rxtd::utils {
	class MediaDeviceWrapper : public GenericComWrapper<IMMDevice> {
	public:
		struct DeviceInfo {
			string name;
			string desc;
			string fullFriendlyName;
			string formFactor;
		};

	private:
		string id;
		bool infoIsFilled = false;
		MediaDeviceType type{};

	public:
		MediaDeviceWrapper() = default;

		template<typename InitFunction>
		MediaDeviceWrapper(InitFunction initFunction, sview id) : GenericComWrapper(std::move(initFunction)), id(id) {
			readType();
		}

		template<typename InitFunction>
		MediaDeviceWrapper(InitFunction initFunction, MediaDeviceType type) : GenericComWrapper(std::move(initFunction)), type(type) {
			readId();
		}

		[[nodiscard]]
		sview getId() const {
			return id;
		}

		[[nodiscard]]
		DeviceInfo readDeviceInfo() noexcept(false);

		[[nodiscard]]
		IAudioClientWrapper openAudioClient();

		template<typename Interface>
		GenericComWrapper<Interface> activateFor() {
			sview source;
			if constexpr (std::is_same_v<IAudioClient, Interface>) {
				source = L"IAudioClient.Activate(IAudioClient)";
			} else if constexpr (std::is_same_v<IAudioEndpointVolume, Interface>) {
				source = L"IAudioClient.Activate(IAudioEndpointVolume)";
			} else if constexpr (std::is_same_v<IAudioMeterInformation, Interface>) {
				source = L"IAudioClient.Activate(IAudioMeterInformation)";
			} else if constexpr (std::is_same_v<IAudioSessionManager, Interface>) {
				source = L"IAudioClient.Activate(IAudioSessionManager)";
			} else if constexpr (std::is_same_v<IAudioSessionManager2, Interface>) {
				source = L"IAudioClient.Activate(IAudioSessionManager2)";
			} else if constexpr (std::is_same_v<IDeviceTopology, Interface>) {
				source = L"IAudioClient.Activate(IDeviceTopology)";
			} else {
				static_assert(false, "Interface is not supported by IMMDevice");
			}

			return {
				[&](auto ptr) {
					throwOnError(typedQuery(&IMMDevice::Activate, ptr, CLSCTX_INPROC_SERVER, nullptr), source);
					return true;
				}
			};
		}

	private:
		[[nodiscard]]
		static sview convertFormFactor(std::optional<EndpointFormFactor> value);

		void readType() noexcept(false) {
			auto endpoint = GenericComWrapper<IMMEndpoint>{
				[&](auto ptr) {
					throwOnError(ref().QueryInterface(ptr), L"IMMDevice.QueryInterface(IMMEndpoint)");
					return true;
				}
			};
			EDataFlow flow{};
			throwOnError(endpoint.ref().GetDataFlow(&flow), L"IMMEndpoint.GetDataFlow()");
			switch (flow) {
			case eRender:
				type = MediaDeviceType::eOUTPUT;
				break;
			case eCapture:
				type = MediaDeviceType::eINPUT;
				break;
			default: throw ComException{ -1, L"invalid EDataFlow value from IMMEndpoint.GetDataFlow()" };
			}
		}

		void readId() noexcept(false) {
			GenericCoTaskMemWrapper<wchar_t> idWrapper{
				[&](auto ptr) {
					throwOnError(ref().GetId(ptr), L"IMMDevice.GetId()");
					return true;
				}
			};
			id = idWrapper.getPointer();
			if (id.empty()) {
				throw ComException{ -1, L"invalid empty value for device id from IMMDevice.GetId()" };
			}
		}
	};
}
