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
	private:

		index sourceRate = 0; // before any modifications
		index targetRate = 0; // desired rate
		index sampleRate = 0; // after division
		index divide = 1;

	public:
		void setSourceRate(index value);
		void setTargetRate(index value);

		index getSampleRate() const;
		index calculateFinalWaveSize(index waveSize) const;
		void resample(array_span<float> values) const;

	private:
		void updateValues();
	};

}
