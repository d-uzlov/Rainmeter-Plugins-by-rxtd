// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "AudioParent.h"
#include "rxtd/audio_analyzer/audio_utils/CustomizableValueTransformer.h"

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
		mutable option_parsing::OptionParser parser = option_parsing::OptionParser::getDefault();

		Version version;

	public:
		explicit AudioChild(Rainmeter&& _rain);

	protected:
		void vReload() override;
		double vUpdate() override;
		void vUpdateString(string& resultStringBuffer) override;

	private:
		Options readOptions();
	};
}
