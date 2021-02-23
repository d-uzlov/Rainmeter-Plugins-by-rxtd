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
	class AudioChild : public utils::MeasureBase {
		using CVT = audio_utils::CustomizableValueTransformer;

		struct Options {
			Channel channel = Channel::eFRONT_LEFT;
			index valueIndex = 0;
			istring handlerName;
			istring procName;
			std::vector<istring> infoRequest;
			std::vector<isview> infoRequestC;
			CVT transformer;

			friend bool operator==(const Options& lhs, const Options& rhs) {
				return lhs.channel == rhs.channel
					&& lhs.valueIndex == rhs.valueIndex
					&& lhs.handlerName == rhs.handlerName
					&& lhs.procName == rhs.procName
					&& lhs.infoRequest == rhs.infoRequest
					&& lhs.infoRequestC == rhs.infoRequestC
					&& lhs.transformer == rhs.transformer;
			}

			friend bool operator!=(const Options& lhs, const Options& rhs) {
				return !(lhs == rhs);
			}
		} options;

		AudioParent* parent = nullptr;

		Version version;

	public:
		explicit AudioChild(Rainmeter&& _rain);

	protected:
		void vReload() override;
		double vUpdate() override;
		void vUpdateString(string& resultStringBuffer) override;

	private:
		Options readOptions() const;
	};
}
