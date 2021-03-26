// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "NamesManager.h"

#include <unordered_map>

#include "rxtd/std_fixes/StringUtils.h"

using namespace std::string_view_literals;

using rxtd::perfmon::pdh::NamesManager;
using rxtd::std_fixes::StringUtils;

void NamesManager::createModifiedNames(const PdhSnapshot& snapshot, const PdhSnapshot& processIdSnapshot, array_span<UniqueInstanceId> ids) {
	names.resize(static_cast<std::vector<ModifiedNameItem>::size_type>(snapshot.getItemsCount()));

	fillOriginalNames(snapshot);

	namesSize = snapshot.getNamesSize();

	switch (modificationType) {
	case ModificationType::NONE:
		break;
	case ModificationType::PROCESS:
		modifyNameProcess(snapshot, ids);
		break;
	case ModificationType::THREAD:
		modifyNameThread(snapshot, ids);
		break;
	case ModificationType::LOGICAL_DISK_DRIVE_LETTER:
		modifyNameLogicalDiskDriveLetter();
		break;
	case ModificationType::LOGICAL_DISK_MOUNT_PATH:
		modifyNameLogicalDiskMountPath();
		break;
	case ModificationType::GPU_PROCESS:
		modifyNameGPUProcessName(processIdSnapshot);
		break;
	case ModificationType::GPU_ENGTYPE:
		modifyNameGPUEngtype();
		break;
	}

	switch (modificationType) {
	case ModificationType::PROCESS: [[fallthrough]];
	case ModificationType::THREAD: break;

	case ModificationType::NONE: [[fallthrough]];
	case ModificationType::LOGICAL_DISK_DRIVE_LETTER: [[fallthrough]];
	case ModificationType::LOGICAL_DISK_MOUNT_PATH: [[fallthrough]];
	case ModificationType::GPU_PROCESS: [[fallthrough]];
	case ModificationType::GPU_ENGTYPE:
		createIdsBasedOnName(ids);
		break;
	}

	generateSearchNames();
}

void NamesManager::fillOriginalNames(const PdhSnapshot& snapshot) {
	for (index instanceIndex = 0; instanceIndex < snapshot.getItemsCount(); ++instanceIndex) {
		const sview name = snapshot.getName(instanceIndex);
		names[static_cast<size_t>(instanceIndex)].originalName = name;
		names[static_cast<size_t>(instanceIndex)].displayName = name;
	}
}

void NamesManager::generateSearchNames() {
	buffer.resize(static_cast<std::vector<wchar_t>::size_type>(namesSize));
	wchar_t* namesBuffer = buffer.data();

	for (auto& item : names) {
		sview name = copyString(item.displayName, namesBuffer);
		namesBuffer += name.length();

		item.searchName = name;
	}

	CharUpperBuffW(buffer.data(), static_cast<DWORD>(namesBuffer - buffer.data()));
}

rxtd::sview NamesManager::copyString(sview source, wchar_t* dest) {
	std::copy(source.begin(), source.end(), dest);
	return { dest, source.length() };
}

void NamesManager::modifyNameProcess(const PdhSnapshot& snapshot, array_span<UniqueInstanceId> ids) {
	for (index instanceIndex = 0; instanceIndex < static_cast<index>(names.size()); ++instanceIndex) {
		ModifiedNameItem& item = names[static_cast<size_t>(instanceIndex)];

		const auto pid = snapshot.getItem(snapshot.getCountersCount() - 1, instanceIndex).FirstValue;
		if (pid <= 0) {
			ids[instanceIndex].id1 = 0;
			ids[instanceIndex].id2 = -getIdFromName(item.originalName);
		} else {
			ids[instanceIndex].id1 = static_cast<int32_t>(pid);
			ids[instanceIndex].id2 = 0;
		}
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

	for (index instanceIndex = 0; instanceIndex < static_cast<index>(names.size()); ++instanceIndex) {
		ModifiedNameItem& item = names[static_cast<size_t>(instanceIndex)];

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
		} else if (item.originalName.startsWith(L"HarddiskVolume")) {
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
		} else if (item.originalName.startsWith(L"HarddiskVolume"sv)) {
			item.displayName = L"HarddiskVolume"sv;
		}
	}
}

void NamesManager::modifyNameGPUProcessName(const PdhSnapshot& idSnapshot) {
	// display name is process name (found by PID)

	namesSize = 0;

	std::unordered_map<long long, sview> pidToName;
	pidToName.reserve(static_cast<size_t>(idSnapshot.getItemsCount()));
	for (index instanceIndex = 0; instanceIndex < static_cast<index>(idSnapshot.getItemsCount()); ++instanceIndex) {
		const auto pid = idSnapshot.getItem(0, instanceIndex).FirstValue;
		pidToName[pid] = idSnapshot.getName(instanceIndex);
	}

	for (auto& item : names) {
		const auto pidPosition = item.originalName.find(L"pid_");
		if (pidPosition == sview::npos) {
			continue;
		}

		const auto pid = StringUtils::parseInt(item.originalName.substr(pidPosition + 4));
		const auto iter = pidToName.find(pid);
		if (iter == pidToName.end()) {
			continue;
		}

		item.displayName = iter->second;
		namesSize += item.displayName.size();
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
	for (index i = 0; i < static_cast<index>(names.size()); ++i) {
		ids[i].id1 = getIdFromName(names[static_cast<size_t>(i)].originalName);
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
