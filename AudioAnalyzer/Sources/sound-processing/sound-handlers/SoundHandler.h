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
 First of all, handler should parse its parameters. It happens in the static method #parseParams().
 Then the main life cycle happens, which consists of several phases.
 1. Function #patchMe function is called.
		Handler should assume that any external resources that it could potentially have links to don't exist anymore
 2. Function #vFinishLinking is called
		At this point:
			- Channel and sample rate are known, and handler may use these values.
			- Source is also known and either valid or doesn't exist.
			  If handler relies on source, then it should check it
		Handler is recalculating data that depend on values above.
		If something fails, handler is invalidated until next params change
 3. vProcess happens in the loop
 4. Data is accessed from other handlers and child measures.
		Before data is accessed vFinish method is called. // todo remove
 */

#pragma once
#include <atomic>


#include "array2d_view.h"
#include "../DataSupplier.h"
#include "array_view.h"
#include "BufferPrinter.h"
#include "RainmeterWrappers.h"
#include "Vector2D.h"

namespace rxtd::audio_analyzer {
	class SoundHandler;

	class HandlerFinder {
	public:
		virtual ~HandlerFinder() = default;

		[[nodiscard]]
		virtual SoundHandler* getHandler(isview id) const = 0;
	};

	class HandlerPatcher {
	public:
		virtual ~HandlerPatcher() = default;

		// takes old pointer and returns new
		//	- if returned pointer is different from the old, then new object was created,
		//		and the caller must release resources associated with old pointer
		//
		//	- if returned pointer is the same, then the old object was reused
		virtual SoundHandler* patch(SoundHandler* handlerPtr) const = 0;
	};

	class LayerDataId {
		using idType = uint32_t;

		static std::atomic<idType> sourceIdCounter;

		idType sourceId{ };
		idType dataId{ };

		LayerDataId(idType handlerId, idType dataId) : sourceId(handlerId), dataId(dataId) {
		}

	public:
		LayerDataId() = default;

		static LayerDataId createNext() {
			const auto nextId = sourceIdCounter.fetch_add(1);
			return { nextId + 1, 0 };
		}

		void advance() {
			dataId++;
		}

		void assign(LayerDataId other) {
			dataId = other.dataId;
		}

		// autogenerated
		friend bool operator==(const LayerDataId& lhs, const LayerDataId& rhs) {
			return lhs.sourceId == rhs.sourceId
				&& lhs.dataId == rhs.dataId;
		}

		friend bool operator!=(const LayerDataId& lhs, const LayerDataId& rhs) {
			return !(lhs == rhs);
		}

		// LayerDataId(const LayerDataId& other) = default;
		// LayerDataId& operator=(const LayerDataId& other) = default;
	};

	class SoundHandler {
	public:
		struct DataSize {
			index layersCount{ };
			index valuesCount{ };

			DataSize() = default;

			DataSize(index layersCount, index valuesCount)
				: layersCount(layersCount),
				  valuesCount(valuesCount) {
			}
		};

		struct LayeredData2 {
			utils::array2d_view<float> values;
			array_view<LayerDataId> ids;
		};

	protected:
		using OptionMap = utils::OptionMap;
		using Rainmeter = utils::Rainmeter;
		using Logger = utils::Rainmeter::Logger;

		struct LinkingResult {
			bool success = false;
			DataSize dataSize{ };

			LinkingResult() = default;

			LinkingResult(DataSize dataSize): success(true), dataSize(dataSize) {
			}

			LinkingResult(index layersCount, index valuesCount): LinkingResult(DataSize{ layersCount, valuesCount }) {
			}
		};

	public:
		template <typename HandlerType>
		class HandlerPatcherImpl : public HandlerPatcher {
			typename HandlerType::Params params{ };
			bool valid = false;

		public:
			HandlerPatcherImpl(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain) {
				HandlerType temp1{ };
				SoundHandler& temp2 = temp1;
				valid = temp2.parseParams(optionMap, cl, rain, &params);
			}

			[[nodiscard]]
			bool isValid() const {
				return valid;
			}

			virtual ~HandlerPatcherImpl() = default;

		private:
			friend SoundHandler;

