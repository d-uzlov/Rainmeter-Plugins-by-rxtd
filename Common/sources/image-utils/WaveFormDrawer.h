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

		enum class FadingType {
			eNONE,
			eLINEAR,
			ePOW2,
			ePOW4,
			ePOW8,
		};

	private:
		StripedImage<float> inflatableBuffer{ };
		Vector2D<uint32_t> resultBuffer{ };
		std::vector<float> stripBuffer{ };
		DiscreetInterpolator interpolator;

		index width{ };
		index height{ };

		LineDrawingPolicy lineDrawingPolicy = LineDrawingPolicy::NEVER;
		bool edgeAntialiasing = false;
		bool connected = false;
		FadingType fading = FadingType::eNONE;

		struct {
			Color background;
			Color wave;
			Color line;
		} colors;

		struct {
			double min = 0.0;
			double max = 0.0;
		} prev;

	public:
		WaveFormDrawer();

		void setEdgeAntialiasing(bool value) {
			edgeAntialiasing = value;
		}

		void setConnected(bool value) {
			connected = value;
		}

		void setFading(FadingType value) {
			fading = value;
		}

		void setStationary(bool value) {
			inflatableBuffer.setStationary(value);
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
		void inflateLine(array_view<float> source, array_span<uint32_t> dest) {
			switch (fading) {
			case FadingType::eNONE:
				inflateLineNoFade(source, dest);
				break;
			case FadingType::eLINEAR:
				inflateLinePow1(source, dest);
				break;
			case FadingType::ePOW2:
				inflateLinePow2(source, dest);
				break;
			case FadingType::ePOW4:
				inflateLinePow4(source, dest);
				break;
			case FadingType::ePOW8:
				inflateLinePow8(source, dest);
				break;
			default: ;
			}
		}

		void inflateLineNoFade(array_view<float> source, array_span<uint32_t> dest);

		void inflateLinePow1(array_view<float> source, array_span<uint32_t> dest);

		void inflateLinePow2(array_view<float> source, array_span<uint32_t> dest);

		void inflateLinePow4(array_view<float> source, array_span<uint32_t> dest);

		void inflateLinePow8(array_view<float> source, array_span<uint32_t> dest);

		std::pair<double, double> correctMinMaxPixels(double minPixel, double maxPixel) const;

		void fillStripBuffer(double min, double max);
	};
}
