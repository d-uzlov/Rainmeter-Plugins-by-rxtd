/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Vector2D.h"
#include "ImageTransposer.h"
#include "Color.h"

namespace rxtd::utils {
	class LinedImageHelper {
		Vector2D<Color> imageLines;
		index lastLineIndex = 0;
		mutable ImageTransposer transposer;
		Color backgroundValue = { };
		Color lastFillValue = { };
		index sameLinesCount = 0;

	public:
		void setBackground(Color value);

		void setImageWidth(index width);

		index getImageWidth() const;

		void setImageHeight(index height);

		index getImageHeight() const;

		array_span<Color> nextLine();

		void fillNextLineFlat(Color value);

		array_span<Color> fillNextLineManual();

		void writeTransposed(const string& filepath, bool withOffset, bool withFading) const;

		bool isEmpty() const;

		void collapseInto(array_span<Color> result) const;
	private:
		array_span<Color> nextLineNonBreaking();
	};

	class SupersamplingHelper {
		LinedImageHelper mainImage;
		LinedImageHelper supersamplingBuffer;
		index supersamplingSize = 0;
		index counter = 0;
	
	public:
		void setBackground(Color value) {
			mainImage.setBackground(value);
			supersamplingBuffer.setBackground(value);
		}
	
		void setImageWidth(index width) {
			mainImage.setImageWidth(width);
			supersamplingBuffer.setImageWidth(width);
		}
	
		void setImageHeight(index height) {
			mainImage.setImageHeight(height);
		}

		void setSupersamplingSize(index value) {
			supersamplingSize = value;
			supersamplingBuffer.setImageHeight(value);
		}
	
		array_span<Color> nextLine() {
			if (counter == supersamplingSize) {
				emitLine();
				counter = 0;
			}
			counter++;
			return supersamplingBuffer.nextLine();
		}
	
		void fillNextLineFlat(Color value) {
			if (counter == supersamplingSize) {
				emitLine();
				counter = 0;
			}
			counter++;
			supersamplingBuffer.fillNextLineFlat(value);
		}
	
		array_span<Color> fillNextLineManual() {
			if (counter == supersamplingSize) {
				emitLine();
				counter = 0;
			}
			counter++;
			return supersamplingBuffer.fillNextLineManual();
		}
	
		void writeTransposed(const string& filepath, bool withOffset, bool withFading) const {
			mainImage.writeTransposed(filepath, withOffset, withFading);
		}

	private:
		void emitLine() {
			std::vector<Color> colorLine;
			colorLine.resize(mainImage.getImageWidth());
			supersamplingBuffer.collapseInto(colorLine);

			if (supersamplingBuffer.isEmpty()) {
				auto line = mainImage.fillNextLineManual();
				for (int i = 0; i < mainImage.getImageWidth(); ++i) {
					line[i] = colorLine[i];
				}
			} else {
				auto line = mainImage.nextLine();
				for (int i = 0; i < mainImage.getImageWidth(); ++i) {
					line[i] = colorLine[i];
				}
			}
		}
	};
}
