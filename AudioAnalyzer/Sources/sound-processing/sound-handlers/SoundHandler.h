/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

/*
 Handler life cycle
 First of all, handler should parse its parameters. It happens in the method #parseParams(),
 which is called on an empty object.
 If params are invalid, then parseParams should return invalid object,
 so that handler with these params will not be created.
 Then the main life cycle happens:
 1. Function #setParams is called.
		This function should only save parameters, any external info in the moment of time when this function is called is considered invalid
 2. Function #vConfigure is called
		At this point handler should do all calculations required to work.
		Handler may call #vConfigure to get external info
		If something fails, handler should return invalid object, and the it is invalidated until next params change
 3. vProcess happens in the loop
 4. Data is accessed from other handlers and child measures.
 */

#pragma once
#include <any>
#include <chrono>
#include <utility>

#include "BufferPrinter.h"
#include "RainmeterWrappers.h"
#include "Vector2D.h"
#include "option-parser/OptionMap.h"

namespace rxtd::audio_analyzer {
	class SoundHandler;

	class HandlerFinder {
	public:
		virtual ~HandlerFinder() = default;

		[[nodiscard]]
		virtual SoundHandler* getHandler(isview id) const = 0;
	};

	class SoundHandler {
	public:
		using OptionMap = utils::OptionMap;
		using Rainmeter = utils::Rainmeter;
		using Logger = utils::Rainmeter::Logger;
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		struct DataSize {
			index valuesCount{ };
			index layersCount{ };
			std::vector<index> eqWaveSizes;

			DataSize() = default;

			DataSize(index valuesCount, std::vector<index> _eqWaveSizes) :
				valuesCount(valuesCount),
				eqWaveSizes(std::move(_eqWaveSizes)) {
				layersCount = eqWaveSizes.size();
			}

			[[nodiscard]]
			bool isEmpty() const {
				return eqWaveSizes.empty() || valuesCount == 0;
			}
		};

		struct ExternCallContext {
			sview channelName{ };
		};

		struct ProcessContext {
			array_view<float> wave;

			struct {
				array_view<float> data;
				float min{ };
				float max{ };
			} originalWave;

			clock::time_point killTime;
		};

		struct ExternalMethods {
			using FinishMethodType = void(*)(const std::any& data, const ExternCallContext& context);
			using GetPropMethodType = bool(*)(
				const std::any& data,
				isview prop,
				utils::BufferPrinter& printer,
				const ExternCallContext& context
			);

			FinishMethodType finish = nullptr;

			// return true if such prop exists, false otherwise
			GetPropMethodType getProp = nullptr;

			// autogenerated
			friend bool operator==(const ExternalMethods& lhs, const ExternalMethods& rhs) {
				return lhs.finish == rhs.finish
					&& lhs.getProp == rhs.getProp;
			}

			friend bool operator!=(const ExternalMethods& lhs, const ExternalMethods& rhs) {
				return !(lhs == rhs);
			}
		};

		struct ParseResult {
			bool valid = false;
			std::any params;
			std::vector<istring> sources;
			ExternalMethods externalMethods{ };

			ParseResult(bool isValid = false) {
				valid = isValid;
			}
		};

		struct Snapshot {
			utils::Vector2D<float> values;
			std::any handlerSpecificData;
		};

	protected:
		struct Configuration {
			SoundHandler* sourcePtr = nullptr;
			index sampleRate{ };

			// autogenerated
			friend bool operator==(const Configuration& lhs, const Configuration& rhs) {
				return lhs.sourcePtr == rhs.sourcePtr
					&& lhs.sampleRate == rhs.sampleRate;
			}

			friend bool operator!=(const Configuration& lhs, const Configuration& rhs) {
				return !(lhs == rhs);
			}
		};

		struct ConfigurationResult {
			bool success = false;
			DataSize dataSize{ };

			ConfigurationResult() = default;

			ConfigurationResult(DataSize dataSize): success(true), dataSize(std::move(dataSize)) {
			}

			ConfigurationResult(index valuesCount, std::vector<index> eqWaveSizes):
				ConfigurationResult(DataSize{ valuesCount, std::move(eqWaveSizes) }) {
			}
		};

	private:
		struct LayerCache {
			mutable std::vector<array_view<float>> chunksView;
			std::vector<index> offsets;
		};

		bool _anyChanges = false;
		DataSize _dataSize{ };
		mutable bool _layersAreValid = false;
		std::vector<float> _buffer;
		std::vector<LayerCache> _layers;
		utils::Vector2D<float> _lastResults;

		Configuration _configuration{ };

	public:
		virtual ~SoundHandler() = default;

		template <typename _HandlerType>
		[[nodiscard]]
		static std::unique_ptr<SoundHandler> patchHandlerImpl(std::unique_ptr<SoundHandler> handlerPtr) {
			using HandlerType = _HandlerType;

			SoundHandler* ptr = dynamic_cast<HandlerType*>(handlerPtr.get());
			if (ptr == nullptr) {
				ptr = new HandlerType();
				handlerPtr = std::unique_ptr<SoundHandler>{ ptr };
			}

			return handlerPtr;
		}

