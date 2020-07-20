// /*
//  * Copyright (C) 2020 rxtd
//  *
//  * This Source Code Form is subject to the terms of the GNU General Public
//  * License; either version 2 of the License, or (at your option) any later
//  * version. If a copy of the GPL was not distributed with this file, You can
//  * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
//  */
//
// #pragma once
// #include "Vector2D.h"
// #include "ImageTransposer.h"
// #include "Color.h"
// #include "BmpWriter.h"
//
// namespace rxtd::utils {
// 	template<typename C>
// 	class LinedImageHelper {
// 	protected:
// 		Vector2D<C> imageLines;
// 		index lastLineIndex = 0;
// 		mutable ImageTransposer transposer;
// 		C backgroundValue = { };
// 		C lastFillValue = { };
// 		index sameLinesCount = 0;
// 		mutable bool emptinessWritten = false;
//
// 	public:
// 		void setBackground(C value) {
// 			backgroundValue = value;
// 		}
//
// 		void setImageWidth(index width) {
// 			if (imageLines.getBufferSize() == width) {
// 				return;
// 			}
//
// 			imageLines.setBufferSize(width);
// 			imageLines.init(backgroundValue);
//
// 			lastFillValue = backgroundValue;
// 			sameLinesCount = imageLines.getBuffersCount();
// 			emptinessWritten = false;
// 		}
//
// 		index getImageWidth() const {
// 			return imageLines.getBufferSize();
// 		}
//
// 		void setImageHeight(index height) {
// 			if (imageLines.getBuffersCount() == height) {
// 				return;
// 			}
//
// 			imageLines.setBuffersCount(height);
// 			imageLines.init(backgroundValue);
//
// 			lastFillValue = backgroundValue;
// 			sameLinesCount = imageLines.getBuffersCount();
// 			emptinessWritten = false;
// 		}
//
// 		index getImageHeight() const {
// 			return imageLines.getBuffersCount();
// 		}
//
// 		array_span<C> nextLine() {
// 			sameLinesCount = 0;
// 			emptinessWritten = false;
// 			return nextLineNonBreaking();
// 		}
//
// 		void fillNextLineFlat(C value) {
// 			if (sameLinesCount == 0 || lastFillValue != value) {
// 				lastFillValue = value;
// 				sameLinesCount = 1;
// 			} else {
// 				if (sameLinesCount >= imageLines.getBuffersCount()) {
// 					return;
// 				}
// 				sameLinesCount++;
// 			}
// 			emptinessWritten = false;
//
// 			auto line = nextLineNonBreaking();
// 			std::fill(line.begin(), line.end(), value);
// 		}
//
// 		array_span<C> fillNextLineManual() {
// 			if (sameLinesCount < imageLines.getBuffersCount()) {
// 				sameLinesCount++;
// 				emptinessWritten = false;
// 			}
//
// 			return nextLineNonBreaking();
// 		}
//
// 		bool isEmpty() const {
// 			return sameLinesCount >= imageLines.getBuffersCount();
// 		}
//
// 	private:
// 		array_span<C> nextLineNonBreaking() {
// 			lastLineIndex++;
// 			if (lastLineIndex >= imageLines.getBuffersCount()) {
// 				lastLineIndex = 0;
// 			}
// 			return imageLines[lastLineIndex];
// 		}
// 	};
//
// 	class InflatableImageHelper {
// 		// [0.0, 1.0], where 0.0 is background, 1.0 is line
// 		// values in between are interpolation between colors for background and line
// 		Vector2D<float> imageLines;
// 		index lastLineIndex = 0;
// 		Vector2D<uint32_t> finalBuffer;
//
// 	public:
// 		void setImageWidth(index width) {
// 			if (imageLines.getBufferSize() == width) {
// 				return;
// 			}
//
// 			imageLines.setBufferSize(width);
// 			imageLines.init(0.0);
// 		}
//
// 		index getImageWidth() const {
// 			return imageLines.getBufferSize();
// 		}
//
// 		void setImageHeight(index height) {
// 			if (imageLines.getBuffersCount() == height) {
// 				return;
// 			}
//
// 			imageLines.setBuffersCount(height);
// 			imageLines.init(0.0);
// 		}
//
// 		index getImageHeight() const {
// 			return imageLines.getBuffersCount();
// 		}
//
// 		array_span<float> nextLine() {
// 			return nextLineNonBreaking();
// 		}
//
// 		void writeTransposed(const string& filepath, bool withOffset) const {
// 			index offset = lastLineIndex + 1;
// 			if (offset >= imageLines.getBuffersCount()) {
// 				offset = 0;
// 			}
// 			index gradientOffset = 0;
// 			if (!withOffset) {
// 				std::swap(offset, gradientOffset);
// 			}
//
// 			const auto width = imageLines.getBufferSize();
// 			const auto height = imageLines.getBuffersCount();
// 			transposer.transposeToBufferSimple(imageLines, offset);
//
// 			BmpWriter::writeFile(filepath, transposer.getBuffer()[0].data(), width, height);
//
// 			if (isEmpty()) {
// 				emptinessWritten = true;
// 			}
// 		}
//
// 	private:
// 		array_span<float> nextLineNonBreaking() {
// 			lastLineIndex++;
// 			if (lastLineIndex >= imageLines.getBuffersCount()) {
// 				lastLineIndex = 0;
// 			}
// 			return imageLines[lastLineIndex];
// 		}
// 	};
//
// 	class LinedImageHelperDynamic : public LinedImageHelper<float> {
// 		Color backgroundColor { };
// 		Color waveColor { };
//
// 	public:
// 		void setColors(Color background, Color wave) {
// 			backgroundColor = background;
// 			waveColor = wave;
// 		}
//
// 		void writeTransposed(const string& filepath, bool withOffset, bool withFading) const {
// 			if (emptinessWritten) {
// 				return;
// 			}
//
// 			index offset = lastLineIndex + 1;
// 			if (offset >= imageLines.getBuffersCount()) {
// 				offset = 0;
// 			}
// 			index gradientOffset = 0;
// 			if (!withOffset) {
// 				std::swap(offset, gradientOffset);
// 			}
//
// 			const auto width = imageLines.getBufferSize();
// 			const auto height = imageLines.getBuffersCount();
// 			transposer.transposeToBuffer(imageLines, offset, withFading, gradientOffset, backgroundColor, waveColor);
//
// 			BmpWriter::writeFile(filepath, transposer.getBuffer()[0].data(), width, height);
//
// 			if (isEmpty()) {
// 				emptinessWritten = true;
// 			}
// 		}
//
// 		void collapseInto(array_span<Color> result) const {
// 			std::fill(result.begin(), result.end(), Color { 0.0, 0.0, 0.0, 0.0 });
//
// 			for (int lineIndex = 0; lineIndex < imageLines.getBuffersCount(); lineIndex++) {
// 				const auto line = imageLines[lineIndex];
// 				for (int i = 0; i < line.size(); ++i) {
// 					result[i] = result[i] + line[i];
// 				}
// 			}
//
// 			const float coef = 1.0 / imageLines.getBuffersCount();
// 			for (int i = 0; i < imageLines.getBufferSize(); ++i) {
// 				result[i] = result[i] * coef;
// 			}
// 		}
// 	};
//
// 	class SupersamplingHelper {
// 		LinedImageHelperDynamic mainImage;
// 		LinedImageHelperDynamic supersamplingBuffer;
// 		index supersamplingSize = 0;
// 		index counter = 0;
// 	
// 	public:
// 		void setBackground(Color value) {
// 			mainImage.setBackground(value);
// 			supersamplingBuffer.setBackground(value);
// 		}
// 	
// 		void setImageWidth(index width) {
// 			mainImage.setImageWidth(width);
// 			supersamplingBuffer.setImageWidth(width);
// 		}
// 	
// 		void setImageHeight(index height) {
// 			mainImage.setImageHeight(height);
// 		}
//
// 		void setSupersamplingSize(index value) {
// 			supersamplingSize = value;
// 			supersamplingBuffer.setImageHeight(value);
// 		}
// 	
// 		array_span<Color> nextLine() {
// 			if (counter == supersamplingSize) {
// 				emitLine();
// 				counter = 0;
// 			}
// 			counter++;
// 			return supersamplingBuffer.nextLine();
// 		}
// 	
// 		void fillNextLineFlat(Color value) {
// 			if (counter == supersamplingSize) {
// 				emitLine();
// 				counter = 0;
// 			}
// 			counter++;
// 			supersamplingBuffer.fillNextLineFlat(value);
// 		}
// 	
// 		array_span<Color> fillNextLineManual() {
// 			if (counter == supersamplingSize) {
// 				emitLine();
// 				counter = 0;
// 			}
// 			counter++;
// 			return supersamplingBuffer.fillNextLineManual();
// 		}
// 	
// 		void writeTransposed(const string& filepath, bool withOffset, bool withFading) const {
// 			mainImage.writeTransposed(filepath, withOffset, withFading);
// 		}
//
// 	private:
// 		void emitLine() {
// 			std::vector<Color> colorLine;
// 			colorLine.resize(mainImage.getImageWidth());
// 			supersamplingBuffer.collapseInto(colorLine);
//
// 			if (supersamplingBuffer.isEmpty()) {
// 				auto line = mainImage.fillNextLineManual();
// 				for (int i = 0; i < mainImage.getImageWidth(); ++i) {
// 					line[i] = colorLine[i];
// 				}
// 			} else {
// 				auto line = mainImage.nextLine();
// 				for (int i = 0; i < mainImage.getImageWidth(); ++i) {
// 					line[i] = colorLine[i];
// 				}
// 			}
// 		}
// 	};
// }
