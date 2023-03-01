//=======================================================================
/** @file MFCC.h
 *  @brief Calculates Mel Frequency Cepstral Coefficients
 *  @author Adam Stark
 *  @copyright Copyright (C) 2014  Adam Stark
 *
 * This file is derived from the 'Gist' audio analysis library
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
//=======================================================================

#include <Arduino.h>
#include <float.h>

//=======================================================================
// class for calculating Mel Frequency Cepstral Coefficients
//
class MFCC
{

public:
    
    //=======================================================================
    /** Constructor */
    // framesize = twice the number of Bins (FFT)
    // 
    MFCC (int frameSize_, size_t samplingFrequency_, size_t numCoefficents_ = 13) :    
            frameSize(frameSize_),samplingFrequency(samplingFrequency_), numCoefficents(numCoefficents_),
            magnitudeSpectrumSize(frameSize/2), minFrequency(0), maxFrequency(samplingFrequency)
    {
        magnitudeSpectrumSize = frameSize/2;
        minFrequency = 0;
        maxFrequency = samplingFrequency / 2;
        // setup data
        
        melSpectrum = new float[numCoefficents];
        MFCCs = new float[numCoefficents];
        dctSignal = new float[numCoefficents];
        filterBank = new float*[numCoefficents];
        for(int i = 0; i < numCoefficents; i++)
            filterBank[i] = new float[magnitudeSpectrumSize];

        calculateMelfilterBank();
    }
    
    // cleanup, reverse order to prevent fragmentation
    ~MFCC()
    {
        for(int i = numCoefficents-1; i>= 0; i--)
            delete[] filterBank[i];
        delete[] filterBank;
        delete[] dctSignal;
        delete[] MFCCs;
        delete[] melSpectrum;
    }
    
    //=======================================================================
    /** Calculates the Mel Frequency Cepstral Coefficients from the magnitude spectrum of a signal. The result
     * is stored in the public vector MFCCs.
     * 
     * Note that the magnitude spectrum passed to the function is not the full mirrored magnitude spectrum, but 
     * only the first half. The frame size passed to the constructor should be twice the length of the magnitude spectrum.
     * @param magnitudeSpectrum the magnitude spectrum in vector format
     */
    void calculateMelFrequencyCepstralCoefficients (const float magnitudeSpectrum[])
    {
        calculateMelFrequencySpectrum (magnitudeSpectrum);
        
        for (size_t i = 0; i <numCoefficents; i++)
            MFCCs[i] = log (melSpectrum[i] + (float)FLT_MIN);

        discreteCosineTransform ();
    }

    /** Calculates the magnitude spectrum on a Mel scale. The result is stored in
     * the public vector melSpectrum.
     */
    void calculateMelFrequencySpectrum (const float magnitudeSpectrum[])
    {
        for (int i = 0; i < numCoefficents; i++)
        {
            double coeff = 0;
            
            for (size_t j = 0; j < magnitudeSpectrumSize; j++)
                coeff += (float)((magnitudeSpectrum[j] * magnitudeSpectrum[j]) * filterBank[i][j]);
            
            melSpectrum[i] = coeff;
        }
    }
    //=======================================================================
    /** a vector to hold the mel spectrum once it has been computed */
    float *melSpectrum;
    
    /** a vector to hold the MFCCs once they have been computed */
    float *MFCCs;
    
    /** the number of MFCCs to calculate */
    int numCoefficents;

private:

    /** Calculates the discrete cosine transform (version 2) of an input signal, performing it in place
     * (i.e. the result is stored in the vector passed to the function)
     *
     */
    void discreteCosineTransform ()
    {
            
        for (size_t i = 0; i < numCoefficents; i++)
            dctSignal[i] = MFCCs[i];
        
        float N = (float)numCoefficents;
        float piOverN = M_PI / N;

        for (size_t k = 0; k < numCoefficents; k++)
        {
        float sum = 0;
        float kVal = (float)k;

            for (size_t n = 0; n < numCoefficents; n++)
            {
                float tmp = piOverN * (((float)n) + 0.5) * kVal;
                sum += dctSignal[n] * cos (tmp);
            }

            MFCCs[k] = 2 * sum;
        }
    }
    /** Calculates the triangular filters used in the algorithm. These will be different depending
     * upon the frame size, sampling frequency and number of coefficients and so should be re-calculated
     * should any of those parameters change.
     */
    void calculateMelfilterBank()
    {
        float centreIndices[numCoefficents+2];

        int maxMel = floor (frequencyToMel (maxFrequency));
        int minMel = floor (frequencyToMel (minFrequency));

        for (int i = 0; i < numCoefficents; i++)
        {
            //filterBank[i].resize (magnitudeSpectrumSize);
            for (int j = 0; j < magnitudeSpectrumSize; j++)
                filterBank[i][j] = 0.0;
        }

        for (int i = 0; i < numCoefficents + 2; i++)
        {
            double f = i * (maxMel - minMel) / (numCoefficents + 1) + minMel;

            double tmp = log (1 + 1000.0 / 700.0) / 1000.0;
            tmp = (exp (f * tmp) - 1) / (samplingFrequency / 2);
            tmp = 0.5 + 700 * ((double)magnitudeSpectrumSize) * tmp;
            tmp = floor (tmp);

            int centreIndex = (int)tmp;
            centreIndices[i] = centreIndex;
        }

        for (int i = 0; i < numCoefficents; i++)
        {
            int filterBeginIndex = centreIndices[i];
            int filterCenterIndex = centreIndices[i + 1];
            int filterEndIndex = centreIndices[i + 2];

        float triangleRangeUp = (float)(filterCenterIndex - filterBeginIndex);
        float triangleRangeDown = (float)(filterEndIndex - filterCenterIndex);

            // upward slope
            for (int k = filterBeginIndex; k < filterCenterIndex; k++)
                filterBank[i][k] = ((float)(k - filterBeginIndex)) / triangleRangeUp;

            // downwards slope
            for (int k = filterCenterIndex; k < filterEndIndex; k++)
                filterBank[i][k] = ((float)(filterEndIndex - k)) / triangleRangeDown;
        }
    }
    /** Calculates mel from frequency
     * @param frequency the frequency in Hz
     * @returns the equivalent mel value
     */
    float frequencyToMel (float frequency)
    {
        return int(1127) * log (1 + (frequency / 700.0));
    }

    /** the sampling frequency in Hz */
    int samplingFrequency;

    /** the audio frame size */
    int frameSize;

    /** the magnitude spectrum size (this will be half the frame size) */
    int magnitudeSpectrumSize;

    /** the minimum frequency to be used in the calculation of MFCCs */
    float minFrequency;

    /** the maximum frequency to be used in the calculation of MFCCs */
    float maxFrequency;

    /** a vector of vectors to hold the values of the triangular filters */
    float ** filterBank;
    // 2D vector filterBank;
    float *dctSignal;
};