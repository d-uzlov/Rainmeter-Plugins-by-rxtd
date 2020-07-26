/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "NamesManager.h"
#include <unordered_map>
#include "BufferPrinter.h"

#include "undef.h"

using namespace perfmon::pdh;

using namespace std::string_view_literals;

const ModifiedNameItem& NamesManager::get(item_t index) const {
	return names[index];
}

void NamesManager::setModificationType(ModificationType value) {
	modificationType = value;
}

void NamesManager::createModifiedNames(const PdhSnapshot& snapshot, const PdhSnapshot& idSnapshot) {
	resetBuffers();

	originalNamesSize = snapshot.getNamesSize();
	names.resize(snapshot.getItemsCount());

	copyOriginalNames(snapshot);

	switch (modificationType) {
	case ModificationType::NONE:
		break;
	case ModificationType::PROCESS:
		modifyNameProcess(idSnapshot);
		break;
	case ModificationType::THREAD:
		modifyNameThread(idSnapshot);
		break;
	case ModificationType::LOGICAL_DISK_DRIVE_LETTER:
		modifyNameLogicalDiskDriveLetter();
		break;
	case ModificationType::LOGICAL_DISK_MOUNT_PATH:
		modifyNameLogicalDiskMountPath();
		break;
	case ModificationType::GPU_PROCESS:
		modifyNameGPUProcessName(idSnapshot);
		break;
	case ModificationType::GPU_ENGTYPE:
		modifyNameGPUEngtype();
		break;
	default:
		break;
	}

	generateSearchNames();
}

void NamesManager::copyOriginalNames(const PdhSnapshot& snapshot) {
	wchar_t* namesBuffer = getBuffer(originalNamesSize);

	for (item_t instanceIndex = 0; instanceIndex < snapshot.getItemsCount(); ++instanceIndex) {
		const wchar_t* namePtr = snapshot.getName(instanceIndex);
		ModifiedNameItem& item = names[instanceIndex];

		sview name = copyString(namePtr, namesBuffer);
		namesBuffer += name.length();

		item.originalName = name;
		item.uniqueName = name;
		item.displayName = name;
	}
}

void NamesManager::generateSearchNames() {
	wchar_t* namesBuffer = getBuffer(originalNamesSize);

	for (auto& item : names) {
		sview name = copyString(item.displayName, namesBuffer);
		namesBuffer[name.length()] = L'\0';
		CharUpperW(namesBuffer);

		// don't need +1 here because string_view doesn't need null-termination
		namesBuffer += name.length();

		item.searchName = name;
	}
}

void NamesManager::resetBuffers() {
	buffersCount = 0;
}

wchar_t* NamesManager::getBuffer(index size) {
	const auto nextBufferIndex = buffersCount;
	buffersCount++;

	if (nextBufferIndex >= index(buffers.size())) {
		buffers.emplace_back();
	}

	auto& buffer = buffers[nextBufferIndex];
	buffer.reserve(size);
	return buffer.data();
}

sview NamesManager::copyString(sview source, wchar_t* dest) {
	std::copy(source.begin(), source.end(), dest);
	return { dest, source.length() };
}

void NamesManager::modifyNameProcess(const PdhSnapshot& idSnapshot) {
	// process name is name of the process file, which is not unique
	// set unique name to <name>#<pid>
	// assume that string representation of pid is no more than 20 symbols

	wchar_t* namesBuffer = getBuffer(originalNamesSize + names.size() * 20);

	utils::BufferPrinter printer { };

	const item_t namesCount(names.size());
	for (item_t instanceIndex = 0; instanceIndex < namesCount; ++instanceIndex) {
		ModifiedNameItem& item = names[instanceIndex];

		const long long pid = idSnapshot.getItem(0, instanceIndex).FirstValue;

		auto nameCopy = copyString(item.originalName, namesBuffer);
		namesBuffer += nameCopy.length();
		namesBuffer[0] = L'#';
		namesBuffer++;

		const auto length = swprintf(namesBuffer, 20, L"%llu", pid);
		if (length < 0) {
			namesBuffer += 20;
		} else {
			namesBuffer += length;
		}

		item.uniqueName = { nameCopy.data(), nameCopy.length() + 1 + length };
	}
}

