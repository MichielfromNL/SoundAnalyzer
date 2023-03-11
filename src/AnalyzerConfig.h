/*
  Define the basic types & constants for the Analyzer classes. 
  used here for readability, so that itis clear what types are used
  The original idea was to have an INT FFT version and use tenplates.
  But I abandoned that idea, too much effort and risk. FFT = float, we have to get used to that
*/
#pragma once

typedef unsigned short  signature_t;
typedef unsigned short  decibel_t;
typedef unsigned long   hash_t;

// Defaults for DecibelSPL
// set for a MAX4466
#define ANALYZER_DEFAULT_GAIN 75          // calibration value for microphone Gain
#define ANALYZER_DEFAULT_MICSENS 5.012    // from datasheet for MAX4466 mic. mvolts 0.005012 = -46dB  for 94 DB SPL

// Defaults for FFT
#define ANALYZER_DEFAULT_SAMPLEFREQ   44100
#define ANALYZER_DEFAULT_FFTLENGTH    512

#define ANALYZER_DEFAULT_ROLLOFF_PERCENTILE 0.85

// Defaults for Shazam . See 
// https://www.mcand.ru/posts/how-shazam-works-part-1/
// the ranges apply to bins. If we have another VVTlebgth, we need to adapt ranges
// accordingly, plus tyhe FUZZfactor: recommended is 2 bands
#define ANALYZER_DEFAULT_FFTSCALE   1000
#define ANALYZER_DEFAULT_FUZZFACTOR 32     // FUZZfactor for fingerprints, in Hz
#define ANALYZER_DEFAULT_NUMRANGES  6 
// default number of  ranges, bins start at 0 / end at 255 etc
// so this range is band 0-5,5-10,10-20,20-40,40-80,80-255
// don;t forget the last one, otherwise index will not work
#define ANALYZER_DEFAULT_RANGES_256  { 5, 10, 20, 40, 80 ,256} 
#define ANALYZER_DEFAULT_RANGES_512  { 10,20, 40, 80,160 ,512} 

// Default For MFCC
#define ANALYZER_DEFAULT_MFCC_COEFF 13

// a factor that roughly applies to our FFT + Hamming.
// to find the amplitude for a given (real) magnitude
// applies only to non-DC bins
// not very accurate, found by trial and error by comparing the rms of DC-free signal and spectrum
// it very much depends on the used FFT  
#define FFT_AMP_SCALE_FACTOR    22.627

// The Spectrum features that the analyzer creates
// strings for convenience, when using Json data collection
enum SpectrumFeature {
  Fpeakfreq=0,Fpeakmag,Favgmag,Fspread,Fskewness,Fcentroid,Fflatness,Fcrest,Fkurtosis,Frolloff,
  ANALYZER_NUMFEATURES
};

#define FEATURENAMES    {"PeakFreq","PeakMag","AvgMag","Spread","Skewness","Centroid","Flatness","Crest","Kurtosis","Rolloff"}

// declared in the cpp file
extern const char * FeatureNames[ANALYZER_NUMFEATURES];
