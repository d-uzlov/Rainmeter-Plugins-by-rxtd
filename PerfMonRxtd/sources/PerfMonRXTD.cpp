/* This plugin was originally written by buckb, then was forked by rxtd from version 2.0.1.0.
 * To prevent compatibility issues names was changed from PerfMonPDH to PerfMonRXTD.
 * Version was reset to 1.0.0 due to name change.
 *
 * Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */


#include "RainmeterAPI.h"
#include "PerfMonRXTD.h"
#include "Parent.h"
#include "Child.h"
#include "utils.h"

#pragma comment(lib, "pdh.lib")

pmr::TypeHolder::~TypeHolder() {
	delete parentData;
	delete childData;
}
PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	pmr::TypeHolder * typeHolder = new pmr::TypeHolder(rm);
	*data = typeHolder;

	LPCWSTR str = RmReadString(rm, L"Type", L"");
	if (_wcsicmp(str, L"Parent") == 0) {
		typeHolder->isParent = true;
		typeHolder->parentData = new pmr::ParentData(typeHolder);
	} else {
		typeHolder->isParent = false;
		typeHolder->childData = new pmr::ChildData(typeHolder);
	}
}
PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue) {
	if (data == nullptr) {
		return;
	}

	pmr::TypeHolder& typeHolder = *static_cast<pmr::TypeHolder*>(data);
	if (typeHolder.broken) { // we skip reload only if the measure is unrecoverable
		return;
	}

	if (typeHolder.isParent) {
		if (typeHolder.parentData != nullptr) {
			typeHolder.parentData->reload();
		}
	} else {
		if (typeHolder.childData != nullptr) {
			typeHolder.childData->reload();
		}
	}
}

PLUGIN_EXPORT double Update(void* data) {
	if (data == nullptr) {
		return 0.0;
	}

	pmr::TypeHolder& typeHolder = *static_cast<pmr::TypeHolder*>(data);
	if (typeHolder.isBroken()) {
		return 0.0;
	}

	typeHolder.resultDouble = 0;

	if (typeHolder.isParent) {
		if (typeHolder.parentData != nullptr) {
			typeHolder.parentData->update();
		}
	} else {
		if (typeHolder.childData != nullptr) {
			typeHolder.childData->update();
		}
	}

	return typeHolder.resultDouble;
}
PLUGIN_EXPORT LPCWSTR GetString(void* data) {
	if (data == nullptr) {
		return nullptr;
	}
	pmr::TypeHolder& typeHolder = *static_cast<pmr::TypeHolder*>(data);
	if (typeHolder.isBroken()) {
		return L"broken";
	}
	return typeHolder.resultString;
}
PLUGIN_EXPORT void Finalize(void* data) {
	if (data == nullptr) {
		return;
	}
	delete static_cast<pmr::TypeHolder*>(data);
}
PLUGIN_EXPORT void ExecuteBang(void* data, LPCWSTR args) {
	if (data == nullptr) {
		return;
	}
	pmr::TypeHolder& typeHolder = *static_cast<pmr::TypeHolder*>(data);
	if (typeHolder.isBroken()) {
		RmLogF(typeHolder.rm, LOG_WARNING, L"Skipping bang on the broken measure '%s'", typeHolder.measureName.c_str());
		return;
	}
	if (!typeHolder.isParent) {
		RmLogF(typeHolder.rm, LOG_WARNING, L"Skipping bang on child measure '%s'", typeHolder.measureName.c_str());
		return;
	}
	if (typeHolder.parentData == nullptr) {
		return;
	}
	if (_wcsicmp(args, L"Stop") == 0) {
		typeHolder.parentData->setStopped(true);
		return;
	}
	if (_wcsicmp(args, L"Resume") == 0) {
		typeHolder.parentData->setStopped(false);
		return;
	}
	if (_wcsicmp(args, L"StopResume") == 0) {
		typeHolder.parentData->changeStopState();
		return;
	}
	std::wstring str = args;
	if (_wcsicmp(str.substr(0, 14).c_str(), L"SetIndexOffset") == 0) {
		std::wstring arg = str.substr(14);
		arg = trimSpaces(arg);
		if (arg[0] == L'-' || arg[0] == L'+') {
			const int offset = typeHolder.parentData->getIndexOffset() + _wtoi(arg.c_str());
			typeHolder.parentData->setIndexOffset(offset);
		} else {
			const int offset = _wtoi(arg.c_str());
			typeHolder.parentData->setIndexOffset(offset);
		}
	}
}