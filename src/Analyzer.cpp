//=======================================================================
/** @file  ArduinoGistPlus.cpp
 *  @brief Implementations of a list of audio analysys features
 *  @author Michiel steltman / derived from the Gist audio library by Adam Stark
 *  @copyright Copyright (C) 2013  Adam Stark, 2023 Michel Steltman
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
#include "SoundAnalyzer.h"

namespace SoundAnalyzer {

const char * FeatureNames[ANALYZER_NUMFEATURES] = FEATURENAMES;

unsigned DefaultRanges[ANALYZER_DEFAULT_NUMRANGES] = ANALYZER_DEFAULT_RANGES_256;
//
template <class T>
Analyzer<T>::Analyzer()
{
  AnalyzerConfig & Def = defaultConfig();
  setConfig(Def);
}

// with configuration
template <class T>
Analyzer<T>::Analyzer(AnalyzerConfig & Config)
{
  setConfig(Config);
}

// make & return default config
template <class T>
AnalyzerConfig & Analyzer<T>::defaultConfig() 
{
  static AnalyzerConfig DefaultConfig {
    // set defaults in Config.
    // per feature-settings.
    // FFT vars
    .samplefreq  = ANALYZER_DEFAULT_SAMPLEFREQ,
    .fftlength   = ANALYZER_DEFAULT_FFTLENGTH,

    // to turn off a specific feautes, set the relevant coefficient to 0
    // SPL parameters . sensitivity 0 = switch off. 
    // Defaults are  ANALYZER_DEFAULT_MICSENS and ANALYZER_DEFAULT_GAIN
    .sensitivity = ANALYZER_DEFAULT_MICSENS,
    .gain        = ANALYZER_DEFAULT_GAIN,
    
    .roloffpercentile = ANALYZER_DEFAULT_ROLLOFF_PERCENTILE,

    //Shazam parameters  numranges 0 = switch off
    .numranges   = ANALYZER_DEFAULT_NUMRANGES,
    .ranges      = DefaultRanges,
    .fuzzfactor  = ANALYZER_DEFAULT_FUZZFACTOR,

    // Mfcc parameters coeff 0 = switch off, default = 13
    .mfcccoeff   = ANALYZER_DEFAULT_MFCC_COEFF,
    
  };

  return DefaultConfig;
}

// Set (new) config values 
template <class T>
void Analyzer<T>::setConfig(AnalyzerConfig & newCfg) 
{
  // resize and re-init if relevant parameters have changed
  if (initialized) {
    if (newCfg.fftlength != Config.fftlength ||  newCfg.samplefreq != Config.samplefreq ||
        newCfg.numranges != Config.numranges ||  newCfg.mfcccoeff != Config.mfcccoeff) 
    {
      End();
    }
  }

  Config = newCfg;

  Fr = (float)Config.samplefreq/Config.fftlength;
  NumBins = Config.fftlength/2;
  SignatureLen = Config.numranges;
  NumMfccCoeff = Config.mfcccoeff;

  // CHeck shazam config: if we still have the default range but another fftlength,
  // that won;t work. So help the caller: create an adapted list
  if (Config.fftlength != ANALYZER_DEFAULT_FFTLENGTH && Config.ranges == DefaultRanges) {
    
    for (unsigned i=0; i<Config.numranges; i++)  {
      unsigned int r = (unsigned )Config.ranges[i];
      r = r * Config.fftlength/ANALYZER_DEFAULT_FFTLENGTH;
      Config.ranges[i] = r;
    }
    Config.fuzzfactor = ANALYZER_DEFAULT_FUZZFACTOR * Config.fftlength / ANALYZER_DEFAULT_FFTLENGTH;
  }

  Begin();
}

// Return currenr configuration
template <class T>
AnalyzerConfig & Analyzer<T>::getConfig() 
{
  static AnalyzerConfig cfg = Config;
  return cfg;
}

template <class T>
Analyzer<T>::~Analyzer() 
{
   End();
}

// Allocates memory on demand, else error may occur before init 
//
template <class T>
bool Analyzer<T>::Begin() 
{
  bool memok = true;

  // shortcuts to keep code readable
  size_t  fftlength = Config.fftlength;
  size_t  samplefreq = Config.samplefreq;
  size_t  mfcccoeff = Config.mfcccoeff;
  size_t  numranges = Config.numranges;

  if (initialized)  return true;

  Fr = (float)samplefreq/fftlength;
  NumBins = fftlength/2;

  // we could make this  a template class, but this will probably be executed only
  // once in the ESP's lifetime, so no big deal.
  // 
  signal   = new float[fftlength];
  spectrum  = new float[fftlength];
  FFT = new ESP_fft (fftlength, samplefreq, FFT_REAL, FFT_FORWARD, signal, spectrum);
  
  memok = (signal || spectrum || FFT); 

  // log_i("EANALYZER_DEFAULT_fft samplefrequency %d, length %d, bins %d, fr %.1f", samplefreq,fftlength,NumBins,Fr);

  // shortcut for external use
  Bins = spectrum;

  if (mfcccoeff > 0 && memok)
  {
    mfcc = new MFCC(fftlength,samplefreq,mfcccoeff);
    Mfccs = mfcc->MFCCs;

    memok = mfcc;
  }

  // a signature is the data between ranges, so 1 less 
  if (numranges > 0 && memok)
  {
    Signature = new signature_t[numranges]; 
    memok = Signature;
  }

  if (memok)
  {
    yin = new YIN(samplefreq,fftlength); 
    memok = yin ;
  }

  if (!memok ) 
  {
    log_e("Can't allocate memory for soundAnalyzer");
    return false;
  }

  initialized = true;
  return true;
}

// free up resources
template <class T>
void Analyzer<T>::End() {

    if (signal)     { delete[] signal;      signal = nullptr; }
    if (spectrum)   { delete[] spectrum;    spectrum = nullptr ; }
    if (Signature)  { delete[] Signature;   Signature = nullptr ;}  
    if (FFT)        { delete FFT;           FFT = nullptr ;}
    if (mfcc)       { delete mfcc;          mfcc = nullptr ;}
    if (yin)        { delete yin;           yin = nullptr; }

    initialized = false;
}

// Calc RMS of the signal, rule out any DC
//
template <class T>
float Analyzer<T>::rms(const T * Signal, unsigned siglen) {

  unsigned len = (siglen == 0) ? Config.fftlength : siglen;

  double Rms = 0; 
  for (unsigned i = 0; i < len; i++) {
    float amp = fabs((float)Signal[i]) ;
    Rms += sq(amp);
  }    
  Rms /= len;  // mean
  return sqrt(Rms); 
}

// calculate Decibel SPL
// is a time domain measurement.  
// We assume that all DC has been taken away, which is required to get an accurate measurement
// to keep performamce optimal we doin;t check that here.
template <class T>
decibel_t Analyzer<T>::decibelSPL(const T * Signal, unsigned siglen) {
    
    double vRms = 0;
    vRms = rms(Signal,siglen);
     
//  Now vRms / mic sensitivity to calculate sound Db value
//  https://electronics.stackexchange.com/questions/96205/how-to-convert-volts-to-db-spl
//  https://electronics.stackexchange.com/questions/375869/to-convert-volts-in-db-spl-confusion
//  calibration = the 4466MAX gain. somewhere between 25 and 125 db, Params.ampGain
//  
//  Sound at xdBSPL, is x-94 dB(1Pa). This becomes x-94+M dBV (or x-94-46 dBv in this case) at the output of the microphone. 
//  At the output of the amplifier, it's x-94+M+G dBV (x-94-46+G dBV).
//
//  If you have a measured voltage of Y dBV, then your original SPL must have been 
//  Y-G-M+94 dBSPL (Y-G-(-46)+94 dBV)  or:  dbv - AmpGain - MicGain + 94
//  (M is a negative number, take care of the sign when subtracting negative numbers!)
//  OR : Y-G+94, if we measure DbV as vrms / micsens ( total gain )
//

  double dBv    = 20 * log10(vRms/Config.sensitivity);
  double dB     = (dBv - (double)Config.gain) + 94;
  
  return (decibel_t) round(dB);
}

template <class T>
void Analyzer<T>::doFft(const T * Signal,bool removeDC)
{
    // copy samples to locall, because Hamming alters our data
    for (unsigned i=0; i < Config.fftlength ; i++) {
      signal[i] = (float)(Signal[i]); 
    }
    // now do FFT
    FFT->hammingWindow();
    if (removeDC) FFT->removeDC();
    FFT->execute();
    FFT->complexToMagnitude();
    // store peak and mag
    Features[Fpeakfreq] = FFT->majorPeakFreq();
    Features[Fpeakmag] = FFT->majorPeak();

    // remove any negative DC; important otherwise make features fail
    if (removeDC) spectrum[0] = 0;
}

//
// https://www.toptal.com/algorithms/shazam-it-music-processing-fingerprinting-and-recognition
// NUMRANGES ranges, find highest peak in each range and keep the frequency of that peak (not the mag)
// https://medium.com/neuronio/a-little-about-how-shazam-works-8b64caa5b6f
// http://coding-geek.com/how-shazam-works/
// scale values to peak of frequencies in our ranges
//
template <class T>
signature_t * Analyzer<T>::getSignature(const float * Spectrum, unsigned len)
{
  const float * bins;
  unsigned NumBins;
  unsigned numranges = Config.numranges;  // readability
  unsigned *ranges =  Config.ranges;
  
  if (! numranges) return 0;
  // Use parameter or existing data?
  bins = (Spectrum != nullptr) ? Spectrum : spectrum;
  NumBins = (len == 0) ? this->NumBins : len;

  float mags[numranges];
  for (unsigned i=0; i<numranges; i++ )
  {
    Signature[i] = 0;
    mags[i] = 0;
  }

  // lambda function to find index of this freq
  auto rangeIndex = [&](unsigned idx) {
    for (unsigned r=0; r < numranges; r++) {
      if (ranges[r] > idx) return r;
    }
    // protect against a config error (last entry forgotten )
    return numranges-1;
  };
  
  float mag,avg;
  unsigned i;
  // Find peak value of freqs in our profile .
  for (i=1; i < NumBins ; i++) {
    int r = rangeIndex(i); // find out in which range we are with this freq band
      // find the mag. for this frequency
      mag = log(fabs(bins[i]) +1);   
    // keep the actual frequency of peak value in each range
    if (mag > mags[r]) {
      Signature[r] = (signature_t) int(frequency(i));
      mags[r] = mag;
    }
  }
  //now zero all indexs with amplitudes below average
  for (i=0, avg=0; i<numranges; i++) avg += mags[i];
  avg = (avg * 1.0) /i;
  for (i=0; i<numranges; i++) 
    if (mags[i] < avg) Signature[i] = 0;
    
  return Signature;
}

// return a hash of the signature.
template <class T>
hash_t Analyzer<T>::getSignatureHash(const signature_t * Sig)
{
  const signature_t *S = (Sig == nullptr) ? Signature : Sig;
  return signatureHash(S, SignatureLen);
}

//
// calculate all characteritics in one go
// peakfreq,peakmag,avgmag,spread,skewness,centroid,flatness,crest,kurtosis,rolloff
//
template <class T>
float * Analyzer<T>::getFeatures(const float * Spectrum, unsigned len)
{
  const float * bins;
  unsigned NumBins;

    // Use parameters or existing data?
  bins = (Spectrum != nullptr) ? Spectrum : spectrum;
  NumBins = (len == 0) ? this->NumBins : len;

  float sumAmplitudes = 0.0;
  float sumWeightedAmplitudes = 0.0;

  float peakFreq = 0.0;
  float peakMag = 0.0;
  float maxfVal = 0.0;

  float meanVal;
  float meancVal =0.0;
  
  // flatness
  double sumfVal = 0.0;
  double logSumfVal = 0.0;

  // crest
  float sumcVal = 0.0;
  float maxcVal = 0.0;

  for (unsigned i = 1; i < NumBins; i++)
  {
    float Mag = bins[i];

    // centroid & roloff
    sumAmplitudes += Mag;
    sumWeightedAmplitudes += Mag * i;
    
    // flatness
    double f = 1 + Mag;
    logSumfVal += log (f);
    sumfVal += f;

    //crest
    float c = sq(Mag);
    sumcVal += c;
    if (c > maxcVal) maxcVal = c;

    if (Mag > peakMag) {
      peakMag = Mag;
      peakFreq = frequency(i);
    }
  }
  
  sumfVal = sumfVal / (double)NumBins;
  logSumfVal = logSumfVal / (double)NumBins;
  meanVal = sumAmplitudes / (double)NumBins;
  meancVal = sumcVal / (double)NumBins;

  Features[Fpeakfreq] = (float) peakFreq;
  Features[Fpeakmag]  = (float) peakMag;
  Features[Favgmag]   = (float) meanVal;
  
  float centroid = sumAmplitudes > 0 ? sumWeightedAmplitudes / sumAmplitudes : 0.0;
  Features[Fcentroid] = (float) centroid;
  Features[Fflatness] = (float) (sumfVal > 0 ? exp (logSumfVal) / sumfVal : 0.0);
  Features[Fcrest]    = (float) (sumcVal > 0 ? maxcVal / meancVal : 1.0);

  // for kurtosis
  float moment2 = 0;
  float moment4 = 0;
  // for skewness
  float spread_sum = 0.0f;
  float skewness_sum = 0.0f;
  
  float rolloff = 0;
  float roloff_sum = 0.0f;
  float rolloff_threshold = Config.roloffpercentile * sumAmplitudes;

  for (unsigned i = 1; i < NumBins; i++) {
    float Mag = bins[i];
    spread_sum += pow(i - centroid,2) * Mag;
    skewness_sum += pow((i - centroid),3) * Mag;
    
    // find percentile of power Vs entire power
    if (rolloff == 0) {
      if (roloff_sum > rolloff_threshold) {
        rolloff = (double)i/(double)NumBins;
      } else {
        roloff_sum += Mag;
      }
    }

    float difference = Mag - meanVal;
    float squaredDifference = sq(difference);
      
    moment2 += squaredDifference;
    moment4 += sq(squaredDifference);
  }
  Features[Frolloff] = rolloff;

  float spread = sqrt(spread_sum / sumAmplitudes); // = weighted std. deviation
  Features[Fspread] = (float) spread;
  Features[Fskewness] = (float) ((skewness_sum / sumAmplitudes) / pow(spread,3));
  
  moment2 = moment2 / NumBins;
  moment4 = moment4 / NumBins;    
  Features[Fkurtosis] = (moment2 == 0 ? -3 : (moment4 / sq(moment2)) - 3.0);

  return Features;

}
// Make cepstrals. Make sure firstbin is empty
template <class T>
float * Analyzer<T>::getMfcc(const float * Spectrum, unsigned len)
{
  const float * bins;
  unsigned NumBins;

  // Use parameter or existing data?
  bins = (Spectrum != nullptr) ? Spectrum : spectrum;
  NumBins = (len == 0) ? this->NumBins : len;

  if (Config.mfcccoeff > 0 ) {
    mfcc->calculateMelFrequencyCepstralCoefficients (bins);
    Mfccs = mfcc->MFCCs;
    return Mfccs;
  }  else 
    return nullptr;
}

// Make yin , takes float
//
template <class T>
float  Analyzer<T>::getPitch(const T * Signal)
{
  for (unsigned i=0; i<Config.fftlength; i++) signal[i] = (float)Signal[i];
  return yin->pitchYin(signal);
}

}
