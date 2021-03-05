// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

 // my-windows must be before any WINAPI include
#include "rxtd/my-windows.h"
// ReSharper disable once CppWrongIncludesOrder
#include <Audioclient.h>

#include "rxtd/std_fixes/Vector2D.h"
#include "rxtd/winapi_wrappers/GenericComWrapper.h"

namespace rxtd::audio_analyzer::wasapi_wrappers {
	class AudioCaptureClient : public winapi_wrappers::GenericComWrapper<IAudioCaptureClient> {
	public:
		enum class Type {
			eInt16,
			eFloat,
		};

	private:
		template<typename T>
		using Vector2D = std_fixes::Vector2D<T>;
		
		Type type{};
		index channelsCount{};

		Vector2D<float> buffer;

	public:
		AudioCaptureClient() = default;

		template<typename InitFunction>
		AudioCaptureClient(InitFunction initFunction) : GenericComWrapper(std::move(initFunction)) { }

		void setParams(Type _type, index _channelsCount) {
			type = _type;
			channelsCount = _channelsCount;
		}

		HRESULT readBuffer();

		[[nodiscard]]
		std_fixes::array2d_view<float> getBuffer() const {
			return buffer;
		}

	private:
		static void copyFloat(void* source, array_span<float> dest, index offset, index stride);

		static void copyInt(void* source, array_span<float> dest, index offset, index stride);
	};
}
