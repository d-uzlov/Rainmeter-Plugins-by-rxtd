AudioAnalyzer [1.1]

This plugin can measure RMS and peak of sound wave, do Fourier Transform, draw waveform, draw spectrogram.
All can be done separately for different channels.
Yes, that sounds almost like description of AudioLevel. See differences below.

Examples of usage:
Plugin has lots of options, but if you only want to make something pretty, then will probably be happy with default values, so it's actually very easy to use.


Documentation:

Plugin uses parent-child model. You should declare one parent measure and zero or more child measured to get information. Child measures don't do any calculations.
Parent measures use following structure of calculations:
There are several Channels (defined in your sound card settings), each being processed independently. A Channel has zero or more Handlers. Each Handler do some computations, which can be fetched using Child measures. Handlers are called in the defined order, so it's important to declare dependent Handlers in the right order.
Also each Handler has properties which can be accessed through Child measures or SectionVariables.

To make declaration of handlers easier they are declared in groups named Processing. A Processing declares that of N specified Channels each has M specified Handlers in specified order. If one channel is noted in several Processings then the order of Handlers between different Processings is undefined.

Declarations have following format:
option1 value1 | option2 value2
Same format is used by Shape meter.
Options are separated by pipe symbol '|', option name is separated from its value by space.
If an option value consists of several subvalues, then this subvalues are separated by comma (','): option a,b,c.

In every place where value of an option is number, math can be used. Supported operations are +-*/^, parentheses are allowed, all numbers are calculated as floating pointer (but if the option requires integer value, then at the end the fractional part is ignored).
For example: option 3.3*10^-2 (same as 0.033).
Division by zero is treated as 0.

Additional info:
There are two major types of data you can get from from plugin: values of handlers and additional info.
Parent measure has section variable "resolve", which can be used to get additional info.
You can get information about parent measure itself or properties of handlers.
Possible arguments:
"current device"
	"status" : 1 if everything works, 0 otherwise
	"status string" : 'active' if everything works, 'down' otherwise
	"name" : human readable name of current audio device. I.g., "Speakers (Realtek High Defivition Audio)"
	"id" : id of current audio device. I.g., "{0.0.0.00000000}.{22494623-1005-435a-8f61-4c8bae227550}"
	"format" : human readable description of format of current device. I.g., "PCM 32b, 192000Hz, 5.1"
"device list" : list of existing devices in the following format: "<id> <name>". List is empty by default and is updated only on demand. Use bang [!CommandMeasure <ParentMeasureName> updateDevList] to update device list.
"prop" : get property of a handler. You have to specify Channel, Handler and PropName. I.g., [&MeasureParent:resolve(prop,frontLeft,band,bands count)].


Child measure options:
Parent : string
	Name of the parent measure. Parent measure should be in the same skin.
Channel : string : Auto
	Channel to get data from.
	See Channels below for the list of possible values.
ValueId : string
	Name of the handler in parent measure.
Index : integer : 0
	Index of value in handler.
Gain : float : 1.0
	Values are multiplied by this gain.
Clamp01 : boolean : 1
	When 1 resulting values are always in range [0; 1].
StringValue : { Number, Info } : Number
	When Number: string values of measure match number value.
	When Info: InfoRequest option determines string value of measure.
InfoRequest : string
	StringValue=Info determines string value of measure.
	Usage: same as SectionVariables on parent measure, but without function call. I.g., prop,frontLeft,band,bands count.
NumberTransform : { Linear, DB, None} : Linear
	When Linear number value is: value*Gain.
	When DB number value is: 20 / Gain * log10(value) + 1.0.
	When None number value is: 0.


Parent measure options:
Port : { Input, Output } : ReadOnly
	Specifies whether input of output should be captured.
DeviceID : string : ReadOnly : Case-Sensitive
	Device identificator. Can be obtained from Device List.

TargetRate : integer >= 0 : 41000
	Target sampling rate. Sampling rate can vary from 41000 (or maybe even less for some devices) to 384000 and potentially more. But you probably don't need that much.
	Plugin can apply low-pass filter to reduce effective sampling rate used in processing.
	Final rate is always >= than TargetRate.
	You probably don't want to change it from default.
	Set to 0 to disable resampling.

Processing : string
	List of processing units separated by spaces.
Processing_<id> : <list of options>
	Description of processing unit.
	Contains list of channels and list of sound handlers.
	Possible options: channels, handlers.
	Example: channels FrontLeft, FrontRigth | handlers rms, peak, fft, band
