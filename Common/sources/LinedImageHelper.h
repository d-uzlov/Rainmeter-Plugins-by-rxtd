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
		Vector2D<uint32_t> imageLines;
		index lastLineIndex = 0;
		mutable ImageTransposer transposer;
		uint32_t backgroundValue = { };
		uint32_t lastFillValue = { };
		index sameLinesCount = 0;

	public:
		void setBackground(uint32_t value);

		void setImageWidth(index width);

		index getImageWidth() const;

		void setImageHeight(index height);

		index getImageHeight() const;

		array_span<uint32_t> nextLine();

		void fillNextLineFlat(uint32_t value);

		array_span<uint32_t> fillNextLineManual();

		void writeTransposed(const string& filepath) const;

		bool isEmpty() const;

		void collapseInto(array_span<Color> result) const;
	private:
		array_span<uint32_t> nextLineNonBreaking();
	};

	class SupersamplingHelper {
		LinedImageHelper mainImage;
		LinedImageHelper supersamplingBuffer;
		index supersamplingSize = 0;
		index counter = 0;
	
	public:
		void setBackground(uint32_t value) {
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
	
		array_span<uint32_t> nextLine() {
			if (counter == supersamplingSize) {
				emitLine();
				counter = 0;
			}
			counter++;
			return supersamplingBuffer.nextLine();
		}
	
		void fillNextLineFlat(uint32_t value) {
			if (counter == supersamplingSize) {
				emitLine();
				counter = 0;
			}
			counter++;
			supersamplingBuffer.fillNextLineFlat(value);
		}
	
		array_span<uint32_t> fillNextLineManual() {
			if (counter == supersamplingSize) {
				emitLine();
				counter = 0;
			}
			counter++;
			return supersamplingBuffer.fillNextLineManual();
		}
	
		void writeTransposed(const string& filepath) const {
			mainImage.writeTransposed(filepath);
		}

	private:
		void emitLine() {
			std::vector<Color> colorLine;
			colorLine.resize(mainImage.getImageWidth());
			supersamplingBuffer.collapseInto(colorLine);

			if (supersamplingBuffer.isEmpty()) {
				auto line = mainImage.fillNextLineManual();
				for (int i = 0; i < mainImage.getImageWidth(); ++i) {
					line[i] = colorLine[i].toInt();
				}
			} else {
				auto line = mainImage.nextLine();
				for (int i = 0; i < mainImage.getImageWidth(); ++i) {
					line[i] = colorLine[i].toInt();
				}
			}
		}
	};
}