void NamesManager::modifyNameThread(const PdhSnapshot& idSnapshot) {
	// instance names are "<processName>/<threadIndex>"
	// process names are not unique
	// thread indices enumerate threads inside one process, starting from 0

	// unique name: <processName>#<tid>
	// exception is Idle/n: all threads have tid 0, so keep /n for this tid
	// _Total/_Total also has tid 0, no problems with this

	// display name: <processName>
	// _Total/_Total -> _Total
	// Idle/n -> Idle

	// assume that string representation of tid is no more than 20 symbols

	wchar_t* namesBuffer = getBuffer(originalNamesSize + names.size() * 20);

	const item_t namesCount(names.size());
	for (item_t instanceIndex = 0; instanceIndex < namesCount; ++instanceIndex) {
		ModifiedNameItem& item = names[instanceIndex];

		const long long tid = idSnapshot.getItem(0, instanceIndex).FirstValue;

		sview nameCopy = copyString(item.originalName, namesBuffer);
		if (tid != 0) {
			nameCopy = nameCopy.substr(0, nameCopy.find_last_of(L'/'));
		}
		namesBuffer += nameCopy.length();

		namesBuffer[0] = L'#';
		namesBuffer++;

		const auto length = swprintf(namesBuffer, 20, L"%llu", tid);
		if (length < 0) {
			namesBuffer += 20;
		} else {
			namesBuffer += length;
		}

		item.uniqueName = { nameCopy.data(), nameCopy.length() + 1 + length };

		item.displayName = item.originalName.substr(0, item.originalName.find_last_of(L'/'));
	}
}

void NamesManager::modifyNameLogicalDiskDriveLetter() {
	// keep folder in mount path: "C:\path\mount" -> "C:"
	// volumes that are not mounted: "HardDiskVolume#123" -> "HardDiskVolume"

	for (auto& item : names) {
		if (item.originalName.length() < 2) {
			continue;
		}

		if (item.originalName[1] == L':') {
			item.displayName = item.originalName.substr(0, 2);
		} else if (utils::StringUtils::startsWith(item.originalName, L"HarddiskVolume"sv)) {
			item.displayName = L"HarddiskVolume"sv;
		}
	}
}

void NamesManager::modifyNameLogicalDiskMountPath() {
	// keep folder in mount path: "C:\path\mount" -> "C:\path\"
	// volumes that are not mounted: "HardDiskVolume#123" -> "HardDiskVolume"

	for (auto& item : names) {
		if (item.originalName.length() < 2) {
			continue;
		}

		if (item.originalName[1] == L':') {
			const auto slashPosition = item.originalName.find_last_of(L'\\');
			if (slashPosition != sview::npos) {
				item.displayName = item.originalName.substr(0, slashPosition + 1);
			}
		} else if (utils::StringUtils::startsWith(item.originalName, L"HarddiskVolume"sv)) {
			item.displayName = L"HarddiskVolume"sv;
		}
	}
}

void NamesManager::modifyNameGPUProcessName(const PdhSnapshot& idSnapshot) {
	// display name is process name (found by PID)

	wchar_t* processNamesBuffer = getBuffer(idSnapshot.getNamesSize());

	std::unordered_map<long long, sview> pidToName(idSnapshot.getItemsCount());
	const item_t namesCount(idSnapshot.getItemsCount());
	for (item_t instanceIndex = 0; instanceIndex < namesCount; ++instanceIndex) {
		sview name = copyString(idSnapshot.getName(instanceIndex), processNamesBuffer);
		processNamesBuffer += name.length();

		const auto pid = idSnapshot.getItem(0, instanceIndex).FirstValue;
		pidToName[pid] = name;
	}

	for (auto& item : names) {
		const auto pidPosition = item.originalName.find(L"pid_");
		if (pidPosition == sview::npos) {
			continue;
		}

		const auto pid = utils::StringUtils::parseInt(item.originalName.substr(pidPosition + 4));
		const auto iter = pidToName.find(pid);
		if (iter == pidToName.end()) {
			continue;
		}

		item.displayName = iter->second;
	}
}

void NamesManager::modifyNameGPUEngtype() {
	// keeping engtype_x only

	for (auto& item : names) {
		const auto suffixStartPlace = item.originalName.find(L"engtype_");
		if (suffixStartPlace != sview::npos) {
			item.displayName = item.originalName.substr(suffixStartPlace);
		}
	}
}

