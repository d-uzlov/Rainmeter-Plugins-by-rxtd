// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Danil Uzlov

#pragma once
#include "PerfmonParent.h"

namespace rxtd::perfmon {
	class PerfmonChild : public utils::MeasureBase {
		Reference ref;
		string instanceName;
		index instanceIndex = 0;
		ResultString resultStringType = ResultString::eNUMBER;

		const PerfmonParent* parent = nullptr;
		string stringValue;
		option_parsing::OptionParser parser = option_parsing::OptionParser::getDefault();

	public:
		explicit PerfmonChild(Rainmeter&& _rain);

	protected:
		void vReload() override;
		double vUpdate() override;
		void vUpdateString(string& resultStringBuffer) override;
	};
}
