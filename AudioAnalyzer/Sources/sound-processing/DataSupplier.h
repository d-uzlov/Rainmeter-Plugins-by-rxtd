/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "array_view.h"
#include "BufferPrinter.h"

namespace rxtd::audio_analyzer {
	class DataSupplier {
		// TODO remove mutable
		mutable utils::BufferPrinter printer;

	public:
		virtual ~DataSupplier() = default;
		[[nodiscard]]
		virtual array_view<float> getWave() const = 0;

		template <typename ...Args>
		void log(const wchar_t* message, const Args&... args) const {
			printer.print(message, args...);
			_log(printer.getBufferPtr());
		}

	protected:
		virtual void _log(const wchar_t* message) const = 0;
	};
}
