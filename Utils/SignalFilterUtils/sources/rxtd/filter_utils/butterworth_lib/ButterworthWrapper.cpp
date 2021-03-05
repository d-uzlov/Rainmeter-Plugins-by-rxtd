// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "ButterworthWrapper.h"
#include "iir.h"

using BW = rxtd::filter_utils::butterworth_lib::ButterworthWrapper;
using GCC = BW::GenericCoefCalculator;

const GCC BW::lowPass = { dcof_bwlp, ccof_bwlp, sf_bwlp, oneSideSlopeSize };
const GCC BW::highPass = { dcof_bwhp, ccof_bwhp, sf_bwhp, oneSideSlopeSize };
const GCC BW::bandPass = { dcof_bwbp, ccof_bwbp, sf_bwbp, twoSideSlopeSize };
const GCC BW::bandStop = { dcof_bwbs, ccof_bwbs, sf_bwbs, twoSideSlopeSize };
