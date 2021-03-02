# LocalPluginLoader plugin

## Overview

LocalPluginLoader is a plugin for [rainmeter application](https://www.rainmeter.net).

LocalPluginLoader allows rainmeter skins to load plugins not only from the
[default plugin folder](https://docs.rainmeter.net/manual/variables/built-in-variables/#PLUGINSPATH) but from any custom folder.

This plugin is intended to be used for testing while developing other plugins.
It's useful for comparing several versions of some plugin.
Two rainmeter skins can use different versions of the same plugin without manual name collision management.

## Usage

In rainneter skin file replace direct plugin measure description with LocalPluginLoader description:
```
[MeasureName]
Measure=Plugin
Plugin=LocalPluginLoader
PluginPath=path/to/plugin/folder
```
All options are transparently passed to the specified plugin.
`path/to/plugin/folder` folder must contain files `64-bit.dll` and `32-bit.dll` if you want to support both currently existing x86-compatible architectures.
For testing purposes it's enough to only have your current architecture version.

It's convenient to use local rainmeter [resources](https://docs.rainmeter.net/manual/variables/built-in-variables/#@)
or [skin](https://docs.rainmeter.net/manual/variables/built-in-variables/#CURRENTPATH) folders for .dlls storing, but LocalPluginLoader doesn't restrict plugins paths,
so you can also use any other folder on your PC.

## Download

This plugin is not intended to be distributed so is doesn't have any official releases.
If you want to use this, you will have to compile it yourself.

## Building

Plugin LocalPluginLoader can be built as a part of solution that contains everything in the repository.
See [main readme](README.md) for details.
