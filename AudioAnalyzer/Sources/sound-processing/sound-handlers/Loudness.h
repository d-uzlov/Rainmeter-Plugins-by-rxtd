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
#include "../../audio-utils/InfiniteResponseFilter.h"

namespace rxtd::audio_analyzer {
	class Loudness : public BlockHandler {
		// based on EBU R 128
		// see:
		//   https://books.google.ru/books?id=wYNiDwAAQBAJ&pg=PT402&lpg=PT402&source=bl&ots=b_IYgSnzH_&sig=ACfU3U24oCdbQZLqFmaH7sFO39CpaoRZVQ&hl=en&sa=X&ved=2ahUKEwjMobfksaDqAhVxx4sKHaRSBToQ6AEwAnoECAoQAQ#v=onepage&f=false
		//   https://github.com/BrechtDeMan/loudness.py/blob/master/loudness.py
		//   https://hydrogenaud.io/index.php?topic=86116.25

		audio_utils::InfiniteResponseFilter highShelfFilter { };
		audio_utils::InfiniteResponseFilter highPassFilter { };

		double intermediateRmsResult = 0.0;
		std::vector<float> intermediateWave { };

	public:
		void _process(array_view<float> wave, float average) override;
		void _setSamplesPerSec(index samplesPerSec) override;

	protected:
		void _reset() override;
		void finishBlock() override;
		sview getDefaultTransform() override;

	private:
		void preprocessWave(array_span<float> wave);
	};

}
