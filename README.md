# A set of various plugins for Rainmeter application

## Overview

This repository contains 3 separate plugins:

1. [AudioAnalyzer](AudioAnalyzer)
2. [LocalPluginLoader](LocalPluginLoader)
3. [PerfMonRxtd](PerfMonRxtd)

See respective plugins' pages to learn about their purpose and documentation.

## Building

All projects in this repository are developed using Visual Studio IDE and building tools.

Project uses C++17 language, so compatible Visual Studio version is required. VS 2017 and up should be OK, but only VS 2019 is tested for compatibility.

No external dependencies are used, so build should be automatic.

Solution is configured to use `git` to embed commit hash into .dll version.
Having `git` in PATH is not required but if you don't have git avaiable in command line then binaries won't contain full version information.
