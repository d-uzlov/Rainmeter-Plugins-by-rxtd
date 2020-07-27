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
	public:
		enum class FadingType {
			eNONE,
			eLINEAR,
			ePOW2,
			ePOW4,
			ePOW8,
		};

	private:
		Vector2D<uint32_t> resultBuffer{ };

		index borderSize = 0;
		index lastStripIndex { };
		FadingType fading = FadingType::eNONE;

		Color background;
		Color border;

	public:
		void setBorderSize(index value) {
			borderSize = value;
		}

		void setLastStripIndex(index value) {
			lastStripIndex = value;
		}

		void setFading(FadingType value) {
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
			switch (fading) {
			case FadingType::eNONE:
				inflateLine<distNone>(source, dest);
				break;
			case FadingType::eLINEAR:
				inflateLine<distLinear>(source, dest);
				break;
			case FadingType::ePOW2:
				inflateLine<distPow2>(source, dest);
				break;
			case FadingType::ePOW4:
				inflateLine<distPow4>(source, dest);
				break;
			case FadingType::ePOW8:
				inflateLine<distPow8>(source, dest);
				break;
			default: ;
			}
		}

		template<double (*distanceTransformFunc)(double)>
		void inflateLine(array_view<IntColor> source, array_span<uint32_t> dest) {
			const index width = source.size();
			const double realWidth = width - borderSize;
			const double distanceCoef = 1.0 / realWidth;
			IntMixer<> mixer;
			auto back = background.toIntColor();

			for (int i = 0; i < width; ++i) {
				double distance = lastStripIndex - i;
				if (distance < 0.0) {
					distance += width;
				}
				if (distance >= realWidth) {
					// this is border
					dest[i] = border.toInt();
					continue;
				}

				distance *= distanceCoef;
				distance = distanceTransformFunc(distance);

				mixer.setParams(distance);
				auto sc = source[i];
				sc.a = mixer.mix(back.a, sc.a);
				sc.r = mixer.mix(back.r, sc.r);
				sc.g = mixer.mix(back.g, sc.g);
				sc.b = mixer.mix(back.b, sc.b);
				dest[i] = sc.full;

				// dest[i] = Color::mix(distance, background, Color { source[i] }).toInt();
			}
		}

		static double distNone(double) {
			return 0.0;
		}

		static double distLinear(double value) {
			return value;
		}

		static double distPow2(double value) {
			value = value * value;
			return value;
		}

		static double distPow4(double value) {
			value = value * value;
			value = value * value;
			return value;
		}

		static double distPow8(double value) {
			value = value * value;
			value = value * value;
			value = value * value;
			return value;
		}
	};
}
