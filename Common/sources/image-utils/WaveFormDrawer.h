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

	private:
		std::pair<double, double> correctMinMaxPixels(double minPixel, double maxPixel) const;

		void fillStripBuffer(double min, double max);
	};
}
