//=======================================================================
/** @file  ArduinoGistPlus.h
 *  @brief Implementations of a list of audio analysys features
 *  @author Michiel steltman / derived from the Gist audio library by Adam Stark
 *  @copyright Copyright (C) 2013  Adam Stark, 2023 Michel Steltman
 *
 * This file is part of the 'AdruinoGistPLus audio analysis library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <Arduino.h>
#include <ESP_fft.h>

namespace SoundAnalyzer {
#include <MFCC.h>
#include <Yin.h>
#include <AnalyzerConfig.h>

// Config struct for the SoundAnalyzer class.
// The constructor must take care of setting the defaults
//
struct AnalyzerConfig {
  // FFT vars
  unsigned    samplefreq;
  unsigned    fftlength;

  // per feature-settings. 
  // to turn off a specific feautes, set the relevant coefficient to 0

  // SPL parameters . sensitivity 0 = switch off
  float       sensitivity;
  decibel_t   gain;
  
  float       roloffpercentile;
  //Shazam parameters  numranges 0 = switch off
  unsigned    numranges;
  unsigned    *ranges;
  unsigned    fuzzfactor;

  // Mfcc parameters coeff 0 = switch off
  unsigned    mfcccoeff;

};

// Sound Analyzer class 
//
template <class T> 
class Analyzer  {

public: 
  Analyzer();
  Analyzer(AnalyzerConfig & );
  ~Analyzer();

  AnalyzerConfig &      defaultConfig();      // return the default configuration
  AnalyzerConfig &      getConfig();
  void                  setConfig(AnalyzerConfig & Cfg);  
  
  // separate routine to get this value
  // time domain mesaurement. If numsamples 0, we take the fftlength
  float           rms(const T * Signal, unsigned len=0);
  decibel_t       decibelSPL(const T * Signal, unsigned len=0); 
  float           getPitch(const T * Signal);
  void            doFft(const T * Signal, bool removeDC=true);

  float *         getFeatures(const float * Spectrum = nullptr, unsigned len = 0);
  float *         getMfcc(const float * Spectrum = nullptr, unsigned len = 0);
  signature_t *   getSignature(const float * Spectrum = nullptr, unsigned len = 0);
  hash_t          getSignatureHash(const signature_t * Signature = nullptr);

  float           frequency (unsigned bin )    {  return ( bin * Fr ); }
  float           amplitude (unsigned bin )    {  return FFT_AMP_SCALE_FACTOR * fabs(Bins[bin]) / Config.fftlength;}
  float           amplitude (float mag )       {  return FFT_AMP_SCALE_FACTOR * fabs(mag) / Config.fftlength;}

  // features and output
  float           *Bins;     // pointer to output
  size_t          NumBins;
  float           Fr;
  // MFCC
  float           *Mfccs;
  size_t          NumMfccCoeff;
  // Shazam
  signature_t     *Signature;
  size_t          SignatureLen;
  hash_t          SignatureHash;
  
  // cached Freq domain features
  // enum is index
  const size_t    NumFeatures = ANALYZER_NUMFEATURES; 
  float           Features[ANALYZER_NUMFEATURES];

private:

  bool            Begin();
  void            End();

  
// hashing   djb2 http://www.cse.yorku.ca/~oz/hash.html
// with fuzz factor in Hz  . we start with size_t is index+1
constexpr hash_t signatureHash(const signature_t sig[], const size_t nr, const size_t off = 0) {
    return off == nr ? 5381 : (signatureHash(sig, nr, off+1)*33) ^ (sig[off] - (sig[off] % Config.fuzzfactor));
}

constexpr float scaleMag(float mag, float peak) { 
  return mag * ANALYZER_DEFAULT_FFTSCALE / peak; 
}

  // configuration parameters
  // defaults are set in constructor
  AnalyzerConfig        Config;

  // local class vars
  bool            initialized = false;
  
  // local buffers. 
  // signal is needed for copy, because the hamming window  function alters out input
  float           *signal   = nullptr;
  float           *spectrum = nullptr;

  ESP_fft         *FFT        = nullptr;

  MFCC            *mfcc       = nullptr;
  YIN             *yin        = nullptr;

};
// instantiate for float and short int
template class Analyzer<int>;
template class Analyzer<int16_t>;
template class Analyzer<float>;

} // namespace