/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ButterworthWrapper.h"
#include "iir.h"

using BW = rxtd::audio_analyzer::audio_utils::butterworth_lib::ButterworthWrapper;
using GCC = BW::GenericCoefCalculator;

const GCC BW::lowPass = { dcof_bwlp, ccof_bwlp, sf_bwlp, oneSideSlopeSize };
const GCC BW::highPass = { dcof_bwhp, ccof_bwhp, sf_bwhp, oneSideSlopeSize };
const GCC BW::bandPass = { dcof_bwbp, ccof_bwbp, sf_bwbp, twoSideSlopeSize };
const GCC BW::bandStop = { dcof_bwbs, ccof_bwbs, sf_bwbs, twoSideSlopeSize };
