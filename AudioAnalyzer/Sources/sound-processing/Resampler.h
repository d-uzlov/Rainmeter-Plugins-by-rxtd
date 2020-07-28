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

namespace rxtd::audio_analyzer {
	class Resampler {
		index sourceRate = 0; // before any modifications
		index targetRate = 0; // desired rate
		index sampleRate = 0; // after division
		index divide = 1;

	public:
		index getSourceRate() const {
			return sourceRate;
		}

		index getTargetRate() const {
			return targetRate;
		}

		index toSourceSize(index resampledSize) const {
			return resampledSize * divide;
		}

		void setSourceRate(index value);
		void setTargetRate(index value);

		index getSampleRate() const {
			return sampleRate;
		}

		index calculateFinalWaveSize(index waveSize) const {
			return waveSize / divide;
		}

		void resample(array_view<float> from, array_span<float> to) const;

		void resample(array_span<float> values) const {
			resample(values, values);
		}

	private:
		void updateValues();
	};
}
