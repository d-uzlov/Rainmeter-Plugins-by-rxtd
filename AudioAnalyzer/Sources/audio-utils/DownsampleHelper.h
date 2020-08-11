/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "butterworth-lib/ButterworthWrapper.h"
#include "filter-utils/InfiniteResponseFilter.h"

namespace rxtd::audio_utils {
	template <index filterOrder>
	class DownsampleHelper {
	public:
		struct ResampleResultInfo {
			index sourceConsumedSize = 0;
			index bufferFilledSize = 0;
		};

	private:
		index decimateFactor = 0;
		InfiniteResponseFilterFixed<filterOrder + 1> filter;
		index counter = 0;

	public:
		DownsampleHelper() {
			setFactor(2);
		}

		void setFactor(index value) {
			if (value == decimateFactor) {
				return;
			}

			decimateFactor = value;
			// digital frequency of (0.907 / decimateFactor) ensures strong cutoff at new nyquist frequency
			filter = { ButterworthWrapper::lowPass.calcCoefDigital(filterOrder, 0.907 / decimateFactor) };
			counter = 0;
		}

		[[nodiscard]]
		index calcBufferSizeFor(index sourceSize) const {
			return (counter + sourceSize) / decimateFactor;
		}

		// returns part of the wave that didn't fit info the buffer
		[[nodiscard]]
		ResampleResultInfo resample(array_view<float> source, array_span<float> buffer) {
			ResampleResultInfo result{ };

			const index loopLengthTillNextSample = decimateFactor - counter;
			const index initialUpdateLength = std::min(loopLengthTillNextSample, source.size());
			for (index i = 0; i < initialUpdateLength - 1; i++) {
				filter.next(source[i]);
			}

			if (initialUpdateLength != loopLengthTillNextSample) {
				filter.next(source[initialUpdateLength - 1]);
				counter += initialUpdateLength;

				result.sourceConsumedSize = initialUpdateLength;
				result.bufferFilledSize = 0;
				return result;
			}

			buffer[0] = float(filter.next(source[initialUpdateLength - 1]));

			buffer.remove_prefix(1);
			result.bufferFilledSize += 1;
			source.remove_prefix(initialUpdateLength);
			result.sourceConsumedSize += initialUpdateLength;

			const index effectiveBufferSize = buffer.size() * decimateFactor;
			const index copySize = std::min(effectiveBufferSize, source.size());
			const index copyResultSize = copySize / decimateFactor;

			for (index i = 0; i < copyResultSize; ++i) {
				for (int j = 0; j < decimateFactor - 1; ++j) {
					filter.next(source[i * decimateFactor + j]);
					// only use these values to update the state of the filter
				}
				buffer[i] = float(filter.next(source[i * decimateFactor + decimateFactor - 1]));
			}

			result.bufferFilledSize += copyResultSize;
			source.remove_prefix(copyResultSize * decimateFactor);
			result.sourceConsumedSize += copyResultSize * decimateFactor;

			counter = 0;
			for (float value : source) {
				filter.next(value);
				counter++;
			}
			result.sourceConsumedSize += counter;

			return result;
		}

		void reset() {
			filter.reset();
			counter = 0;
		}
	};

	template <index filterOrder, index decimateFactor>
	class DownsampleHelperFixed {
		static_assert(decimateFactor > 1);

	public:
		struct ResampleResultInfo {
			index sourceConsumedSize = 0;
			index bufferFilledSize = 0;
		};

	private:
		InfiniteResponseFilterFixed<filterOrder + 1> filter;
		index counter = 0;

	public:
		DownsampleHelperFixed() {
			// digital frequency of 0.907 / decimateFactor ensures strong cutoff at new nyquist frequency
			filter = { ButterworthWrapper::lowPass.calcCoefDigital(filterOrder, 0.907 / decimateFactor) };
		}

		[[nodiscard]]
		index calcBufferSizeFor(index sourceSize) const {
			return (counter + sourceSize) / decimateFactor;
		}

		// returns part of the wave that didn't fit info the buffer
		[[nodiscard]]
		ResampleResultInfo resample(array_view<float> source, array_span<float> buffer) {
			ResampleResultInfo result{ };

			const index loopLengthTillNextSample = decimateFactor - counter;
			const index initialUpdateLength = std::min(loopLengthTillNextSample, source.size());
			for (index i = 0; i < initialUpdateLength - 1; i++) {
				filter.next(source[i]);
			}

			if (initialUpdateLength != loopLengthTillNextSample) {
				filter.next(source[initialUpdateLength - 1]);
				counter += initialUpdateLength;

				result.sourceConsumedSize = initialUpdateLength;
				result.bufferFilledSize = 0;
				return result;
			}

			buffer[0] = float(filter.next(source[initialUpdateLength - 1]));

			buffer.remove_prefix(1);
			result.bufferFilledSize += 1;
			source.remove_prefix(initialUpdateLength);
			result.sourceConsumedSize += initialUpdateLength;

			const index effectiveBufferSize = buffer.size() * decimateFactor;
			const index copySize = std::min(effectiveBufferSize, source.size());
			const index copyResultSize = copySize / decimateFactor;

			for (index i = 0; i < copyResultSize; ++i) {
				for (int j = 0; j < decimateFactor - 1; ++j) {
					filter.next(source[i * decimateFactor + j]);
					// only use these values to update the state of the filter
				}
				buffer[i] = float(filter.next(source[i * decimateFactor + decimateFactor - 1]));
			}

			result.bufferFilledSize += copyResultSize;
			source.remove_prefix(copyResultSize * decimateFactor);
			result.sourceConsumedSize += copyResultSize * decimateFactor;

			const index remainderSize = copySize - copyResultSize * decimateFactor;
			for (index i = 0; i < remainderSize; ++i) {
				filter.next(source[i]);
			}
			result.sourceConsumedSize += remainderSize;

			counter = remainderSize;

			return result;
		}

		void reset() {
			filter.reset();
			counter = 0;
		}
	};
}