Handler_<id> : <list of options>
	Description of sound handler.
	Contains handler type and handler-specific options.
	Possible types are: RMS, Peak, FFT, BandResampler, BandCascadeTransformer, WeightedBlur, UniformBlur, FiniteTimeFilter, LogarithmicValueMapper, Spectrogram, Waveform.
	See description of certain types for details.
FreqList_<id> : <list of options>
	Description bound frequencies for band handler.
	See below.

Handler types

RMS
Type : { rms }
Measures power of signal. Can correlate with perceived loudness.
Options:
attack : float >= 0 : 100
	Time in milliseconds of signal increase from min to max.
decay : float >= 0 : attack
	Time in milliseconds of signal decrease from max to min.
resolution : float > 0 : 10
	Time in milliseconds of block in which signal is averaged before calculating RMS.
Example: type rms | resolution 5 | attack 1000
Props:
"block size" : size of the block in which values are averaged, in audio points.
"attack" : value of attack option
"decay" : value of attack option

Actual attack and decay times have specified values when measuring the response to Heaviside step function and mirrored Heaviside step function respectively. More complex signals can distort these values.
Same applies to all other Handlers where Attack/Decay values are used.

Peak
Type : { peak }
Maximum of [absolute] value of signal over period of time. Use when you need to know if there was any sound on not.
Options:
attack : float >= 0 : 100
	Time in milliseconds of signal increase from min to max.
decay : float >= 0 : attack
	Time in milliseconds of signal decrease from max to min.
resolution : float > 0 : 10
	Time in milliseconds of block in which maximum is found.
Example: type peak | resolution 1 | attack 10 |  decay 1000
Props:
"block size" : size of the block in which maximum if found, in audio points.
"attack" : value of attack option
"decay" : value of attack option

Fourier Transform
Type : { fft }
Fourier Transform of the signal. Gives you values of frequencies from 0 to SamplingRate/2 (=Nyquist frequency).
Should always be used with Band handler. You can get values directly but that doesn't make much sense.
Options:
Attack : float >= 0 : 100
	Time in milliseconds of signal increase from min to max**
Decay : float >= 0 : attack
	Time in milliseconds of signal decrease from max to min**
SizeBy : { BinWidth, Size, SizeExact } : BinWidth
	Determines how FFT size is calculated.
	Do not change. Anything but resolution will cause problems when exact settings of a computer where your skin will be running is not known.
	BinWidth determines time in milliseconds of block on which FFT is applied.
	Size and SizeExact determines size of the FFT directly. When Size is used, actual size is adjusted to values which can be calculated optimally. SizeExact prevent this. Neither one should be used for anything but testing purposes.
BinWidth : float > 0 : 100
	Width of one FFT result bin.
	Size is automatically set to make actual resolution to be equal to target resolution.
	The less this option is, the more detailed result you get, but the less frequently values change.
	Recommended values are [5, 1000]. 
Size : integer > 0
	Used when sizeBy is size or sizeExact. Specifies size of the FFT in data points.
Overlap : float in [0, 1] : 0.5
	Specifies how much FFT windows is moved after each iteration.
	Values below 0.5 will make you lose information.
	Complexity of the FFT is correlated to overlap as 1/(1-overlap), so values close to 1.0 will greatly increase complexity.
CascadesCount : integer > 0 : 5
	Plugin can increase resolution in lower frequencies using cascades of FFT.
	See FFT Cascades below.
CorrectZero : boolean : 1
	Do not change. Setting this to 0 will give you mathematically incorrect results.
	Zeroth FFT bin is incorrect by default as it consists of only sine (cosine is missing). This can be corrected by multiplying it by 2^0.5, if we assume that sine and cosine should have same values.
TestRandom : float >= 0 : 0
	When not zero input signal in FFT is replaced with white noise with amplitude of testRandom. Noise is mixed with silent: X seconds of noise, X seconds of silence.
	Use to test the response of handler.
RandomDuration : float : 1000
	Controls duration of noise and silence when TestRandom != 0.
Example: type fft | attack 10 | resolution 100 | cascadesCount 10
Props:
"size" : size of the FFT.
"attack" : value of attack option
"decay" : value of attack option
"cascades count" : value of cascadesCount option
"overlap" : value of overlap option
"nyquist frequency" : Nyquist frequency of first cascade
"nyquist frequency <integer>" : Nyquist frequency of Nth cascade
"dc" : DC value of first cascade
"dc <integer>" : DC value of Nth cascade
"resolution" : resolution of first cascade
"resolution <integer>" : resolution of Nth cascade

