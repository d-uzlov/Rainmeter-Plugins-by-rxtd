/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "InstanceKeeper.h"

#include <atomic>
#include <mutex>

#include "BufferPrinter.h"
#include "DataWithLock.h"
#include "RainmeterAPI.h"

// 
// How this works:
// 
// Global std::atomic<int32_t> counter keeps count of all instances of InstanceKeeper
// When there were 0 instances and an instance was created, InstanceKeeper#initThread() is called
//		It creates a WinAPI thread that sleeps on global MessageQueue queue
//		This thread holds a handle to dll, which doesn't let the dll be unloaded from memory
// When someone pushes a message into queue, that thread reads and executes the message
// When global counter reaches zero, it means that all of the instances of InstanceKeeper are destroyed
// so we can kill the thread.
// When thread receives kill message, it calls FreeLibraryAndExitThread, thus library can now be unloaded from memory.
//
// See InstanceKeeper.h for reasoning for this solution.
// 

using namespace ::rxtd::common::rainmeter;

using Message = InstanceKeeper::Message;

struct MessageQueue : DataWithLock {
	std::vector<Message> buffer;
	std::condition_variable sleepVariable;

	MessageQueue() : DataWithLock(true) { }
};

struct ThreadArguments {
	HMODULE dllHandle = nullptr;
};

std::atomic<int32_t> counter = 0;
MessageQueue queue; // NOLINT(clang-diagnostic-exit-time-destructors)

DWORD WINAPI asyncRun(void* param) {
	using namespace std::chrono_literals;

	auto args = static_cast<ThreadArguments*>(param);
	HMODULE handle = args->dllHandle;
	delete args;

	// RmLog(nullptr, 0, L"asyncRun started");

	try {
		std::vector<Message> localQueueBuffer;
		while (true) {
			{
				auto lock = queue.getLock();
				while (queue.buffer.empty()) {
					queue.sleepVariable.wait(lock);
				}
				std::swap(localQueueBuffer, queue.buffer);
			}

			for (auto& mes : localQueueBuffer) {
				if (mes.action != nullptr) {
					mes.action(mes);
				}
			}
			localQueueBuffer.clear();
		}
	} catch (...) {}

	// RmLog(nullptr, 0, L"asyncRun finished");

	FreeLibraryAndExitThread(handle, 0);
}

void InstanceKeeper::sendLog(DataHandle handle, string message, int level) {
	Message mes;
	mes.action = [](const Message& mes) {
		RmLog(mes.rainmeterData, mes.logLevel, mes.messageText.c_str());
	};
	mes.rainmeterData = handle.getRawHandle();
	mes.messageText = std::move(message);
	mes.logLevel = level;
	sendMessage(std::move(mes));
}

void InstanceKeeper::sendCommand(SkinHandle skin, string command) {
	Message mes;
	mes.action = [](const Message& mes) {
		RmExecute(mes.rainmeterData, mes.messageText.c_str());
	};
	mes.rainmeterData = skin.getRawHandle();
	mes.messageText = std::move(command);
	sendMessage(std::move(mes));
}

void InstanceKeeper::incrementCounter(void* data) {
	if (counter == 0) {
		initThread(data);
	}
	counter.fetch_add(1);
}

void InstanceKeeper::decrementCounter() {
	counter.fetch_add(-1);
	if (counter == 0) {
		sendKill();
	}
}

void InstanceKeeper::initThread(void* rm) {
	HMODULE dllHandle = nullptr;
	// this call should increment library handler counter
	const auto handlerReadSuccess = GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		reinterpret_cast<LPCTSTR>(asyncRun),
		&dllHandle
	);
	if (!handlerReadSuccess) {
		const auto errorCode = GetLastError();
		common::buffer_printer::BufferPrinter bp;
		bp.print(L"Log and bangs are not available, GetModuleHandleExW failed (error {})", errorCode);
		RmLog(rm, LOG_ERROR, bp.getBufferPtr());

		wchar_t* receiveBuffer = nullptr;
		const auto messageLength = FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			nullptr,
			errorCode,
			0,
			reinterpret_cast<LPWSTR>(&receiveBuffer),
			0,
			nullptr
		);
		if (messageLength != 0) {
			bp.print(L"More info: {}", receiveBuffer);
		} else {
			bp.print(
				L"Can't get readable explanation of error because FormatMessageW failed (error {})",
				GetLastError()
			);
		}
		RmLog(rm, LOG_ERROR, bp.getBufferPtr());

		LocalFree(receiveBuffer);

		return;
	}

	auto threadArgs = new ThreadArguments;
	threadArgs->dllHandle = dllHandle;
	const auto threadHandle = CreateThread(
		nullptr,
		0,
		&asyncRun,
		threadArgs,
		0,
		nullptr
	);

	if (threadHandle == nullptr) {
		const auto errorCode = GetLastError();
		common::buffer_printer::BufferPrinter bp;
		bp.print(L"Log and bangs are not available, CreateThread failed (error {})", errorCode);
		RmLog(rm, LOG_ERROR, bp.getBufferPtr());

		wchar_t* receiveBuffer = nullptr;
		const auto messageLength = FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			nullptr,
			errorCode,
			0,
			reinterpret_cast<LPWSTR>(&receiveBuffer),
			0,
			nullptr
		);
		if (messageLength != 0) {
			bp.print(L"More info: {}", receiveBuffer);
		} else {
			bp.print(
				L"Can't get readable explanation of error because FormatMessageW failed (error {})",
				GetLastError()
			);
		}
		RmLog(rm, LOG_ERROR, bp.getBufferPtr());

		LocalFree(receiveBuffer);

		FreeLibrary(dllHandle);
		delete threadArgs;
	}
}

void InstanceKeeper::sendMessage(Message message) {
	try {
		auto lock = queue.getLock();
		queue.buffer.push_back(std::move(message));
		queue.sleepVariable.notify_one();
	} catch (...) {}
}

void InstanceKeeper::sendKill() {
	Message mes{};
	mes.action = [](const Message& mes) {
		throw std::runtime_error{ "stop thread" };
	};
	sendMessage(mes);
}
