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
#include <chrono>
#include <mutex>

#include "RainmeterAPI.h"
#include "rxtd/DataWithLock.h"
#include "rxtd/buffer_printer/BufferPrinter.h"

//
// How this works:
//
// Global std::atomic<int32_t> counter keeps count of all instances of InstanceKeeper
// When global counter reaches zero, it means that all of the instances of InstanceKeeper are destroyed
// so nobody needs the log thread anymore and we can kill it.
// However, just in case a skin was refreshed, thread destruction is delayed by 500 ms,
// after which counter is checked.
// If anyone created InstanceKeeper instance, then counter would be not 0,
// indicating that thread should not be stopped.
// When thread exits, it calls FreeLibraryAndExitThread, thus library can now be unloaded from memory.
//
// See InstanceKeeper.h for reasoning for this solution.
//

using rxtd::rainmeter::InstanceKeeper;
using BufferPrinter = rxtd::buffer_printer::BufferPrinter;

using Message = InstanceKeeper::Message;

struct MessageQueue : rxtd::DataWithLock {
	std::vector<Message> buffer;
	std::condition_variable sleepVariable;

	MessageQueue() : DataWithLock(true) { }
};

static constexpr rxtd::index cCRITICAL_MESSAGES_COUNT = 100;

struct ThreadArguments {
	HMODULE dllHandle = nullptr;
};

std::atomic<int32_t> counter = 0;
struct ThreadGuard : rxtd::DataWithLock {
	bool threadIsRunning = false;
};

// To avoid deadlocks,
// always obtain in the order: threadGuard → queue
// when both objects need to be locked at the same time
ThreadGuard threadGuard; // NOLINT(clang-diagnostic-exit-time-destructors)
MessageQueue queue; // NOLINT(clang-diagnostic-exit-time-destructors)

DWORD WINAPI asyncRun(void* param) {
	using namespace std::chrono_literals;

	auto args = static_cast<ThreadArguments*>(param);
	HMODULE handle = args->dllHandle;
	delete args;

	// RmLog(nullptr, 0, L"asyncRun started");

	try {
		while (true) {
			std::vector<Message> localQueueBuffer;
			bool stop = false;
			while (!stop) {
				if (localQueueBuffer.empty()) {
					auto lock = queue.getLock();
					queue.sleepVariable.wait(
						lock, [&]() {
							return !queue.buffer.empty();
						}
					);
					std::swap(localQueueBuffer, queue.buffer);
				}

				// Let's prevent potential message loss when we got a message with stop signal
				// but there were some other messages in the queue

				for (auto& queuedMessage : localQueueBuffer) {
					auto mes = std::exchange(queuedMessage, {});
					if (mes.action != nullptr) {
						stop = mes.action(mes);
						if (stop) {
							break;
						}
					}
				}

				if (!stop) {
					localQueueBuffer.clear();
				}
			}

			{
				{
					using namespace std::chrono_literals;
					auto lock = queue.getLock();
					queue.sleepVariable.wait_for(lock, 50ms);
				}

				const bool needToStop = threadGuard.runGuarded(
					[]() {
						// In the incrementCounter() counter is incremented
						// and then threadGuard.threadIsRunning is checked
						// to see if thread should be created
						//
						// If in the few ms while we were waiting someone
						// created new InstanceKeeper instance,
						// then by the time we acquire threadGuard lock,
						// counter will be not 0
						//
						// If someone were to send a message and delete all the instances,
						//		though this shouldn't be possible since sendMessage() wakes this thread
						// then queue.buffer will be not empty,
						// and we should do another loop to grab all data from it.
						// The buffer will contain another message with a stop signal at the end,
						// so we will naturally exit the inner loop and go to this code again.
						//
						if (counter.load() == 0 && queue.runGuarded([]() {return queue.buffer.empty(); })) {
							threadGuard.threadIsRunning = false;
							return true;
						} else {
							return false;
						}
					}
				);
				if (needToStop) {
					break;
				}
			}
		}
	} catch (...) {}

	// RmLog(nullptr, 0, L"asyncRun finished");

	FreeLibraryAndExitThread(handle, 0);
}

void InstanceKeeper::sendLog(DataHandle handle, string message, int level) const {
	Message mes;
	mes.action = [](const Message& mes) {
		RmLog(mes.dataHandle.getRawHandle(), mes.logLevel, mes.messageText.c_str());
		return false;
	};
	mes.dataHandle = handle;
	mes.messageText = std::move(message);
	mes.logLevel = level;
	sendMessage(std::move(mes));
}

void InstanceKeeper::sendCommand(DataHandle handle, SkinHandle skin, string command) {
	Message mes;
	mes.action = [](const Message& mes) {
		RmExecute(mes.skinHandle.getRawHandle(), mes.messageText.c_str());
		return false;
	};
	mes.dataHandle = handle;
	mes.skinHandle = skin;
	mes.messageText = std::move(command);
	sendMessage(std::move(mes));
}

void InstanceKeeper::incrementCounter(void* data) {
	auto prev = counter.fetch_add(1);
	if (prev != 0) return;

	try {
		threadGuard.runGuarded(
			[&]() {
				if (threadGuard.threadIsRunning) {
					// Helps to keep the background thread alive in case it was exiting on timeout
					sendMessage({});
				} else {
					initThread(data);
					threadGuard.threadIsRunning = true;
				}
			}
		);
	} catch (...) {}
}

void InstanceKeeper::decrementCounter() {
	auto prev = counter.fetch_add(-1);
	if (prev != 1) return;

	try {
		threadGuard.runGuarded(
			[&]() {
				if (!threadGuard.threadIsRunning) return;

				Message mes{};
				mes.action = [](const Message& mes) {
					return true;
				};
				sendMessage(mes);
			}
		);
	} catch (...) {}
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
		buffer_printer::BufferPrinter bp;
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
		BufferPrinter bp;
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
	} else {
		threadGuard.threadIsRunning = true;
	}
}

void InstanceKeeper::sendMessage(Message message) const {
	try {
		queue.runGuarded(
			[&]() {
				if (queue.buffer.size() > cCRITICAL_MESSAGES_COUNT) {
					// too many massages, something went wrong
					return;
				}
				queue.buffer.push_back(std::move(message));
				queue.sleepVariable.notify_one();
			}
		);
	} catch (...) {}
}
