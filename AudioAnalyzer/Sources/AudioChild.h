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

namespace rxaa {
	class AudioChild : public rxu::TypeHolder {
		enum class StringValue {
			NUMBER,
			INFO
		};

		enum class NumberTransform {
			LINEAR,
			DB,
			NONE,
		};

		// Options
		Channel channel = Channel::FRONT_LEFT;
		NumberTransform numberTransform = NumberTransform::LINEAR;
		StringValue stringValueType = StringValue::NUMBER;
		unsigned index = 0;
		std::wstring valueId;
		double correctingConstant = 0.0;
		bool clamp = true;
		std::vector<std::wstring> infoRequest;
		std::vector<const wchar_t*> infoRequestC;

		// data
		AudioParent* parent = nullptr;
		std::wstring stringValue;

	public:
		explicit AudioChild(rxu::Rainmeter&& _rain);
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
