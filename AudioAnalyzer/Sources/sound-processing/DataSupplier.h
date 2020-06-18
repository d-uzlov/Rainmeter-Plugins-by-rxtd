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
		virtual const float* getWave() const = 0;
		virtual index getWaveSize()  const = 0;

		template<typename T = SoundHandler>
		const T* getHandler(isview id) const {
			return dynamic_cast<const T*>(getHandlerRaw(id));
		}
		virtual Channel getChannel() const = 0;

		/**
		 * returns array of size @code size.
		 * Can be called several times, each time buffer will be different.
		 * Released automatically after end of process() and processSilent().
		 */
		template<typename T>
		array_span<T> getBuffer(index size) const {
			return { reinterpret_cast<T*>(getBufferRaw(size * sizeof(T))), size };
		}

	protected:
		virtual std::byte* getBufferRaw(index size) const = 0;
		virtual const SoundHandler* getHandlerRaw(isview id) const = 0;
	};
}
