/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "GenericComWrapper.h"
#include <Audioclient.h>

#include "array2d_view.h"
#include "Vector2D.h"

namespace rxtd::utils {
	class IAudioCaptureClientWrapper : public GenericComWrapper<IAudioCaptureClient> {
		index lastResult{ };

		Vector2D<float> buffer;
		bool empty = true;

	public:
		enum class Type {
			eInt,
			eFloat,
		};

		IAudioCaptureClientWrapper() = default;
		explicit IAudioCaptureClientWrapper(InitFunction initFunction);

		[[nodiscard]]
		bool isEmpty() const {
			return empty;
		}

		void readBuffer(Type type, index channelsCount);

		[[nodiscard]]
		index getLastResult() const {
			return lastResult;
		}

		[[nodiscard]]
		array2d_view<float> getBuffer() const {
			return buffer;
		}

	private:
		static void copyFloat(void* source, array_span<float> dest, index offset, index stride);

		static void copyInt(void* source, array_span<float> dest, index offset, index stride);
	};
}
