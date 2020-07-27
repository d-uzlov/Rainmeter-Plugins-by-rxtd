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
#include "Color.h"
#include "IntMixer.h"

namespace rxtd::utils {
	class StripedImageFadeHelper {
	private:
		Vector2D<uint32_t> resultBuffer{ };

		index borderSize = 0;
		index lastStripIndex{ };
		double fading = 0.0;

		Color background;
		Color border;

	public:
		void setBorderSize(index value) {
			borderSize = value;
		}

		void setLastStripIndex(index value) {
			lastStripIndex = value;
		}

		void setFading(double value) {
			fading = value;
		}

		void setColors(Color _background, Color _border) {
			background = _background;
			border = _border;
		}

		array2d_view<uint32_t> getResultBuffer() const {
			return resultBuffer;
		}

		void inflate(array2d_view<IntColor> source);

	private:
		void inflateLine(array_view<IntColor> source, array_span<uint32_t> dest) {
			const index width = source.size();

			const double realWidth = width - borderSize;

			const index fadeEnd = realWidth * (1.0 - fading);

			const double distanceCoef = 1.0 / (realWidth * fading);
			IntMixer<> mixer;
			auto back = background.toIntColor();

			for (int i = 0; i < width; ++i) {
				index distance = lastStripIndex - i;
				if (distance < 0) {
					distance += width;
				}
				if (distance >= realWidth) {
					// this is border
					dest[i] = border.toInt();
					continue;
				}

				if (distance > fadeEnd) {
					double dd = (distance - fadeEnd) * distanceCoef;
					dd = dd * dd;

					mixer.setParams(dd);
					auto sc = source[i];
					sc.a = mixer.mix(back.a, sc.a);
					sc.r = mixer.mix(back.r, sc.r);
					sc.g = mixer.mix(back.g, sc.g);
					sc.b = mixer.mix(back.b, sc.b);
					dest[i] = sc.full;

					// dest[i] = Color::mix(distance, background, Color { source[i] }).toInt();
				} else {
					dest[i] = source[i].full;
				}

			}
		}

		static double distNone(double) {
			return 0.0;
		}

		static double distPow8(double value) {
			value = value * value;
			value = value * value;
			value = value * value;
			return value;
		}
	};
}
