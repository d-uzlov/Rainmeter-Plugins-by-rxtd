/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "StripedImage.h"
#include "DiscreetInterpolator.h"
#include "Color.h"

namespace rxtd::utils {
	class WaveFormDrawer {
	public:
		enum class LineDrawingPolicy {
			NEVER,
			BELOW_WAVE,
			ALWAYS,
		};

	private:
		StripedImage<float> inflatableBuffer{ };
		Vector2D<uint32_t> resultBuffer{ };
		std::vector<float> stripBuffer{ };
		DiscreetInterpolator interpolator;
		double gain = 1.0; // TODO remove

		index width{ };
		index height{ };

		LineDrawingPolicy lineDrawingPolicy = LineDrawingPolicy::NEVER;
		bool edgeAntialiasing = false;

		struct {
			Color background;
			Color wave;
			Color line;
		} colors;

	public:
		WaveFormDrawer();

		void setGain(double value) {
			gain = value;
		}

		void setEdgeAntialiasing(bool value) {
			edgeAntialiasing = value;
		}

		void setLineDrawingPolicy(LineDrawingPolicy value) {
			lineDrawingPolicy = value;
		}

		void setColors(Color background, Color wave, Color line) {
			colors.background = background;
			colors.wave = wave;
			colors.line = line;
		}

		void setDimensions(index width, index height);

		void fillSilence();

		void fillStrip(double min, double max);

		array2d_view<uint32_t> getResultBuffer() const {
			return resultBuffer;
		}

		bool isEmpty() const {
			return inflatableBuffer.isEmpty();
		}

		void inflate();

		void fillStripBuffer(double min, double max) {
			auto& buffer = stripBuffer;

			min *= gain;
			max *= gain;

			min = std::clamp(min, -1.0, 1.0);
			max = std::clamp(max, -1.0, 1.0);
			min = std::min(min, max);
			max = std::max(min, max);

			auto minPixel = interpolator.toValue(min);
			auto maxPixel = interpolator.toValue(max);
			const auto minMaxDelta = maxPixel - minPixel;
			if (minMaxDelta < 1) {
				const auto average = (minPixel + maxPixel) * 0.5;
				minPixel = average - 0.5;
				maxPixel = minPixel + 1.0; // max should always be >= min + 1

				const auto minD = interpolator.makeDiscreet(minPixel);
				const auto minDC = interpolator.makeDiscreetClamped(minPixel);
				if (minD != minDC) {
					const double shift = 1.0 - interpolator.percentRelativeToNext(minPixel);
					minPixel += shift;
					maxPixel += shift;
				}

				const auto maxD = interpolator.makeDiscreet(maxPixel);
				const auto maxDC = interpolator.makeDiscreetClamped(maxPixel);
				if (maxD != maxDC) {
					const double shift = -interpolator.percentRelativeToNext(maxPixel);
					minPixel += shift;
					maxPixel += shift;
				}
			}

			const auto lowBackgroundBound = interpolator.makeDiscreetClamped(minPixel);
			const auto highBackgroundBound = interpolator.makeDiscreetClamped(maxPixel);

			// [0, lowBackgroundBound) ← background
			// [lowBackgroundBound, lowBackgroundBound] ← transition
			// [lowBackgroundBound + 1, highBackgroundBound) ← line
			// [highBackgroundBound, highBackgroundBound] ← transition
			// [highBackgroundBound + 1, MAX] ← background

			// Should never really clamp due to minPixel <= maxPixel - 1
			// Because discreet(maxPixel) and discreet(minPixel) shouldn't clamp after shift above
			// If (max - min) was already >= 1.0, then also no clamp because discreet(toValue([-1.0, 1.0])) should be within clamp range
			const auto lowLineBound = interpolator.clamp(lowBackgroundBound + 1);
			const auto highLineBound = highBackgroundBound;

			for (index i = 0; i < lowBackgroundBound; ++i) {
				buffer[i] = 0.0;
			}
			for (index i = highBackgroundBound + 1; i < height; ++i) {
				buffer[i] = 0.0;
			}

			// if (params.lineDrawingPolicy == LineDrawingPolicy::BELOW_WAVE) {
			// 	const index centerPixel = interpolator.toValueD(0.0);
			// 	buffer[centerPixel] = params.lineColor;
			// }

			const double lowPercent = interpolator.percentRelativeToNext(minPixel);
			if (edgeAntialiasing) {
				buffer[lowBackgroundBound] = 1.0 - lowPercent;
			} else {
				if (lowPercent < 0.5) {
					buffer[lowBackgroundBound] = 1.0;
				} else {
					buffer[lowBackgroundBound] = 0.0;
				}
			}

			for (index i = lowLineBound; i < highLineBound; i++) {
				buffer[i] = 1.0;
			}

			const double highPercent = interpolator.percentRelativeToNext(maxPixel);

			if (edgeAntialiasing) {
				// const auto highTransitionColor = params.backgroundColor * (1.0 - highPercent) + params.waveColor * highPercent;
				buffer[highBackgroundBound] = highPercent;
			} else {
				if (highPercent > 0.5) {
					buffer[highBackgroundBound] = 1.0;
				} else {
					buffer[highBackgroundBound] = 0.0;
				}
			}

			// if (params.lineDrawingPolicy == LineDrawingPolicy::ALWAYS) {
			// 	const index centerPixel = interpolator.toValueD(0.0);
			// 	buffer[centerPixel] = params.lineColor;
			// }
		}
	};
}
