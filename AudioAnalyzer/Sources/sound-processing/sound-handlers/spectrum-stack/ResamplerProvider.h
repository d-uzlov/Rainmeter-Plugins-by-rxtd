/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../SoundHandler.h"

namespace rxtd::audio_analyzer {
	class BandResampler;

	class ResamplerProvider : public SoundHandler {
		istring resamplerId{ };
		const BandResampler* resampler = nullptr;

	public:
		void setResamplerID(isview value) {
			resamplerId = value;
		}

		void _process(const DataSupplier& dataSupplier) final;

		const BandResampler* getResampler() const {
			return resampler;
		}

	protected:
		virtual void _process2(const DataSupplier& dataSupplier) = 0;
	};

}