BandResampler
Type : { BandResampler }
Allows you to get one or more bands from FFT result.
Each cascade of FFT result is resampled to fit into specified list of bands.
Options:
Source : string
	Name of FFT handler.
FreqList : string
	Name of description of frequencies list.
	See below.
MinCascade : integer >= 0 : 0
	Min cascade that should be used in band value calculating.
	Set to 0 to use all cascades.
	Set to value in [1; cascadesCount] to use cascades starting from cascadeMin.
MaxCascade : integer >= 0 : 0
	Max cascade that should be used in band value calculating.
	Set to [cascadeMin; cascadesCount] to use cascades from cascadeMin to cascadeMax.
	Set to [0, cascadeMin-1] to use all cascades up from cascadeMin.
IncludeDC : boolean : 1
	When set to 0 zeroth bin of FFT is ignored.
ProportionalValues : boolean : 1
	When 1 values are multiplied by log of band width to show bigger values in higher frequencies.
	Set to 0 to disable this behavior.
Example: type band | source fft | freqList flist
Props:
"bands count" : count of bands as specified by freqList.
"lower bound <integer>" : lower frequency bound of Nth band.
"upper bound <integer>" : upper frequency bound of Nth band.
"central frequency <integer>" : center frequency of Nth band.

WeightedBlur
type : { WeightedBlur }
Allows you to blur values, with blur radius relative to band weight.
Options:
RadiusMultiplier : float >= 0 : 1.0
	When 0 disables blur.
	Else defines a multiplier to blur strength.
	Increase to have more uniform values. Decrease to have more distinct values.
MinRadius : float >= 0 : 1
	Lower bound for blur radius.
MaxRadius : float >= MinBlurRadius : 20
	Upper bound for blur radius.
BlurMinAdaptation : float : 2
	Blur for different cascades can be different.
	MinRadius for cascade is: MinRadius * BlurMinAdaptation^cascade.
BlurMaxAdaptation : float : BlurMinAdaptation
	MaxRadius for cascade is: MaxRadius * BlurMaxAdaptation^cascade.
Example: type weightedBlur | source resampler
Props: none.

UniformBlur
type : { UniformBlur }
Allows you to blur all values in one way.
Options:
Radius : float : 1
	Radius of blur for first cascade.
RadiusAdaptation : float : 2
	Radius for cascade is Radius * RadiusAdaptation^cascade.
Example: type UniformBlur | source mapper
Props: none.


BandCascadeTransformer
type : { BandCascadeTransformer }
Allows you to combine several cascades into one set of final values.
Options:
MixFunction : { Product, Average } : Product
	Determines how different cascades are mixed.
	When Average: result = (c1 + c2 + ...) / N.
	When Product: result = (c1 * c2 * ...) ^ (1 / N).
MinWeight : float >= 0 : 0.1
	Cascades with weight below MinWeight are thrown away.
TargetWeight : float >= MinWeight : 2.5
	Minimum target weight of band.
	Weight is count of FFT bins that were used to calculate band value. Weight of single bin may be very small (e.g., 0.05) when band frequency width is much less than FFT bin width.
	Cascades are summed (and averaged at the end) until sum of their weights is less than targetWeight.
WeightFallback : float in [0; 1] : 0.4
	When cascade values are too small they can be discarded despite having enough weight, resulting in target wight not being hit.
	WeightFallback specifies a threshold below which values with weight below MinWeight are accounted.
ZeroLevelMultiplier : float >= 0 : 1
	Multiplier for a threshold below which value is considered effectively zero.
ZeroLevelHardMultiplier : float in [0; 1] : 0.01
	Multiplier for a threshold below which value is considered invalid. Relative to ZeroLevelMultiplier.
ZeroWeightMultiplier : float >= 0 : 1
	Multiplier for a threshold below which weight is considered effectively zero.
	Replacement for MinWeight when Fallback is happened.
Example: type BandCascadeTransformer | source resampler | minWeight 0.25 | targetWeight 10
Props:
"cascade analysis" : technical string containing info about all bands: index, final weight, min cascade used, max cascade used.
"min cascade used" : index of first cascade that was used in calculation of values.
"max cascade used" : index of last cascade that was used in calculation of values.


