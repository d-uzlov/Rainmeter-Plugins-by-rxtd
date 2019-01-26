/* 
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <vector>
#include "PdhWrapper.h"

#undef min
#undef max
#undef IN
#undef OUT

namespace rxpm::pdh {
	struct ModifiedNameItem {
		std::wstring_view originalName;
		std::wstring_view uniqueName;
		std::wstring_view displayName;
		std::wstring_view searchName;
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
		size_t buffersCount { };
		std::vector<std::vector<wchar_t>> buffers;
		size_t originalNamesSize { };

		ModificationType modificationType { };

	public:
		const ModifiedNameItem& get(size_t index) const;

		void setModificationType(ModificationType value);

		void createModifiedNames(const Snapshot& snapshot, const Snapshot& idSnapshot);

	private:
		void copyOriginalNames(const Snapshot& snapshot);

		void generateSearchNames();

		void resetBuffers();
		wchar_t* getBuffer(size_t value);
		
		static std::wstring_view copyString(std::wstring_view source, wchar_t* dest);

		void modifyNameProcess(const Snapshot& idSnapshot);

		void modifyNameThread(const Snapshot& idSnapshot);

		void modifyNameLogicalDiskDriveLetter();

		void modifyNameLogicalDiskMountPath();

		void modifyNameGPUProcessName(const Snapshot& idSnapshot);

		void modifyNameGPUEngtype();
	};
}
