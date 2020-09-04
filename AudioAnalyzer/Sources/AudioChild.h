/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "AudioParent.h"
#include "audio-utils/CustomizableValueTransformer.h"

namespace rxtd::audio_analyzer {
	class AudioChild : public utils::TypeHolder {
		using CVT = audio_utils::CustomizableValueTransformer;

		struct Options {
			Channel channel = Channel::eFRONT_LEFT;
			index valueIndex = 0;
			istring handlerName;
			istring procName;
			std::vector<istring> infoRequest;
			std::vector<isview> infoRequestC;
			CVT transformer;

			struct Legacy {
				enum class NumberTransform {
					eNONE,
					eLINEAR,
					eDB,
				};

				bool clamp01 = false;
				NumberTransform numberTransform = NumberTransform::eNONE;
				double correctingConstant = 0.0;

				friend bool operator==(const Legacy& lhs, const Legacy& rhs) {
					return lhs.clamp01 == rhs.clamp01
						&& lhs.numberTransform == rhs.numberTransform
						&& lhs.correctingConstant == rhs.correctingConstant;
				}

				friend bool operator!=(const Legacy& lhs, const Legacy& rhs) {
					return !(lhs == rhs);
				}
			} legacy;

			friend bool operator==(const Options& lhs, const Options& rhs) {
				return lhs.channel == rhs.channel
					&& lhs.valueIndex == rhs.valueIndex
					&& lhs.handlerName == rhs.handlerName
					&& lhs.procName == rhs.procName
					&& lhs.infoRequest == rhs.infoRequest
					&& lhs.infoRequestC == rhs.infoRequestC
					&& lhs.transformer == rhs.transformer
					&& lhs.legacy == rhs.legacy;
			}

			friend bool operator!=(const Options& lhs, const Options& rhs) {
				return !(lhs == rhs);
			}
		} options;

		AudioParent* parent = nullptr;

		index legacyNumber;

	public:
		explicit AudioChild(utils::Rainmeter&& _rain);

	protected:
		void vReload() override;
		double vUpdate() override;
		void vUpdateString(string& resultStringBuffer) override;

	private:
		Options readOptions() const;
		Options::Legacy legacy_readOptions() const;

		[[nodiscard]]
		double legacy_update() const;
	};
}
