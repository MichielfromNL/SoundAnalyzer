//=======================================================================
/** @file Yin.h
 *  @brief Calculates Yin (pitch) coefficient for a signal
 *  @author Adam Stark, Gist
 *  @copyright
 *
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

  //===========================================================
  /** template class for the pitch detection algorithm Yin.
   * Instantiations of the class should be of either 'float' or
   * 'double' types and no others */
  class YIN
  {

  public:
    //===========================================================
    /** constructor
     * @param samplingFrequency the sampling frequency
     */
    YIN(size_t samplingFrequency, size_t len) : fs(samplingFrequency), framesz(len) 
    {
      prevPeriodEstimate = 1.0;
      setMaxFrequency(1500);
      delta = new float[framesz/2];
    }

    ~YIN() {
      delete[] delta;
    }

    void setMaxFrequency(float maxFreq)
    {
      float minPeriodFloating;

      // if maxFrequency is zero or less than 200Hz, assume a bug
      // and set it to an arbitrary value fo 2000Hz
      if (maxFreq <= 200)
        maxFreq = 2000.;

      minPeriodFloating = ((float)fs) / maxFreq;
      minPeriod = (int)ceil(minPeriodFloating);
    }

    //===========================================================
    /** @returns the maximum frequency that the algorithm will return */
    float getMaxFrequency()
    {
      return ((float)fs) / ((float)minPeriod);
    }

    //===========================================================
    /** calculates the pitch of the audio frame passed to it
     * @param frame an audio frame stored in a vector
     * @returns the estimated pitch in Hz
     */
    float pitchYin(const float *frame)
    {
      unsigned long period;
      float fPeriod;

      // steps 1, 2 and 3 of the Yin algorithm
      // get the difference function ("delta")
      cumulativeMeanNormalisedDifferenceFunction(frame);

      // first, see if the previous period estimate has a minima
      long continuityPeriod = searchForOtherRecentMinima(delta);

      // if there is no minima at the previous period estimate
      if (continuityPeriod == -1)
      {
        // then estimate the period from the function
        period = getPeriodCandidate(delta);
      }
      else // if there was a minima at the previous period estimate
      {
        // go with that
        period = (unsigned long)continuityPeriod;
      }

      // check that we can interpolate (i.e. that period isn't first or last element)
      if ((period > 0) && (period < ((framesz/2) - 1)))
      {
        // parabolic interpolation
        fPeriod = parabolicInterpolation(period, delta[period - 1], delta[period], delta[period + 1]);
      }
      else // if no interpolation is possible
      {
        // just use the period "as is"
        fPeriod = (float)period;
      }

      // store the previous period estimate for later
      prevPeriodEstimate = fPeriod;

      return periodToPitch(fPeriod);
    }

  private:
    //===========================================================
    /** converts periods to pitch in Hz
     * @param period the period in audio samples
     * @returns the pitch in Hz
     */
    float periodToPitch(float period)
    {
      return ((float)fs) / period;
    }

    /** this method searches the previous period estimate for a
     * minimum and if it finds one, it is used, for the sake of consistency,
     * even if it is not the optimal choice
     * @param delta the cumulative mean normalised difference function
     * @returns the period found if a minimum is found, or -1 if not
     */
    long searchForOtherRecentMinima(const float *delta)
    {
      long newMinima = -1;

      long prevEst;

      prevEst = (long)round(prevPeriodEstimate);

      for (long i = prevEst - 1; i <= prevEst + 1; i++)
      {
        if ((i > 0) && (i < static_cast<long>((framesz/2)) - 1))
        {
          if ((delta[i] < delta[i - 1]) && (delta[i] < delta[i + 1]))
          {
            newMinima = i;
          }
        }
      }

      return newMinima;
    }

    /** interpolates a period estimate using parabolic interpolation
     * @param period the period estimate
     * @param y1 the value of the cumulative mean normalised difference function at (period-1)
     * @param y2 the value of the cumulative mean normalised difference function at (period)
     * @param y3 the value of the cumulative mean normalised difference function at (period+1)
     * @returns the interpolated period
     */
    float parabolicInterpolation(unsigned long period, float y1, float y2, float y3)
    {
      // if all elements are the same, our interpolation algorithm
      // will end up with a divide-by-zero, so just return the original
      // period without interpolation
      if ((y3 == y2) && (y2 == y1))
      {
        return (float)period;
      }
      else
      {
        float newPeriod = ((float)period) + (y3 - y1) / (2. * (2 * y2 - y3 - y1));
        return newPeriod;
      }
    }

    /** calculates the period candidate from the cumulative mean normalised difference function
     * @param delta the cumulative mean normalised difference function
     * @returns the period estimate
     */
    unsigned long getPeriodCandidate(const float * delta)
    {
      unsigned long minPeriod = 30;
      unsigned long period;

      float thresh = 0.1;

      bool periodCandidateFound = false;

      float minVal = 100000;
      unsigned long minInd = 0;

      for (unsigned long i = minPeriod; i < ((framesz/2) - 1); i++)
      {
        if (delta[i] < minVal)
        {
          minVal = delta[i];
          minInd = i;
        }

        if (delta[i] < thresh)
        {
          if ((delta[i] < delta[i - 1]) && (delta[i] < delta[i + 1]))
          {
            // we have found a minimum below the threshold, and because we
            // look for them in order, this is the first one, so we accept it
            // as the candidate period (i.e. the minimum period), and break the loop
            period = i;
            periodCandidateFound = true;
            break;
          }
        }
      }

      if (!periodCandidateFound)
      {
        period = minInd;
      }

      return period;
    }

    /** this calculates steps 1, 2 and 3 of the Yin algorithm as set out in
     * the paper (de Cheveigné and Kawahara,2002).
     * @param frame a vector containing the audio frame to be procesed
     */
    void cumulativeMeanNormalisedDifferenceFunction(const float *frame)
    {
      float cumulativeSum = 0.0;
      size_t L = framesz / 2;

      float *deltaPointer = &delta[0];

      // for each time lag tau
      for (unsigned long tau = 0; tau < L; tau++)
      {
        *deltaPointer = 0.0;

        // sum all squared differences for all samples up to half way through
        // the frame between the sample and the sample 'tau' samples away
        for (unsigned long j = 0; j < L; j++)
        {
          float diff = frame[j] - frame[j + tau];
          *deltaPointer += (diff * diff);
        }

        // calculate the cumulative sum of tau values to date
        cumulativeSum = cumulativeSum + delta[tau];

        if (cumulativeSum > 0)
          *deltaPointer = *deltaPointer * tau / cumulativeSum;

        deltaPointer++;
      }

      // set the first element to zero
      delta[0] = 1.;
    }

    // float round (T val)
    // {
    // 	return floor(val + 0.5);
    // }

    /** the previous period estimate found by the algorithm last time it was called - initially set to 1.0 */
    float prevPeriodEstimate;

    /** the sampling frequency */
    int fs;
    /** the size of the input frame (we can't use vector here) **/
    size_t framesz;
    /** the minimum period the algorithm will look for. this is set indirectly by setMaxFrequency() */
    int minPeriod;
    // the buffer
    float      *delta;

  };
