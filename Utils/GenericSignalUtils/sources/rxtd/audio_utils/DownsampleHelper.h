/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "rxtd/GrowingVector.h"
#include "rxtd/filter_utils/InfiniteResponseFilter.h"
#include "rxtd/filter_utils/butterworth_lib/ButterworthWrapper.h"

namespace rxtd::audio_utils {
	class DownsampleHelper {
		using ButterworthWrapper = filter_utils::butterworth_lib::ButterworthWrapper;
		template<index order>
		using InfiniteResponseFilterFixed = filter_utils::InfiniteResponseFilterFixed<order>;
		
		constexpr static index filterOrder = 10;
		constexpr static index filterSize = ButterworthWrapper::oneSideSlopeSize(filterOrder);

		index decimateFactor = 0;
		GrowingVector<float> buffer;
		InfiniteResponseFilterFixed<filterSize> filter1;
		InfiniteResponseFilterFixed<filterSize> filter2;
		InfiniteResponseFilterFixed<filterSize> filter3;

	public:
		DownsampleHelper() {
			setFactor(2);
		}

		DownsampleHelper(index factor) {
			setFactor(factor);
		}

		void setFactor(index value) {
			if (value == decimateFactor) {
				return;
			}

			decimateFactor = value;
			// digital frequency of 0.95 / decimateFactor ensures strong cutoff at new nyquist frequency
			const double digitalCutoff = 0.95 / static_cast<double>(decimateFactor);
			filter1 = { ButterworthWrapper::lowPass.calcCoefDigital(filterOrder, digitalCutoff) };
			filter2 = filter1;
			filter3 = filter1;
		}

		// returns size of the buffer required to grab all of the data downsampled
		[[nodiscard]]
		index pushData(array_view<float> source) {
			buffer.compact();

			auto chunk = buffer.allocateNext(source.size());
			source.transferToSpan(chunk);
			filter1.apply(chunk);
			filter2.apply(chunk);
			filter3.apply(chunk);

			return buffer.getRemainingSize() / decimateFactor;
		}

		// returns count of downsampled elements
		index downsample(array_span<float> dest) {
			const index size = buffer.getRemainingSize();
			const index resultSize = std::min(size / decimateFactor, dest.size());
			const index sourceGrabSize = resultSize * decimateFactor;

			const auto data = buffer.removeFirst(sourceGrabSize);
			for (index i = 0, j = 0; i < resultSize; i++, j += decimateFactor) {
				dest[i] = data[j];
			}

			return resultSize;
		}

		// returns count of downsampled elements
		template<index fixedFactor>
		index downsampleFixed(array_span<float> dest) {
			const index size = buffer.getRemainingSize();
			const index resultSize = std::min(size / fixedFactor, dest.size());
			const index sourceGrabSize = resultSize * fixedFactor;

			const auto data = buffer.removeFirst(sourceGrabSize);
			for (index i = 0, j = 0; i < resultSize; i++, j += fixedFactor) {
				dest[i] = data[j];
			}

			return resultSize;
		}

		void reset() {
			buffer.reset();
			filter1.reset();
			filter2.reset();
			filter3.reset();
		}
	};
}
