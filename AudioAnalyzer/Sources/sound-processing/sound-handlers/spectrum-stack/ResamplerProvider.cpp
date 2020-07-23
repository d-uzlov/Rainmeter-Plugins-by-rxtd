/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ResamplerProvider.h"
#include "BandResampler.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

const BandResampler* ResamplerProvider::getResampler(const DataSupplier& dataSupplier) const {
	const auto source = dataSupplier.getHandler(resamplerId);
	if (source == nullptr) {
		setValid(false);
		return nullptr;
	}

	const auto provider = dynamic_cast<const ResamplerProvider*>(source);
	if (provider == nullptr) {
		return nullptr;
	}

	return provider->getResampler(dataSupplier);
}
