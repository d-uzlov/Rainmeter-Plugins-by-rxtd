
#include "RainmeterAPI.h"
#include "LocalPluginLoader.h"

PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	*data = new LocalPluginLoader(rm);
}
PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue) {
	static_cast<LocalPluginLoader*>(data)->reload(maxValue, rm);
}
PLUGIN_EXPORT double Update(void* data) {
	return static_cast<LocalPluginLoader*>(data)->update();
}
PLUGIN_EXPORT LPCWSTR GetString(void* data) {
	return static_cast<LocalPluginLoader*>(data)->getStringValue();
}
PLUGIN_EXPORT void Finalize(void* data) {
	delete static_cast<LocalPluginLoader*>(data);
}
PLUGIN_EXPORT void ExecuteBang(void* data, LPCWSTR args) {
	static_cast<LocalPluginLoader*>(data)->executeBang(args);
}
PLUGIN_EXPORT LPCWSTR sv(void* data, const int argc, const WCHAR* argv[]) {
	return static_cast<LocalPluginLoader*>(data)->solveSectionVariable(argc, argv);
}
PLUGIN_EXPORT void* LocalPluginLoaderRecursionPrevention_123_() {
	return nullptr;
}
