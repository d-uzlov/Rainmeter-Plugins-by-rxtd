#include "RainmeterAPI.h"
#include "LocalPluginLoader.h"

PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	*data = new LocalPluginLoader(rm);
	utils::Rainmeter::printLogMessages();
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue) {
	static_cast<LocalPluginLoader*>(data)->reload(maxValue, rm);
	utils::Rainmeter::printLogMessages();
}

PLUGIN_EXPORT double Update(void* data) {
	const auto result = static_cast<LocalPluginLoader*>(data)->update();
	utils::Rainmeter::printLogMessages();
	return result;
}

PLUGIN_EXPORT LPCWSTR GetString(void* data) {
	const auto result = static_cast<LocalPluginLoader*>(data)->getStringValue();
	utils::Rainmeter::printLogMessages();
	return result;
}

PLUGIN_EXPORT void Finalize(void* data) {
	delete static_cast<LocalPluginLoader*>(data);
	utils::Rainmeter::printLogMessages();
}

PLUGIN_EXPORT void ExecuteBang(void* data, LPCWSTR args) {
	static_cast<LocalPluginLoader*>(data)->executeBang(args);
	utils::Rainmeter::printLogMessages();;
}

PLUGIN_EXPORT LPCWSTR sv(void* data, const int argc, const WCHAR* argv[]) {
	const auto result = static_cast<LocalPluginLoader*>(data)->solveSectionVariable(argc, argv);
	utils::Rainmeter::printLogMessages();
	return result;
}

PLUGIN_EXPORT void* LocalPluginLoaderRecursionPrevention_123_() {
	return nullptr;
}
