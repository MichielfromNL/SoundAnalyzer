## Sound Analyzer

When I was working on a Machine Learning sketch on ESP32 to detect sounds I found that there is suprisingly little easy-to-use ESP software for analyzing signals. I needed decibel measurement, FFT, MFCC and Shazam-style fingerprints. So I needed fast FFT and an analyzer class , suitable or embedded applications: i.e. robust, no dependencies on std:: and dynamic memory only at class creation / initialization. 
I earlier found a very fast FFT in C and adapted / wrapped it to [ESPFFT](https://github.com/MichielFromNL/ESPFFT).
Then I found  [Adam Starks GIST library](https://github.com/adamstark/Gist), and several Java snippets for making Shazam fingerprints.

SoundAnalyzer combines all of this in a single class with just a few methods, and the requested characteristics as output

To collect signals, you can use my easy-to-use [ESP32Sampler](https://github.com/MichielFromNL/ESP32Sampler) wrapper  
Which gets a bunch of 16-bits ADC values in pretty accurate millivolts, with no CPU overhead at all. 

Typical approach for Sound ML:
1. Get Samples using ESP32Sampler
2. Process the signal using SoundAnalyzer
3. Make an array with the relevant features form the previous step
4. call a Classifier.predict(array) function, to get a match on the signal

This class makes step 2 very easy. 
But to find the right features for your project is another story, I'll cover that in an other topic on ML modelling

Extra note:  With sound processing and ML, sampling, an important thing is to use RTOS, so that you can  put a sampler task on a separate core. 
Believe me: RTOS is awesome, and very much needed in systems that process lots of data. 

## Installation

- Platformio:  use the Library manager, or simply add https://github.com/MichielfromNL/SoundAnalyzer.git to lib_deps in your project's platformio.ini file
- Arduino IDE:  [get the ZIP](https://github.com/MichielfromNL/ESP32Sampler/archive/refs/heads/main.zip) , extract the contents in a folder in the libraries folder of your Arduino environment
 
 ## Details

 The class has 2 sets of procedures: 3 in the time domain (Pitch , rms and dbSPL), and in the frequency domain : MFCC, Signatures and spectrum features.
 for time domain features, you have to pass the signal. 

 ### dBSPL

 Decibel sound Pressure Level measurements (dBSPL), commonly known as Sound decibels, only work if your samples are mvolts, and you know the sensitivity and gain of the micro (breakout). This is why the Sampler calibrates, without that dBSPL measurements are not possible.
 You can find the microphone sensitivity in the spec sheet of the microphone/ breakout board. The gain: level is the amplification introduced by the circuit between micrisphone and GPIO pin. For example the Max4466 has a potmeter to control gain, so the only way to find out what that level is, is by measuring DDB's and comparing to the actual DB with a sound meter on your mobile phone
 [this thread](https://forums.adafruit.com/viewtopic.php?f=8&p=570094)  explains how that all works but I'm afraid that it still requires some basic knowledge of signal processing to know what happens here.

 To get the frequency domain features you first have to perform the FFT on the signal, then call the relevant functions to get what you need.
 Each function returns an array (pointer). 
 The features array returns all features. To know which feature you need there is an enum list that can be used as an index. there also is a list of tags, 'FeatureNames', the index is the const char *  with the name of the tag, usefull if you want to push features to Json
  
  Size of the array are class members, so that you don't have to 'remember' those after config init

  The fingerprint algorithm, which is similar to what Shazam does, is pretty usefull to classify / identify specific music / sound parts.  See these posts (https://www.toptal.com/algorithms/shazam-it-music-processing-fingerprinting-and-recognition) and (https://www.royvanrijn.com/blog/2010/06/creating-shazam-in-java/) that describe how it works. The basics are published and common knowledge but the entire shazam algorithm is patented, just so you know. 

  The whole thing is very fast. Normally the FFT and features collection take no more than 20 msecs for 1024 samples. If you sample 1024 at reasonable frequencies suchas 8192 (44100 is not needed for sound recognition) that  gives you plenty time to do FFT and e.g MFCC, and pass the results on to the next task (on the other ESp32 core) via an RTOS queue. That's what I do and it works very well. 

 ## Example use

```c++
/*

  Example: collect samples from a connected Mic, process and print results

*/

#include <Arduino.h>
#include "ESP32Sampler.h"  // included for completeness
#include "SoundAnalyzer.h"

typedef sample_t int16_t;
sample_t Samples[1024];  // the collect buffer

using namespace SoundAnalyzer;
Analyzer<sample_t> Processor;

// connect analogue breakout e.g MAX 4466 to pin 34
const gpio_num_t micPin = GPIO_NUM_34;

void setup() {
// Get the default config and set parameters
  Serial.begin(115200);

  // configure the sampler
  SamplerConfig SConfig = Sampler.defaultConfig();
  // 8192 Hz at GPIO34 (default), 1024 sample (default), AC mode, 4 x sampling to reduce noise  
  SConfig.samplefrequency = 8192;
  SConfig.pin = micPin;
  SConfig.mode = SMODE_AC;
  SConfig.multisample = 4;
  Sampler.setConfig(SConfig);

  // configure the analyzer
  AnalyzerConfig PConfig = Processor.defaultConfig();

  // 8192 Hz, 512 bytes FFT, MAx466 mic = 5.012 mvs, gain approx 75 DbSPL (needs calibration)
  // 13 MFCC's,  6 ranges for Shamzam signatures
  PConfig.samplefreq = 8192;
  PConfig.gain = 75;
  PConfig.sensitivity = 5.012;
  Processor.setConfig(PConfig);

  Sampler.Begin();
}

void loop() {

  // We collect more samples than needed for the FFT signal, because standard Decibel measurement
  // requires a specific duration (there is even even an ISO standard for that)
  //  But of course there are plenty other methods to collect equially spaced signal data
  //
  Sampler.Collect(Samples, 1024);
  
  // SPL only works if your samples are mvolts, and you know the sensitivity and gain of the micro (breakout)
  decibel_t dB = Processor.decibelSPL(Samples,1024);

  // Yin (Pitch) is a timedomain feature
  float Yin = Processor.getPitch(Samples);

  // Now Make a spectrum by processing the samples. True = to remove DC
  // Then create features, MFCC, Signature, depending on what we need
  // either use the local array, or use the cached data in the class
  // the audiofeature enums are usd to find the feature in the array
  //
  Processor.doFft(Samples,true);
  float * Features  = Processor.getFeatures();
  float * Mfccs     = Processor.getMfcc();
  signature_t * Signature = Processor.getSignature();

  Serial.printf("dBSpl %d, Pitch %0f, % %.2f, Crest %.2f, Rolloff %.2f etc etc\n", 
            dB, Yin, Features[Fpeakfreq], Features[Fcrest], Features[Frolloff]);

  Serial.print("Mfccs: ");
  for (size_t i=0; i<Processor.NumMfccCoeff; i++) {
    Serial.printf("%.2f%s",Processor.Mfccs[i],i<Processor.NumMfccCoeff ? ",": "");
  }
  Serial.println();
  
  Serial.printf("Signature: ");
  for (size_t i=0; i<Processor.SignatureLen; i++) {
    Serial.printf("%d%s",Signature[i],i<Processor.SignatureLen ? ",": "");
  }
  Serial.printf("\nSignaturehash = %ul\n", Processor.getSignatureHash());

}

```

In another example I will xplain how I use this stuff to classify sound types, with ML from this guy [EloquentML](https://eloquentarduino.com/arduino-machine-learning/) . It is pretty easy: the Arduino requires just 2 lines of code. 
The preparation and feature selection is another story though

Have fun!
