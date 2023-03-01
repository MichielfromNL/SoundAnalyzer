/*

  Example: collect samples from a connected Mic, process and print results

*/

#include <Arduino.h>
#include "ESP32Sampler.h"  // included for completeness
#include "SoundAnalyzer.h"

typedef sample_t int16_t;
typedef decibel_t unsigned short;

sample_t Samples[1024];  // the collect buffer

using namespace SoundAnalyzer;
Analyzer<sample_t> Processor;

// connect analogue breakout e.g MAX 4466 to pin 34
const gpio_num_t micPin = GPIO_NUM_34;

void setup() {
// Get the default config and set parameters
  Serial.Begin();

  // configure the sampler
  SamplerConfig SConfig = Sampler.defaultConfig();
  // 8192 Hz at GPIO34 (default), 1024 sample (default), AC mode, 4 x sampling to reduce noise  
  SConfig.samplefreq = 8192;
  SConfig.pin = micPin;
  SConfig.mode = SMODE_AC;
  SConfig.multisample = 4;
  Sampler.setConfig(SConfig);

  // configure the analyzer
  Analyzer PConfig = Processor.defaultConfig();

  // 8192 Hz, 512 bytes FFT, MAx466 mic = 5.012 mvs, gain approx 75 DbSPL (needs calibration)
  // 13 MFCC's,  6 ranges for Shamzam signatures
  PConfig.samplefreq = 8192;
  PConfig.gain = 75;
  PConfig.sensitivity = 5.012;
  Processor.setConfig(Pconfig);

  Sampler.Begin();
}

void loop() {

  // We collect more samples than needed for the FFT signal, because standard Decibel measurement
  // requires a specific duration (there is even even an ISO standard for that)
  //
  Sampler.Collect(Samples, 1024);
  
  decibel_t dB = Processor.decibelSPL(Samples,1024);

  // Yin (Pitch) is a timedomain feature
  float Yin = Processor.getYin(Samples, 1024);

  // Now Make a spectrum by processing the samples. True = to remove DC
  // Then create features, MFCC, Signature, depending on what we need
  // either use the local array, or use the cached data in the class
  //  the audiofeature enums are usd to find the feature in the array
  //
  Processor.doFft(Samples,true);
  float * Features  = Processor.getFeatures();
  float * Mfccs     = Processor.getMfcc();
  signature_t * Signature = Processor.getSignature();

  Serial.printf("dBSpl %d, Pitch %0f, Peakfreq %.2f, Crest %.2f, Rolloff %.2f etc etc\n", 
            dB, Processor.Yin, Features[Fpeakfreq], Features[Fcrest], Features[Frolloff]);

  Serial.print("Mfccs: ");
  for (size_t i=0; i<Processor.NumMfccCoeff; i++) {
    Serial.printf("%.2f%s",Processor.Mfccs[i],i<Processor.NumMfccCoeff ",": "");
  }
  Serial.println();
  
  Serial.printf("Signature: ");
  for (size_t i=0; i<Processor.SignatureLen; i++) {
    Serial.printf("%.2f%s",Signature[i],i<Processor.SignatureLen ",": "");
  }
  Serial.printf("\nSignaturehash = %ul\n", Processor.getSignatureHash());


}