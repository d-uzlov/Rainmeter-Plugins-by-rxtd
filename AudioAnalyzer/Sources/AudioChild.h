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

namespace rxtd::audio_analyzer {
	class AudioChild : public utils::TypeHolder {
		Channel channel = Channel::eFRONT_LEFT;
		index valueIndex = 0;
		istring valueId;
		istring procId;
		std::vector<istring> infoRequest;
		std::vector<isview> infoRequestC;

		AudioParent* parent = nullptr;

		struct Legacy {
			enum class NumberTransform {
				eNONE,
				eLINEAR,
				eDB,
			};

			bool use = false;
			bool clamp01 = false;
			NumberTransform numberTransform = NumberTransform::eNONE;
			double correctingConstant = 0.0;

		} legacy;

	public:
		explicit AudioChild(utils::Rainmeter&& _rain);
		~AudioChild() = default;
		/** This class is non copyable */
		AudioChild(const AudioChild& other) = delete;
		AudioChild(AudioChild&& other) = delete;
		AudioChild& operator=(const AudioChild& other) = delete;
		AudioChild& operator=(AudioChild&& other) = delete;

	protected:
		void _reload() override;
		double _update() override;
		void _updateString(string& resultStringBuffer) override;

	private:
		void legacy_readOptions();
		double legacy_update();
	};
}