		[[nodiscard]]
		virtual ParseResult
		parseParams(const OptionMap& om, Logger& cl, const Rainmeter& rain, index legacyNumber) const = 0;

		// returns true on success, false on invalid handler
		[[nodiscard]]
		bool patch(
			const std::any& params, const std::vector<istring>& sources,
			index sampleRate,
			HandlerFinder& hf, Logger& cl,
			Snapshot& snapshot
		);

		void finishConfiguration() {
			_anyChanges = false;
		}

		void process(ProcessContext context, Snapshot& snapshot) {
			clearChunks();

			vProcess(context, snapshot.handlerSpecificData);

			for (index layer = 0; layer < index(_dataSize.eqWaveSizes.size()); layer++) {
				auto chunks = _layers[layer].chunksView;
				if (!chunks.empty()) {
					snapshot.values[layer].copyFrom(chunks.back());
				} else {
					snapshot.values[layer].copyFrom(_lastResults[layer]);
				}
			}
		}

		// following public members are public for access between handlers
		[[nodiscard]]
		const DataSize& getDataSize() const {
			return _dataSize;
		}

		[[nodiscard]]
		virtual index getStartingLayer() const {
			return _configuration.sourcePtr == nullptr ? 0 : _configuration.sourcePtr->getStartingLayer();
		}

		[[nodiscard]]
		array_view<array_view<float>> getChunks(index layer) const {
			if (layer >= index(_dataSize.eqWaveSizes.size())) {
				return { };
			}

			inflateLayers();
			return _layers[layer].chunksView;
		}

		// returns saved data from previous iteration
		[[nodiscard]]
		array_view<float> getSavedData(index layer) const {
			if (layer >= index(_dataSize.eqWaveSizes.size())) {
				return { };
			}

			return _lastResults[layer];
		}


	protected:
		template <typename Params>
		static bool compareParamsEquals(const Params& p1, const std::any& p2) {
			return p1 == *std::any_cast<Params>(&p2);
		}

		template <typename DataStructType, auto methodPtr>
		static auto wrapExternalMethod() {
			if constexpr (std::is_invocable<
					decltype(methodPtr),
					const DataStructType&,
					const ExternCallContext&>::value
			) {
				return [](const std::any& handlerSpecificData, const ExternCallContext& context) {
					return methodPtr(*std::any_cast<DataStructType>(&handlerSpecificData), context);
				};
			} else if constexpr (
				std::is_invocable_r<
					bool,
					decltype(methodPtr),
					const DataStructType&, isview, utils::BufferPrinter&, const ExternCallContext&>::value
			) {
				return [](
					const std::any& handlerSpecificData,
					isview prop,
					utils::BufferPrinter& bp,
					const ExternCallContext& context
				) {
					return methodPtr(*std::any_cast<DataStructType>(&handlerSpecificData), prop, bp, context);
				};
			} else {
				static_assert(false, L"wrapExternalMethod: unsupported method");
			}
		}

		// should return true when params are the same
		[[nodiscard]]
		virtual bool checkSameParams(const std::any& p) const = 0;

		[[nodiscard]]
		virtual ConfigurationResult vConfigure(const std::any& _params, Logger& cl, std::any& snapshotAny) = 0;

		[[nodiscard]]
		const Configuration& getConfiguration() const {
			return _configuration;
		}

		[[nodiscard]]
		array_span<float> pushLayer(index layer) {
			const index offset = index(_buffer.size());
			_buffer.resize(offset + _dataSize.valuesCount);
			_layersAreValid = false;

			_layers[layer].offsets.push_back(offset);

			return { _buffer.data() + offset, _dataSize.valuesCount };
		}

		// if handler is potentially heavy,
		// handler should try to return control to caller
		// when time is more than context.killTime
		virtual void vProcess(ProcessContext context, std::any& handlerSpecificData) = 0;

		static index legacy_parseIndexProp(const isview& request, const isview& propName, index endBound) {
			return legacy_parseIndexProp(request, propName, 0, endBound);
		}

		static index legacy_parseIndexProp(
			const isview& request,
			const isview& propName,
			index minBound, index endBound
		);

	private:
		void inflateLayers() const {
			if (_layersAreValid) {
				return;
			}

			for (auto& data : _layers) {
				data.chunksView.resize(data.offsets.size());
				for (index i = 0; i < index(data.offsets.size()); i++) {
					data.chunksView[i] = { _buffer.data() + data.offsets[i], _dataSize.valuesCount };
				}
			}

			_layersAreValid = true;
		}

		void clearChunks() {
			for (index layer = 0; layer < _dataSize.layersCount; layer++) {
				auto& offsets = _layers[layer].offsets;
				if (!offsets.empty()) {
					_lastResults[layer].copyFrom({ _buffer.data() + offsets.back(), _dataSize.valuesCount });
				}
				offsets.clear();
			}

			_layersAreValid = false;

			_buffer.clear();
		}
	};
}
