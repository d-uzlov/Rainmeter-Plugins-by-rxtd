# AudioAnalyzer Plugin

**High Performance Audio analyzing plugin for Rainmeter.**

## **Table of Content:**

- [Introduction](##Introduction)
- [Why using AudioAnalyzer?](##Why%20using%20AudioAnalyzer?)
- [Getting Started](##Getting%20Started)
  - [.rmskin Examples](##Getting%20Started)
  - [Spectrogram](##Spectrogram)
  - [Spectrogram with Waveform](##Spectrogram%20with%20Waveform)
  - [Spectrum](##Spectrum)
  - [Waveform](##Waveform)
  - [Loudness meter](##Loudness%20meter)
  - [Curve Tester](##Curve%20Tester)
  <!-- - [](##) -->
- [Building your skin with AudioAnalyzer plugin](##Building%20your%20Skin%20with%20AudioAnalyzer%20plugin)
  - [Automatic reconfiguring](##Automatic%20reconfiguring)
    <!-- - [](##) -->
    <!-- - [](##) -->

---

## **Introduction**

Rainmeter has AudioLevel plugin, which provides you a basic functionality on extracting useful data from audio stream. However, the "basic" is the very fitting term here.

What do users want to see regarding audio visualization? Users may want to see a "loudness meter" and some fancy visualization frequencies.

AudioLevel plugin doesn't really have a correct loudness meter. It has "RMS" and "Peak" calculations, which will show you if there was any sound, but they won't really give you an insight on real loudness level. I will post loudness discussion somewhere nearby.

For fancy visualizations AudioLevel has Fourier transform. Kind of. It has several issues both from the mathematical nature of FFT algorithm and incorrect implementation.

There is also a second version of that plugin out there, AudioLevel2, that can also draw a waveform image.

---

## **Why using AudioAnalyzer?**

AudioAnalyzer Plugin has much better ways to extract loudness information from audio stream, it has better implementation of fourier transform with ways to fight fundamental flaw of FFT algorithm, it also can draw a waveform, and it can draw a spectrogram (not a spectrum which only give you slice of frequency information, but a spectrogram).

By the way, drawing the spectrogram is the feature I was missing the most, and it was the main reason why I created this plugin in the first place.

On top of that, among other features, AudioAnalyzer 1.1.5 brings automatic switching between audio devices (including WASAPI exclusive mode handling) and [async computing](###Threading) for better UI performance.

---

## **Getting Started**

Here are simple [examples](.rmskin-link-placeholder) of what this plugin can do:

## Spectrogram

[Image](Image)

This plugin can draw fancy spectrograms (don't confuse them with spectrum meters).
Spectrogram is an image of how spectrum of the sound has changes over a period of time.
It generates an image file on you drive 60 times per second (well, it is optimized not to write when there are no changes), so if you care about your SSD life you probably want to create some small RAM-drive and put it there using 'folder' property of the spectrogram sound handler.

"Bugs & Feature Suggestions" forum is always open for you complaints about how Rainmeter plugins can't transfer pictures directly to Rainmeter, without writing them on drive first. If you are a C++ developer and you know some hacks about directly modifying skins to show pictures, your pull requests are also welcome.

## Spectrogram with Waveform

[Image](Image)

This skin shows you that you can draw synchronized waveform and spectrogram. If their appropriate options matches then they will update synchronously.

## Spectrum

[Image](Image)

Spectrum is also available.

## Waveform

[Image](Image)

This plugin can draw fancy waveform images.
Same thing as with spectrogram: writing on drive, SSD life.

<!-- notes on performance -->

## Loudness meter

[Image](Image)

Digital loudness is always measured in decibels relative to full scale, or dBFS. Simply put, this means that 0 dB is the loudest possible sound, -20 dB is something quieter, and -70 dB is something almost inaudible.

This skin shows something that roughly correlates with perceived loudness. Measuring real loudness is a very complex and challenging topic (and actually this can't be done digitally without special equipment). This skin uses an approximation, and in my opinion this approximation is good enough.

This skin displays both numeric value of current loudness and a bar that shows relative loudness, where empty bar indicates silence and full bar indicated a very loud sound.

To do this, it converts decibels in some range (particularly, in range from -50 dB to 0 dB) into range from 0.0 to 1.0 and clamps them to that range so that Bar meter can display it.

However, numeric values are not converted and not clamped, so they can show that there is some sound that is too quiet to be displayed on Mar meter.

**Note** that for loudness calculations there is a `filter like-a` property specified. Without this property the values it displays would be very-very different from perceived loudness.

There is also a peak meter in the [.rmskin file](.rmskin-link-placeholder). It acts similar to loudness meter, except what it shows doesn't correlate with perceived loudness. In sound industry peak meters are usually used to detect possible clipping. It just a showcase that you can detect sound peaks, don't use it when you need loudness.

Note that for loudness calculations there is a no 'filter' property specified, which means that there is no filtering done on sound wave. If you were to specify some filter, then it wouldn't measure actual sound peaks anymore. It would measure peaks on some filtered values instead, which is probably not what you want.

## Curve Tester

[Image](Image)

There is also a simple tester to show you how different handler chains affect values.

Orange line shows raw RMS value.
Green line shows RMS values converted to decibels. Its purpose is to show that you should always convert your values to decibels, because orange line only reacts to very strong sound. There are quiet sound that you can still clearly hear, and which are you can see on green line but can not see on orange line. If you were to just ramp up value of RMS then it would show you quiet sounds but then loud sounds would just clip on the upper edge of the graph.

There are also blue and white lines. Their purpose is to show that order of handlers matter. Blue line shows RMS values that were first converted into decibels and then filtered/smoothed. White line shows values that were first filtered/smoothed and then converted into decibels. As you can see, the difference is very noticeable. It's up to you to decide which way of calculations suits you better.

---

## **Building your skin with AudioAnalyzer plugin**

First of all, I spent a lot of time writing meaningful log error and warning messages. When you are writing a skin, have a Rainmeter log window open. If you have made some mistake with syntax, there is a good chance that log will have a message about it. It's very helpful.

Plugin doesn't have a fixed set of possible ways to calculate some values. Instead, it acts somewhat like a DSP-utility and provides you a set of building blocks called 'sound handlers' that may be combined in any amount into any tree-like graph.

They are defined in groups called 'processing'. Each processing have: a set of channels on which it acts, a chain (or a tree) of sound handlers that it manages, and some options to transform sound wave, like filtering to make loudness calculations better match perceived loudness.

Usually you only use one processing. However, there are cases where you would want to have several processings. For example, you may want to make FFT of Auto channel, and you don't need it on any other channel, and at the same time you want to display loudness for each available individual channel. In cush case for performance reasons you would want to define one processing that performs only FFT and some transformations on it, and a separate processing that does only loudness calculations.

Or if you want to show spectrogram you should use some filtering in processing, and if you want to show waveform then you shouldn't use any filtering. In such case you would want to define two processings, one if which will use filtering and the other one won't.

If you want to get frequency values, then your processing should contain something like this:

[Image](Image)

FFT -> BandResampler -> [optional transforms] -> BandCascadeTransformer -> [optional transforms] -> ???
On the place of '???' there may be a Spectrogram handler if you are making a skin with a spectrogram, or nothing, if you are making a spectrum skin (then you will probably access values through Child measures).
If you don't want to use cascaded FFT, then you should drop BandCascadeTransformer, and only have a chain like this: FFT -> BandResampler -> [optional transforms]
You should never use raw output of the FFT handler because number of its output values is unspecified. BandResampler allows you to get required number of values which are properly sampled from FFT result. If for some reason you want to get values that are not logarithmically scaled across frequency range, then there are options for linear or custom scale.

## Automatic reconfiguring

If you want your skin to react to audio device properties, this plugin provides you with some information.
For example, there is a plugin section variable "device list output" that will give you a list of available audio devices, that you can parse with a LUA script, and then somehow show user a list of devices to choose from. Of there is a "current device, channels" section variable that will give you a list of available audio channels.

[See plugin syntax section for details.]()

---

## **Syntax**

A plugin is designed around a central computing point: a parent measure. It has all the descriptions of what should be calculated, and how it is named.
To access this data you can either use child measures or section variables.

Parent measures are distinguished from child measures by Type option, which must be either Parent or Child.

Plugin is quite complex, and so does its syntax. But you don't have to use all the options. Bare minimum versions of skins using this plugin are very simple.

### Note: Value Type, and enumerations inside one string.

This plugin heavily relies on option enumerations inside one string, similar to how Shape meter describes its objects, so you might already be familiar with it.

Most of the rainmeter meters and measures, String meter as example, would write it like this:

```ini
Text=This is a Text
FontColor=255,255,255,255
FontSize=20
```

Imagine you have 10 meters or measures each having 10 properties, now you have to manage 100 lines describing options for just a one plugin. Maybe some of the properties are itself complex and require several options for full descriptions. And each option would have a very long name. Yikes.

This plugin would write it like this:

```ini
Object-ABCD=length 10 | color black
```

_Notice the hyphen symbol '-' after the word Object._

Much simple to write and read, infinitely easier to manage complex description.

In the documentation options that use that syntax to define properties are called list of named properties. In such a list different options are separated with '|' (pipe symbol) and option name is separated from option value with ' ' (white space).

All options are case-insensitive. ABC, abc and aBc will be treated the same, and if this documentation specifies value Abc, then all three of these will match it.

This plugin supports math. In every place where number (either float or integer) is expected you can you math operations to calculate it.
Supported operations are +-*/^, parentheses are allowed, all numbers are calculated as floating pointer. If you try to divide something by zero, result will be replaced with 0.
For example, you can write "(5*10^2 + 10)\*0.7" instead of "357".

## Measure Options and its Syntax

**Type:** Specifise the type of the measure.

- Parent: The measure which analyzing the audio.

- Child: the measure that recives processed signal.

_Example:_

```ini
Type=Parent
; or
Type=Chid
```

**MagicNumber:** This plugin has an old version which is partially compatible with the new one.

To avoid breaking changes while still delivering improved experiance to old skin users, option MagicNumber was introduced. It is used to determine whether plugin should run in the new or in the legacy mode. Always set MagicNumber to value 104, or else plugin will run in legacy mode which have many default values different from what is described in this documentation.

- 0: The plugin will work in legacy mode, legacy mode which have many default values different from what is described in this documentation.

- 104: The plugin will work as described in this documentation.

`Default Value: 0`

_Example:_

```ini
MagicNumber=0
; or
MagicNumber=104
```

<!--  Source Option
_Example:_

```ini

``` -->

**Processing:** List of processing unit names separated by pipe symbol. Names in this list must be unique.

`Default Value: None(empty)`

_Example:_

```ini
Processing= process1 | process2
```

### Note: If possible Avoid spawning many processess because of(some reason i'll try to explain properly), Instead modify existing ones.

**Processing-_name_:** Description of processing unit.

- Channels: list of channels that this processing must process of they are present in the audio device.

- Handlers: list of handlers that this processing must call in the specifier order.
<!-- what is sample rate means for the end user? -->
- TargetRate: Very high sample rates aren't very helpful, because humans only hear sounds below 22 KHz, and 44.1 KHz sample rate is enough to describe any wave with frequencies below 22.05 KHz. But high sample rates significantly increase CPU demands, so it makes sense to downsample sound wave. And typical modern PC is capable of running 192 KHz, which is totally redundant.
  Final rate is always >= than TargetRate. So if you rate is 48000 and TargetRate is 44100, then nothing will happen. If you sampling rate is less than TargetRate then nothing will happen.
  Setting this to 0 disables downsampling completely.

  - `Default Value: 44100`

- Filter: Human hearing is not uniform across different frequencies. To accommodate for that, filters are usually used. They alter sound way in some way. See filtering discussion for details. This plugin provides some prebuilt filters, so you don't need to think about them.
  None means no filtering.
  <!--Options: -->

  - none

  - like-a: means filter that is similar to A-weighting.

  - like-d: like-d means filter that is similar to D-weighting.

    Both like-a and like-d will make frequency response roughly match human perception of sound. Like, very roughly, but it's much better than nothing. I personally find like-d to work the best, despite A-weighting usually being considered more accurate for home usage.

    See Filters discussion for details on how to describe custom filter.
    Note, that any filter aside from "none" will alter the audio signal, so if you want to use Waveform handler to display actual sound wave, then it's a good idea to create a separate processing with disabled filter.

  - like-rg:

  - custom(_filter description_):

  - `Default Value: None`

_Example:_

```ini
Processing-proc1=channels FrontLeft, FrontRigth | handlers loudness, fft, resampler | filter like-d
```

**Option:**

`Default Value: 0`

_Example:_

```ini

```
