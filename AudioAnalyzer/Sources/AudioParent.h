/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "TypeHolder.h"
#include "Channel.h"
#include "SoundAnalyzer.h"
#include "DeviceManager.h"

namespace rxaa {
	using namespace std::string_literals;
	using namespace std::literals::string_view_literals;

	class AudioParent : public rxu::TypeHolder {
	private:
		static rxu::ParentManager<AudioParent> parentManager;

		SoundAnalyzer soundAnalyzer;
		DeviceManager deviceManager;

	public:
		explicit AudioParent(rxu::Rainmeter&& rain);
		~AudioParent();
		/** This class is non copyable */
		AudioParent(const AudioParent& other) = delete;
		AudioParent(AudioParent&& other) = delete;
		AudioParent& operator=(const AudioParent& other) = delete;
		AudioParent& operator=(AudioParent&& other) = delete;

		static AudioParent* findInstance(rxu::Rainmeter::Skin skin, const wchar_t * measureName);

	protected:
		void _reload() override;
		std::tuple<double, const wchar_t*> _update() override;
		void _command(const wchar_t* bangArgs) override;
		const wchar_t* _resolve(int argc, const wchar_t* argv[]) override;

	public:
		double getValue(const std::wstring &id, Channel channel, int index) const;
	private:
	};


}

