/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "NamesManager.h"

#include <unordered_map>

#include "BufferPrinter.h"
#include "StringUtils.h"

using namespace perfmon::pdh;

using namespace std::string_view_literals;

void NamesManager::createModifiedNames(const PdhSnapshot& snapshot, array_span<UniqueInstanceId> ids) {
	resetBuffers();

	originalNamesSize = snapshot.getNamesSize();
	names.resize(snapshot.getItemsCount());

	copyOriginalNames(snapshot);

	switch (modificationType) {
	case ModificationType::NONE:
		createIdsBasedOnName(ids);
		break;
	case ModificationType::PROCESS:
		modifyNameProcess(snapshot, ids);
		break;
	case ModificationType::THREAD:
		modifyNameThread(snapshot, ids);
		break;
	case ModificationType::LOGICAL_DISK_DRIVE_LETTER:
		modifyNameLogicalDiskDriveLetter();
		createIdsBasedOnName(ids);
		break;
	case ModificationType::LOGICAL_DISK_MOUNT_PATH:
		modifyNameLogicalDiskMountPath();
		createIdsBasedOnName(ids);
		break;
	case ModificationType::GPU_PROCESS:
		modifyNameGPUProcessName(snapshot);
		createIdsBasedOnName(ids);
		break;
	case ModificationType::GPU_ENGTYPE:
		modifyNameGPUEngtype();
		createIdsBasedOnName(ids);
		break;
	}

	generateSearchNames();
}

void NamesManager::copyOriginalNames(const PdhSnapshot& snapshot) {
	wchar_t* namesBuffer = getBuffer(originalNamesSize);

	for (index instanceIndex = 0; instanceIndex < snapshot.getItemsCount(); ++instanceIndex) {
		const wchar_t* namePtr = snapshot.getName(instanceIndex);
		ModifiedNameItem& item = names[instanceIndex];

		sview name = copyString(namePtr, namesBuffer);
		namesBuffer += name.length();

		item.originalName = name;
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

void NamesManager::modifyNameProcess(const PdhSnapshot& snapshot, array_span<UniqueInstanceId> ids) {
	const index namesCount(names.size());
	for (index instanceIndex = 0; instanceIndex < namesCount; ++instanceIndex) {
		ModifiedNameItem& item = names[instanceIndex];

		const auto pid = snapshot.getItem(snapshot.getCountersCount() - 1, instanceIndex).FirstValue;
		ids[instanceIndex].id1 = static_cast<int32_t>(pid);
		ids[instanceIndex].id2 = 0;
	}
}

void NamesManager::modifyNameThread(const PdhSnapshot& snapshot, array_span<UniqueInstanceId> ids) {
	// instance names are "<processName>/<threadIndex>"
	// process names are not unique
	// thread indices enumerate threads inside one process, starting from 0

	// some threads have invalid thread ID: tid==0
	// "some" are usually _Total and Idle
	// for these threads I use -getIdFromName value which is unique throughout NamesManager instance
	// and since I have yet to see a negative thread ID these shouldn't overlap with valid IDs

	// display name: <processName>
	// _Total/_Total -> _Total
	// Idle/n -> Idle

	for (index instanceIndex = 0; instanceIndex < index(names.size()); ++instanceIndex) {
		ModifiedNameItem& item = names[instanceIndex];

		const auto tid = snapshot.getItem(snapshot.getCountersCount() - 1, instanceIndex).FirstValue;
		if (tid <= 0) {
			ids[instanceIndex].id1 = 0;
			ids[instanceIndex].id2 = -getIdFromName(item.originalName);
		} else {
			ids[instanceIndex].id1 = static_cast<int32_t>(tid);
			ids[instanceIndex].id2 = 0;
		}

		item.displayName = item.originalName.substr(0, item.originalName.find_last_of(L'/'));
	}
}

void NamesManager::modifyNameLogicalDiskDriveLetter() {
	// keep only letter in mount path: "C:\path\mount" -> "C:"
	// volumes that are not mounted: "HardDiskVolume#123" -> "HardDiskVolume"

	for (auto& item : names) {
		if (item.originalName.length() < 2) {
			continue;
		}

		if (item.originalName[1] == L':') {
			item.displayName = item.originalName.substr(0, 2);
		} else if (utils::StringUtils::checkStartsWith(item.originalName, L"HarddiskVolume"sv)) {
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
		} else if (utils::StringUtils::checkStartsWith(item.originalName, L"HarddiskVolume"sv)) {
			item.displayName = L"HarddiskVolume"sv;
		}
	}
}

void NamesManager::modifyNameGPUProcessName(const PdhSnapshot& idSnapshot) {
	// display name is process name (found by PID)

	wchar_t* processNamesBuffer = getBuffer(idSnapshot.getNamesSize());

	std::unordered_map<long long, sview> pidToName;
	pidToName.reserve(idSnapshot.getItemsCount());
	for (index instanceIndex = 0; instanceIndex < index(idSnapshot.getItemsCount()); ++instanceIndex) {
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

void NamesManager::createIdsBasedOnName(array_span<UniqueInstanceId> ids) {
	for (index i = 0; i < index(names.size()); ++i) {
		ids[i].id1 = getIdFromName(names[i].originalName);
		ids[i].id2 = 0;
	}
}

int32_t NamesManager::getIdFromName(sview name) {
	auto iter = name2id.find(name);
	if (iter != name2id.end()) {
		return static_cast<int32_t>(iter->second);
	}

	auto pair = name2id.insert({ string{ name }, 0 });
	lastId++;
	pair.first->second = lastId;
	return static_cast<int32_t>(pair.first->second);
}
