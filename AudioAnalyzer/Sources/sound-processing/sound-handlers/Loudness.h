/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "BlockHandler.h"
#include "../../audio-utils/filter-utils/FilterCascade.h"

namespace rxtd::audio_analyzer {
	class Loudness : public BlockHandler {
		// audio_utils::FilterCascade fc{ };

		double intermediateRmsResult = 0.0;

	public:
		void _process(array_view<float> wave, float average) override;
		void _setSamplesPerSec(index samplesPerSec) override;

	protected:
		void _reset() override;
		void finishBlock() override;
		sview getDefaultTransform() override;
	};
}
