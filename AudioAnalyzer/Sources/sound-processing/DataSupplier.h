/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Channel.h"
#include "array_view.h"

namespace rxtd::audio_analyzer {
	class SoundHandler;

	class DataSupplier {
	public:
		virtual ~DataSupplier() = default;
		[[nodiscard]]
		virtual array_view<float> getWave() const = 0;

		template <typename T = SoundHandler>
		[[nodiscard]]
		const T* getHandler(isview id) const {
			return dynamic_cast<const T*>(getHandlerRaw(id));
		}

		virtual void log(wchar_t* message) const = 0;

	protected:
		[[nodiscard]]
		virtual const SoundHandler* getHandlerRaw(isview id) const = 0;
	};
}