FiniteTimeFilter
type : { FiniteTimeFilter }
Filters values so they change smoothly.
Options:
SmoothingFactor : integer > 0 : 1
	Analogue to AverageSize in Rainmeter. Last N=SmoothingFactor values are combined to make resulting value smoother.
SmoothingCurve : { Exponential, Linear, Flat } : Exponential
	Determines how resulting value is calculated.
	Flat: sum[i=1; SmoothingFactor](pastValue).
	Linear: sum[i=1; SmoothingFactor](pastValue * i).
	Exponential: sum[i=1; SmoothingFactor](pastValue[i] * ExponentialFactor^i).
ExponentialFactor : float : 1.5
	Affects values smoothing when smoothingCurve=exponential
Example: type FiniteTimeFilter | source mapper | smoothingFactor 3 | smoothingCurve flat
Props: none.


LogarithmicValueMapper
type : { LogarithmicValueMapper }
Transforms final results into dB form and then map to range [0; 1] with 0 being "-sensitivity dB" and 1 being "0 dB" with optional offset.
Actual values may be > 1.
Options:
Sensitivity : float > 0 : 35.0
	Band Handler's output values are in dB form, with 1 being "0 dB" and 0 being "-sensitivity dB".
	Increase this value to see lower values in bound of [0; 1], decrease it to have clearer picture of only high values.
Offset : float : 0.0
	Offset to final value. Meaningful values are expected to be in range [0; 1], but if it don't you can use this option.
Example: type LogarithmicValueMapper | source transformer | offset -0.2
Props: none.


Spectrogram
Type : { spectrogram }
Shows band's values changes in time.
Generates BMP image on disk. Height of the image is determined by number of point in source Handler. Name of the file is: "<folder>\spectrogram-<channel>.bmp". Color of the pixel is linearly interpolated between baseColor and maxColor in sRGB space (yes, I know, that's mathematically incorrect).
Options:
Source : string
	Name of source handler. Usually Band. You can use it on FFT too, but it's kinda pointless.
Length : integer > 0 : 100
	Count of points in time to show. Equals to resulting image width.
Resolution : float > 0 : 50
	Time in milliseconds of block that represents one pixel in image.
Folder : path : <skin folder>
	Path to folder where image will be stored.
BaseColor : color : 0,0,0,1
	Color of the space where band values are below 0.
MaxColor : color : 1,1,1,1
	Color of the space where band values are above 1.
Example: type spectrogram | length 600 | resolution 25 | source band | folder [#@]
Props:
"file" : path of the file in which image is written.
"block size" : size of the block that represents one pixel in image, in audio points.


Wave
Type : { Waveform }
Shows min and max values of the wave.
Generates BMP image on disk. Name of the file is: "<folder>\wave-<channel>.bmp".
Options:
Width : integer > 0 : 100
	Resulting image width. Equals to count of points in time to show.
Height : integer > 0 : 100
	Resulting image height.
Resolution : float > 0 : 50
	Time in milliseconds of block that represents one pixel in image.
Folder : path : <skin folder>
	Path to folder where image will be stored.
LineDrawingPolicy : { Always, BelowWave, Never } : always
	Resulting image can have horizontal line indicating zero value.
	always — draw line above wave.
	belowWave — wave will hide line.
	never — don't draw line.
BackgroundColor : color : 0,0,0,1
	Color of the space where wave is not drawn.
WaveColor : color : 1,1,1,1
	Color of the wave.
LineColor : color : waveColor
	Color of the line in zero values.
Example: type waveform | width 200 | height 100 | resolution 17 | folder "[#@]" | backgroundColor 0.6,0.6,0.6,0.8 | waveColor 0.1,0.1,0.1,2 | lineColor 0.5,0.5,0.5
Props:
"file" : path of the file in which image is written.
"block size" : size of the block that represents one pixel in image, in audio points.


Frequency lists
Band handlers gather frequency information based on frequency list.
Frequency list can contain arbitrary list of band bounds.
There are 3 types of bound definitions:
linear: 'linear' count minFreq maxFreq
log: 'log' count minFreq maxFreq
custom: freq1, freq2, ...
Several types can be combined with | symbol.
Example: log 100 40 10500 | linear 10 10500 20000
This will define 50 bands, first 40 are logarithmic between 40 Hz and 10.5 kHz, last 10 are linear between 10.5 kHz and 20 kHz.


FFT cascades:
FFT is always a trade-off between frequency resolution and time resolution.
FFT of size N gives you N/2 bins on range [0, SampleFreq/2], which correspond N values of signal.
If you increase N (with same SampleFreq), then you will get better frequency resolution, but worse time resolution, and bigger complexity of algorithm.
If you decrease SampleFreq (with same N) you will get better frequency resolution and much lower complexity, but worse time resolution.
You can't have both fast and detailed FFT. But there is a solution: you can make a cascade of FFTs, decreasing SampleFreq in every next cascade. This way you will get bad frequency resolution and good time resolution on higher frequencies and vice versa in low frequencies. However, logarithmic scale, which you most probably be using, requires much better resolution in lower frequencies, so this can allow you to have decent spectrum without loosing too many details in high frequencies. As a bonus you have very low complexity.
Unfortunately, it's not straightforward how to get the best result from mix of values.


Bands:
Transforming FFT result into bands can be quite complex, depending on what you want to get.
1) The simplest case: FFT has only one cascade, you want N bands.
Then you define a chain of handlers with types FFT, BandResampler, LogarithmicValueMapper, and connect them using "source" option. Then you can take you final values from LogarithmicValueMapper.
2) There are several FFT cascades. THen you need a BandCascadeTransformer to gather all cascades into single array of value. It need to be placed after BandResampler.
3) You want to perform blurring of time filtering.
There are several places where you can add handlers for this.
Between BandResampler and BandCascadeTransformer can be handlers of types WeightedBlur, UniformBlur, FiniteTimeFilter in any variations. You can use none, you can use blur only, you can use blur and then filtering, etc.
You can also place handlers after BandResampler: UniformBlur, FiniteTimeFilter, LogarithmicValueMapper, also in any variations.
You can't place LogarithmicValueMapper before BandResampler, you can't place WeightedBlur after BandResampler.
I don't know which combination of handlers will give you better results.


