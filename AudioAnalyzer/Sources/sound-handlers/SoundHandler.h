/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../Channel.h"
#include "StringUtils.h"

namespace rxaa {
	class SoundHandler;

	class DataSupplier {
	public:
		virtual ~DataSupplier() = default;
		virtual const float* getWave() const = 0;
		virtual index getWaveSize()  const = 0;
		virtual const SoundHandler* getHandler(isview id) const = 0;
		virtual Channel getChannel() const = 0;

		/**
		 * returns array of size @code size.
		 * Can be called several times, each time buffer will be different.
		 * Released automatically after end of process() and processSilent().
		 */
		template<typename T>
		T* getBuffer(index size) const {
			return reinterpret_cast<T*>(getBufferRaw(size * sizeof(T)));
		}

	protected:
		virtual uint8_t* getBufferRaw(index size) const = 0;
	};

	class SoundHandler {
	public:
		SoundHandler() = default;

		SoundHandler(const SoundHandler& other) = delete;
		SoundHandler(SoundHandler&& other) noexcept = delete;
		SoundHandler& operator=(const SoundHandler& other) = delete;
		SoundHandler& operator=(SoundHandler&& other) noexcept = delete;

		virtual ~SoundHandler() = default;

		virtual void setSamplesPerSec(index samplesPerSec) = 0;
		virtual void reset() = 0;

		virtual void process(const DataSupplier &dataSupplier) = 0;
		virtual void processSilence(const DataSupplier &dataSupplier) = 0;

		// Method can be called several time, handler should check for changes
		virtual void finish(const DataSupplier &dataSupplier) = 0;

		virtual const double* getData() const = 0;
		virtual index getCount() const = 0;

		virtual const wchar_t* getProp(const isview& prop) const {
			return nullptr;
		}
		virtual bool isStandalone() {
			return false;
		}

	protected:
		static double calculateAttackDecayConstant(double time, index samplesPerSec, index stride) {
			return std::exp(-2.0 * stride / (samplesPerSec * time));
		}

		static double calculateAttackDecayConstant(double time, index samplesPerSec) {
			return calculateAttackDecayConstant(time, samplesPerSec, 1);
		}

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
