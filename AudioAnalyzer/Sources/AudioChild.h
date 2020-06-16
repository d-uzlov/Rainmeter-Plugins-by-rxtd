/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "AudioParent.h"

namespace rxtd::audio_analyzer {
	class AudioChild : public utils::TypeHolder {
		enum class StringValue {
			eNUMBER,
			eINFO
		};

		enum class NumberTransform {
			eLINEAR,
			eDB,
			eNONE,
		};

		// Options
		Channel channel = Channel::eFRONT_LEFT;
		NumberTransform numberTransform = NumberTransform::eLINEAR;
		StringValue stringValueType = StringValue::eNUMBER;
		index valueIndex = 0;
		string valueId;
		double correctingConstant = 0.0;
		bool clamp = true;
		std::vector<string> infoRequest;
		std::vector<const wchar_t*> infoRequestC;

		// data
		AudioParent* parent = nullptr;
		string stringValue;

	public:
		explicit AudioChild(utils::Rainmeter&& _rain);
		~AudioChild();
		/** This class is non copyable */
		AudioChild(const AudioChild& other) = delete;
		AudioChild(AudioChild&& other) = delete;
		AudioChild& operator=(const AudioChild& other) = delete;
		AudioChild& operator=(AudioChild&& other) = delete;


	protected:
		void _reload() override;
		std::tuple<double, const wchar_t*> _update() override;
	};
}