Colors:
Unlike Rainmeter, this plugin defines color channels in range [0; 1]. This correspond to [0; 255] in Rainmeter.
Colors channel values can actually be anything (i.g., 10,-5,0,50), but resulting are clamped into [0; 1] before drawing images. You can use it to make linear interpolation a bit less linear.
Beware though, that defining colors outside range of [0; 1] may give you oversaturated picture.

Channels:
Plugin supports 9 channels: 8 real and 1 fake.
Real channels:
FrontLeft or FL
FrontRight or FR
Center or C
LowFrequency or LFE
BackLeft or BL
BackRight or BR
SideLeft or SL
SideRight or SR
These channels correspond to real channels in layout that you can define in your audio card settings.
There is a fake channel: "Auto". This channel is either average of FrontLeft and FrontRight if they both exist, or Center if it exist, or average of first two channels in the layout if there are at least 2 channels, or else fist and only channel in the layout.
This generally gives you reliable source, but can be counter-intuitive (i.g., two sine waves of inverted amplitudes in left and right channels will make Auto channel be exactly zero).

Comparison to AudioLevel:
AudioLevel doesn't have Spectrogram.
AudioLevel doesn't correct attack/decay values when sample rate changes.
AudioLevel doesn't do proper channel mapping. For example, it doesn't recognize 4.0 channel layout.
AudioLevel can't resample wave when sampling rate is high.
AudioLevel only allow you to declare one calculation of values for all channels. It doesn't allow you to define FFT for only one channel and RMS for one another.
AudioLevel calculates values differently:
RMS: AudioLevel doesn't have RMS. It calculates filtered squared wave value instead.
Peak: AudioLevel doesn't have Peak. It calculates filtered absolute wave value instead.
FFT: AudioLevel doesn't do proper normalization of the values. This leads to great change of values when size if changed. To be precise, FFT of size 16384 is 32 times bigger than FFT of size 16.
AudioLevel doesn't have cascaded FFT.
AudioLevel don't let you define relative size of the FFT, only absolute (this causes a big problems when sample rate of your sound card changes or when you want to distribute your skin).
AudioLevel doesn't correct zeroth value of the FFT.
Band: AudioLevel's Band feature always calculates sum of the FFT bins instead of average, which is not always desirable.
AudioLevel calculates bound using squared values of the FFT, which leads to incorrect filtering and (as far as I can tell) incorrect final values (they are much spikier than they need to be).
AudioLevel's Band doesn't blur values.
