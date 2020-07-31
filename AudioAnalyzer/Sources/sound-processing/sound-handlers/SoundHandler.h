/*
 * Copyright (C) 2019-2020 rxtd
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
	class SoundHandler;

	class HandlerFinder {
		// NOLINT(cppcoreguidelines-special-member-functions)
	public:
		virtual ~HandlerFinder() = default;

		template <typename T = SoundHandler>
		[[nodiscard]]
		T* getHandler(isview id) const {
			// TODO remove template?
			return dynamic_cast<const T*>(getHandlerRaw(id));
		}

	protected:
		[[nodiscard]]
		virtual SoundHandler* getHandlerRaw(isview id) const = 0;
	};


	class SoundHandler {
	protected:
		using OptionMap = utils::OptionMap;
		using Rainmeter = utils::Rainmeter;
		using Logger = utils::Rainmeter::Logger;

	private:
		SoundHandler* source = nullptr;
		mutable bool valid = true;

	public:
		SoundHandler() = default;

		SoundHandler(const SoundHandler& other) = delete;
		SoundHandler(SoundHandler&& other) noexcept = delete;
		SoundHandler& operator=(const SoundHandler& other) = delete;
		SoundHandler& operator=(SoundHandler&& other) noexcept = delete;

		virtual ~SoundHandler() = default;

		virtual void setSamplesPerSec(index value) = 0;
		virtual void reset() = 0;

		void prePatch() {
			// this function exists because there is no united #patch() function: patchers are unique to handler type
			setValid(true);
		}

		void linkSources(HandlerFinder& hf, Logger& cl) {
			if (!isValid()) {
				return;
			}

			source = hf.getHandler(getSourceName());

			const bool success = vCheckSources(cl);
			setValid(success);
		}

		void process(const DataSupplier& dataSupplier) {
			if (!isValid()) {
				return;
			}

			_process(dataSupplier);
		}

		void finish() {
			if (!isValid()) {
				return;
			}

			_finish();
		}

		[[nodiscard]]
		bool isValid() const {
			return valid;
		}

		[[nodiscard]]
		virtual array_view<float> getData(index layer) const = 0;

		[[nodiscard]]
		virtual index getLayersCount() const {
			return 1;
		}

		[[nodiscard]]
		virtual index getStartingLayer() const {
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

		SoundHandler* getSource() const {
			return source;
		}

		[[nodiscard]]
		virtual isview getSourceName() const = 0;

		// method should return false if check failed, true otherwise
		[[nodiscard]]
		virtual bool vCheckSources(Logger& cl) = 0;

		virtual void _process(const DataSupplier& dataSupplier) = 0;

		// Method can be called several times in a row, handler should check for changes for optimal performance
		virtual void _finish() {
		}

		static index legacy_parseIndexProp(const isview& request, const isview& propName, index endBound) {
			return legacy_parseIndexProp(request, propName, 0, endBound);
		}

		static index legacy_parseIndexProp(const isview& request, const isview& propName, index minBound,
		                                   index endBound) {
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
