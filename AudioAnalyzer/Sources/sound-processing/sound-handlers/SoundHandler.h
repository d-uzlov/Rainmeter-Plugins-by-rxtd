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
#include "../DataSupplier.h"
#include "StringUtils.h"
#include "array_view.h"
#include "BufferPrinter.h"
#include "RainmeterWrappers.h"

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

	struct LayerData {
		array_view<float> values;
		uint32_t id{ }; // todo what if source changed and new source has the same id but different data?
	};

	using LayeredData = array_view<LayerData>;

	class SoundHandler {
	public:
		struct DataSize {
			index layersCount{ };
			index valuesCount{ };
		};

	protected:
		using OptionMap = utils::OptionMap;
		using Rainmeter = utils::Rainmeter;
		using Logger = utils::Rainmeter::Logger;

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
		SoundHandler* _sourceHandler = nullptr;
		index _sampleRate{ };
		Channel _channel{ };
		mutable bool valid = true;

	public:
		SoundHandler() = default;

		SoundHandler(const SoundHandler& other) = delete;
		SoundHandler(SoundHandler&& other) noexcept = delete;
		SoundHandler& operator=(const SoundHandler& other) = delete;
		SoundHandler& operator=(SoundHandler&& other) noexcept = delete;

		virtual ~SoundHandler() = default;

	private:
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
		) {
			auto& result = *patcher.patch(old);
			result._sourceHandler = hf.getHandler(result.vGetSourceName());
			result._sampleRate = sampleRate;
			result._channel = channel;

			const bool success = result.vFinishLinking(cl);
			result.setValid(success);
			return &result;
		}

		/*
		 * All derived classes should have methods with following signatures
		 * I can't declare these as pure virtual functions
		 * because Params class is defined in derived class,
		 * and C++ doesn't support template virtual functions

		const Params& getParams() const;
		void patchMe(const Params& _params);
		
		 */

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

		[[nodiscard]]
		bool isValid() const {
			return valid;
		}

		virtual void vReset() = 0;

		[[nodiscard]]
		virtual LayeredData vGetData() const = 0;

		[[nodiscard]]
		virtual index getStartingLayer() const {
			// todo remove
			return 0;
		}

		[[nodiscard]]
		virtual DataSize getDataSize() const = 0;

		// return true if such prop exists, false otherwise
		virtual bool vGetProp(const isview& prop, utils::BufferPrinter& printer) const {
			return false;
		}

		virtual bool vIsStandalone() {
			return false;
		}

	protected:
		void setValid(bool value) const {
			valid = value;
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

		[[nodiscard]]
		virtual isview vGetSourceName() const = 0;

		// method should return false if check failed, true otherwise
		[[nodiscard]]
		virtual bool vFinishLinking(Logger& cl) = 0;

		virtual void vProcess(const DataSupplier& dataSupplier) = 0;

		// Method can be called several times in a row, handler should check for changes for optimal performance
		virtual void vFinish() {
		}

		static index legacy_parseIndexProp(const isview& request, const isview& propName, index endBound) {
			return legacy_parseIndexProp(request, propName, 0, endBound);
		}

		static index legacy_parseIndexProp(
			const isview& request,
			const isview& propName,
			index minBound, index endBound
		) {
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
