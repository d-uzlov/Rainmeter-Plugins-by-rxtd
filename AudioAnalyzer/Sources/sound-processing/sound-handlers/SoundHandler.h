/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../DataSupplier.h"
#include "StringUtils.h"
#include "array_view.h"
#include "BufferPrinter.h"
#include "RainmeterWrappers.h"

namespace rxtd::audio_analyzer {
	class SoundHandler {
	protected:
		using OptionMap = utils::OptionMap;
		using Rainmeter = utils::Rainmeter;
		using Logger = utils::Rainmeter::Logger;

	private:
		mutable bool valid = true;

	public:
		using layer_t = int8_t;

		SoundHandler() = default;

		SoundHandler(const SoundHandler& other) = delete;
		SoundHandler(SoundHandler&& other) noexcept = delete;
		SoundHandler& operator=(const SoundHandler& other) = delete;
		SoundHandler& operator=(SoundHandler&& other) noexcept = delete;

		virtual ~SoundHandler() = default;

		virtual void setSamplesPerSec(index value) = 0;
		virtual void reset() = 0;

		void process(const DataSupplier& dataSupplier) {
			if (!valid) {
				return;
			}

			_process(dataSupplier);
		}

		void finish(const DataSupplier& dataSupplier) {
			if (!valid) {
				return;
			}

			_finish(dataSupplier);
		}

		virtual bool isValid() const {
			return valid;
		}

		virtual array_view<float> getData(layer_t layer) const = 0;

		virtual layer_t getLayersCount() const {
			return 1;
		}

		virtual layer_t getStartingLayer() const {
			return 0;
		}

		// return true if such prop exists, false otherwise
		virtual bool getProp(const isview& prop, utils::BufferPrinter& printer) const {
			return false;
		}

		virtual bool isStandalone() {
			return false;
		}

	protected:
		void setValid(bool value) const {
			valid = value;
		}

		virtual void _process(const DataSupplier& dataSupplier) = 0;

		// Method can be called several times in a row, handler should check for changes for optimal performance
		virtual void _finish(const DataSupplier& dataSupplier) { }

		static index parseIndexProp(const isview& request, const isview& propName, index endBound) {
			return parseIndexProp(request, propName, 0, endBound);
		}

		static index parseIndexProp(const isview& request, const isview& propName, index minBound, index endBound) {
			const auto indexPos = request.find(propName);

			if (indexPos != 0) {
				return -1;
			}

			const auto indexView = request.substr(propName.length());
			if (indexView.length() == 0) {
				return 0;
			}

			const auto cascadeIndex = utils::StringUtils::parseInt(indexView % csView());

			if (cascadeIndex < minBound || cascadeIndex >= endBound) {
				return -2;
			}

			return cascadeIndex;
		}
	};
}