			[[nodiscard]]
			SoundHandler* patch(SoundHandler* handlerPtr) const override {
				auto ptr = dynamic_cast<HandlerType*>(handlerPtr);
				if (ptr == nullptr) {
					ptr = new HandlerType();
				}

				if (ptr->getParams() != params) {
					ptr->setParams(params);
				}

				return ptr;
			}
		};

	private:
		LayerDataId _generatorId;
		bool _valid = true;
		DataSize _dataSize{ };
		utils::Vector2D<float> _values;
		std::vector<LayerDataId> _idsRef;
		std::vector<LayerDataId> _ids;

		SoundHandler* _sourceHandler = nullptr;
		index _sampleRate{ };
		Channel _channel{ };

	public:
		SoundHandler() {
			_generatorId = LayerDataId::createNext();
		}

		SoundHandler(const SoundHandler& other) = delete;
		SoundHandler(SoundHandler&& other) noexcept = delete;
		SoundHandler& operator=(const SoundHandler& other) = delete;
		SoundHandler& operator=(SoundHandler&& other) noexcept = delete;

		virtual ~SoundHandler() = default;

	private:
		// All derived classes should have methods with following signatures

		/*
		const Params& getParams() const;
		void patchMe(const Params& _params);
		 */

		// I can't declare these as virtual functions
		// because Params structs are defined in derived classes,
		// and C++ doesn't support template virtual functions

		template <typename>
		friend class HandlerPatcherImpl;

		// must return true if all options are valid, false otherwise
		virtual bool parseParams(
			const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr
		) const = 0;

	public:
		[[nodiscard]]
		static SoundHandler* patch(
			SoundHandler* old, HandlerPatcher& patcher,
			Channel channel, index sampleRate,
			HandlerFinder& hf,
			Logger& cl
		);

	protected:
		[[nodiscard]]
		virtual isview vGetSourceName() const = 0;

		// method should return false if check failed, true otherwise
		[[nodiscard]]
		virtual LinkingResult vFinishLinking(Logger& cl) = 0;

		[[nodiscard]]
		array_span<float> generateLayerData(index layer) {
			_generatorId.advance();
			_ids[layer] = _generatorId;
			return _values[layer];
		}

		[[nodiscard]]
		array_span<float> updateLayerData(index layer, LayerDataId sourceId) {
			_idsRef[layer] = sourceId;
			_ids[layer].assign(sourceId);
			return _values[layer];
		}

		[[nodiscard]]
		array_view<LayerDataId> getRefIds() const {
			return _idsRef;
		}

	public:
		[[nodiscard]]
		DataSize getDataSize() const {
			return _dataSize;
		}

		[[nodiscard]]
		LayeredData2 getData() const {
			return { _values, _ids };
		}

		void process(const DataSupplier& dataSupplier) {
			if (!isValid()) {
				return;
			}

			vProcess(dataSupplier);
		}

		void finish() {
			if (!isValid()) {
				return;
			}

			vFinish();
		}

		void reset() {
			// todo do we even need reset at all?
			_values.init(0.0f);
			vReset();
		}

		[[nodiscard]]
		bool isValid() const {
			return _valid;
		}

		[[nodiscard]]
		virtual index getStartingLayer() const {
			return 0;
		}

		// return true if such prop exists, false otherwise
		[[nodiscard]]
		virtual bool vGetProp(const isview& prop, utils::BufferPrinter& printer) const {
			return false;
		}

		[[nodiscard]]
		virtual bool vIsStandalone() {
			return false;
		}

	protected:
		void setValid(bool value) {
			_valid = value;
		}

		[[nodiscard]]
		SoundHandler* getSource() const {
			return _sourceHandler;
		}

		[[nodiscard]]
		index getSampleRate() const {
			return _sampleRate;
		}

		[[nodiscard]]
		Channel getChannel() const {
			return _channel;
		}

		virtual void vProcess(const DataSupplier& dataSupplier) = 0;

		// Method can be called several times in a row, handler should check for changes for optimal performance
		virtual void vFinish() {
		}

		virtual void vReset() {
		}

		static index legacy_parseIndexProp(const isview& request, const isview& propName, index endBound) {
			return legacy_parseIndexProp(request, propName, 0, endBound);
		}

		static index legacy_parseIndexProp(
			const isview& request,
			const isview& propName,
			index minBound, index endBound
		);
	};
}
