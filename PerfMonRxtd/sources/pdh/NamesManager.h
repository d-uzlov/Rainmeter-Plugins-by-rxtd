﻿/* 
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "PdhWrapper.h"

namespace rxtd::perfmon::pdh {
	struct ModifiedNameItem {
		sview originalName;
		sview uniqueName;
		sview displayName;
		sview searchName;
	};

	class NamesManager {
	public:
		enum class ModificationType {
			NONE,
			PROCESS,
			THREAD,
			LOGICAL_DISK_DRIVE_LETTER,
			LOGICAL_DISK_MOUNT_PATH,
			GPU_PROCESS,
			GPU_ENGTYPE,
		};

	private:
		std::vector<ModifiedNameItem> names;
		index buffersCount { };
		std::vector<std::vector<wchar_t>> buffers;
		index originalNamesSize { };

		ModificationType modificationType { };

	public:
		const ModifiedNameItem& get(index index) const;

		void setModificationType(ModificationType value);

		void createModifiedNames(const Snapshot& snapshot, const Snapshot& idSnapshot);

	private:
		void copyOriginalNames(const Snapshot& snapshot);

		void generateSearchNames();

		void resetBuffers();
		wchar_t* getBuffer(index value);
		
		static sview copyString(sview source, wchar_t* dest);

		void modifyNameProcess(const Snapshot& idSnapshot);

		void modifyNameThread(const Snapshot& idSnapshot);

		void modifyNameLogicalDiskDriveLetter();

		void modifyNameLogicalDiskMountPath();

		void modifyNameGPUProcessName(const Snapshot& idSnapshot);

		void modifyNameGPUEngtype();
	};
}
