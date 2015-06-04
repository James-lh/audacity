#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <iostream>
#include <complex>
#include <algorithm>
#include <functional>
#include <vector>
#include <stdexcept>
#include <inttypes.h>
#include <fenv.h>

#include <wx/defs.h>
#include <wx/wxchar.h>

#include "../FFT.h"
#include "HPSS-core.h"
#include "HPSS.h"

HPSSCore::HPSSCore(): numIterations(1), blockSize(60), sigmaP(0.5), sigmaH(0.5) {
}

// =================================================================
//  == Audacity-FFT (FFT size: "frameSize")
// ==============================
std::vector<std::complex<HPSSFloat> > HPSSCore::FFT_R2C_Audacity(const std::vector<HPSSFloat>& input) {
   float* realIn = new float[frameSize]; float* imagIn = new float[frameSize];
   float* realOut = new float[frameSize]; float* imagOut = new float[frameSize];
   for (uint32_t i = 0; i < frameSize; i++) { realIn[i] = input[i]; imagIn[i] = 0; }
   FFT(frameSize, false, realIn, imagIn, realOut, imagOut); // Possible optimization: use real-to-complex FFT
   std::vector<std::complex<HPSSFloat> > output; output.reserve(frameSize); // WARNING FFT uses float...
   for (uint32_t i = 0; i < frameSize; i++) { output.push_back(std::complex<HPSSFloat>(realOut[i], imagOut[i])); }
   delete[] realIn; delete[] imagIn; delete[] realOut; delete[] imagOut;
   return output;
}

std::vector<HPSSFloat> HPSSCore::IFFT_C2R_Audacity(const std::vector<std::complex<HPSSFloat> >& input) {
   float* realIn = new float[frameSize]; float* imagIn = new float[frameSize];
   float* realOut = new float[frameSize]; float* imagOut = new float[frameSize];
   for (uint32_t i = 0; i < frameSize; i++) { realIn[i] = input[i].real(); imagIn[i] = input[i].imag(); }
   FFT(frameSize, true, realIn, imagIn, realOut, imagOut); // Normalization is within FFT()  // Possible optimization: use complex-to-real FFT
   std::vector<HPSSFloat> output; output.reserve(frameSize); // WARNING FFT uses float...
   for (uint32_t i = 0; i < frameSize; i++) { output.push_back(realOut[i]); }
   delete[] realIn; delete[] imagIn; delete[] realOut; delete[] imagOut;
   return output;
}

// =================================================================
//  == HPSS auxiliary functions
// ==============================
HPSSFloat HPSSCore::ConsumeOneSample(SignalStream& sourceStream) {
   if (sourceStream.empty()) {
      return 0;
   } else {
      HPSSFloat result = sourceStream.front();
      sourceStream.pop();
      return result;
   }
}

FrameInfo HPSSCore::CalcNewFrame(std::list<HPSSFloat>& inputBufferState, SignalStream& inputSignalStream) {
   for (uint32_t i = 0; i < shift; i++) { // inputBufferState length = frameSize
      inputBufferState.push_back(ConsumeOneSample(inputSignalStream));
      inputBufferState.pop_front();
   }
   std::vector<HPSSFloat> wbuf(frameSize, 0);
   std::transform(inputBufferState.begin(), inputBufferState.end(), analysisWindow.begin(), wbuf.begin(), std::multiplies<HPSSFloat>());
   std::vector<std::complex<HPSSFloat> > fftResult = FFT_R2C_Audacity(wbuf);
   FrameInfo newFrameInfo(frameSize / 2 + 1); // used up to index frameSize / 2
   for (uint32_t h = 1; h < frameSize / 2; h++) {
      newFrameInfo.amplitudes[h] = abs(fftResult[h]);
      newFrameInfo.cosinePhases[h] = newFrameInfo.amplitudes[h] == 0 ? 0 : (fftResult[h].real() / newFrameInfo.amplitudes[h]);
      newFrameInfo.sinePhases[h] = newFrameInfo.amplitudes[h] == 0 ? 0 : (fftResult[h].imag() / newFrameInfo.amplitudes[h]);
      newFrameInfo.harmonicComponent[h] = newFrameInfo.amplitudes[h] / sqrt(2);
      newFrameInfo.percussiveComponent[h] = newFrameInfo.amplitudes[h] / sqrt(2);
   }
   return newFrameInfo;
}

void HPSSCore::SlidingBlockProcess(const FrameInfo& incomingFrame, std::list<FrameInfo>& slidingWindow,
                  std::vector<std::complex<HPSSFloat> >& outHarmonic, std::vector<std::complex<HPSSFloat> >& outPercussive) {
   slidingWindow.pop_front();
   slidingWindow.push_back(incomingFrame);
   
   // separation
   std::list<FrameInfo>::iterator itOldestFrame = slidingWindow.begin();
   std::list<FrameInfo>::iterator itNewestFrame = --slidingWindow.end();
   for (uint32_t k = 0; k < numIterations; k++) {   
      for (std::list<FrameInfo>::iterator it = slidingWindow.begin(); it != slidingWindow.end(); it++) {
         if (it == itOldestFrame || it == itNewestFrame) continue;
         for (uint32_t h = 1; h < frameSize / 2; h++) {
            HPSSFloat c_p, c_h;
            HPSSFloat ww = sqr(it->amplitudes[h]);
            if (sqr(it->harmonicComponent[h]) + sqr(it->percussiveComponent[h]) > 0) {
               c_h = sqr(it->harmonicComponent[h]) / (sqr(it->harmonicComponent[h]) + sqr(it->percussiveComponent[h])) * 2 * ww;
               c_p = 2 * ww - c_h;
            } else {
               c_h = c_p = ww;
            }
            std::list<FrameInfo>::const_iterator itPreviousFrame = --std::list<FrameInfo>::const_iterator(it);
            std::list<FrameInfo>::const_iterator itNextFrame = ++std::list<FrameInfo>::const_iterator(it);
            HPSSFloat b = (itPreviousFrame->harmonicComponent[h] + itNextFrame->harmonicComponent[h]) * wH[h];
            it->harmonicComponent[h] = b + sqrt(sqr(b) + c_h * (1 - 4 * wH[h]));
            b = (it->percussiveComponent[h - 1] + it->percussiveComponent[h + 1]) * wP[h]; // TODO h-1 can be 0 (FFT DC? is that OK?)
            it->percussiveComponent[h] = b + sqrt(sqr(b) + c_p * (1 - 4 * wP[h]));
         }
      }
   }
   
   for (uint32_t h = 1; h < frameSize / 2; h++) {
      switch (maskType) {
         case BinaryMask: {
            if (itOldestFrame->harmonicComponent[h] > itOldestFrame->percussiveComponent[h]) {
               itOldestFrame->harmonicComponent[h] = itOldestFrame->amplitudes[h];
               itOldestFrame->percussiveComponent[h] = 0;
            } else{
               itOldestFrame->percussiveComponent[h] = itOldestFrame->amplitudes[h];
               itOldestFrame->harmonicComponent[h] = 0;
            }
         } break;
         case WienerMask: {
            HPSSFloat ratio; 
            if (sqr(itOldestFrame->harmonicComponent[h]) + sqr(itOldestFrame->percussiveComponent[h]) == 0) {
               ratio = 0.5;
            } else {
               ratio = sqr(itOldestFrame->harmonicComponent[h]) /
                       (sqr(itOldestFrame->harmonicComponent[h]) + sqr(itOldestFrame->percussiveComponent[h]));
            }
            itOldestFrame->harmonicComponent[h] = ratio * itOldestFrame->amplitudes[h];
            itOldestFrame->percussiveComponent[h] = (1 - ratio) * itOldestFrame->amplitudes[h];
         } break;
         default: throw std::logic_error("Invalid mask type");
      }
      outHarmonic[h] = std::complex<HPSSFloat>(itOldestFrame->harmonicComponent[h] * itOldestFrame->cosinePhases[h],
                                               itOldestFrame->harmonicComponent[h] * itOldestFrame->sinePhases[h]);
      outPercussive[h] = std::complex<HPSSFloat>(itOldestFrame->percussiveComponent[h] * itOldestFrame->cosinePhases[h],
                                                 itOldestFrame->percussiveComponent[h] * itOldestFrame->sinePhases[h]);
   }
   slidingBlockProcessRunCount++;
}

void HPSSCore::CalcOutput(std::list<HPSSFloat>& outputBufferState, SignalStream& outputSignal,
                          const std::vector<std::complex<HPSSFloat> >& processedFrame){
   std::vector<HPSSFloat> abuf = IFFT_C2R_Audacity(processedFrame);
   uint32_t i = 0;
   for (std::list<HPSSFloat>::iterator it = outputBufferState.begin(); it != outputBufferState.end(); it++, i++) { // 'frameSize' iterations
      *it += synthesisWindow[i] * abuf[i] * 2;
   }
   for (uint32_t i = 0; i < shift; i++) {
      if (slidingBlockProcessRunCount >= blockSize + 1) { // TODO why +1?
         if (outputBufferState.front() != outputBufferState.front()) {
            std::cout << "WARNING: outputting NAN (position " << outputSignal.size() << ")" << std::endl;
         }
         outputSignal.push(outputBufferState.front() * finalMultiplier); // output sample (queue transfer)
      }
      outputBufferState.pop_front(); // consume sample
      outputBufferState.push_back(0); // add one '0' back to the queue
   }
}

// =======================================================================
//  == HPSS MAIN API
// ========================
void HPSSCore::executeHPSS(uint32_t frameSize, MaskType maskType, float finalMultiplier,
                           SignalStream& inputSignal, SignalStream& outputSignalH, SignalStream& outputSignalP,
                           ProgressInfo& progressInfo, EffectBaseHPSS* hpsHandle) {
   //feenableexcept(FE_INVALID | FE_OVERFLOW); // TODO floating-point debugging only (Linux only)
   this->frameSize = frameSize;
   this->maskType = maskType;
   this->finalMultiplier = finalMultiplier;
   shift = frameSize/2;

   // memory allocation 
   std::list<FrameInfo> slidingWindow(blockSize, FrameInfo(frameSize / 2 + 1)); // used up to index frameSize / 2
   std::list<HPSSFloat> inputBufferState(frameSize, 0);
   std::vector<std::complex<HPSSFloat> > harmonicFrame(frameSize, 0);
   std::vector<std::complex<HPSSFloat> > percussiveFrame(frameSize, 0);
   std::list<HPSSFloat> outputBufferStateH(frameSize, 0);
   std::list<HPSSFloat> outputBufferStateP(frameSize, 0);
   
   // Set up Hamming window arrays
   analysisWindow.clear(); analysisWindow.reserve(frameSize);
   synthesisWindow.clear(); synthesisWindow.reserve(frameSize);
   wH.clear(); wH.reserve(frameSize);
   wP.clear(); wP.reserve(frameSize);
   for (uint32_t i = 0; i < frameSize; i++) {
      analysisWindow.push_back(sqrt(0.54 - 0.46 * cos(2.0 * M_PI * i / frameSize)));
      synthesisWindow.push_back(sqrt(0.54 - 0.46 * cos(2.0 * M_PI * i / frameSize)) / 1.08);
      wH.push_back(0.25 / (sqr(sigmaH) + 1));
      wP.push_back(0.25 / (sqr(sigmaP) + 1));
   }

   inputSignalLength = inputSignal.size(); // Store the original length because we will consume the signal
   std::cout << "Processing signal (" << inputSignalLength << " samples)" << std::endl;
   std::cout << "Frame size: " << frameSize << " // Shift: " << shift << " // Mask type: " << maskType << std::endl;
   
   // main process
   slidingBlockProcessRunCount = 0;
   while (outputSignalH.size() < inputSignalLength) {
      const FrameInfo newFrame = CalcNewFrame(inputBufferState, inputSignal);
      SlidingBlockProcess(newFrame, slidingWindow, harmonicFrame, percussiveFrame);
      CalcOutput(outputBufferStateH, outputSignalH, harmonicFrame);
      CalcOutput(outputBufferStateP, outputSignalP, percussiveFrame);
      progressInfo.localProgress = (float)outputSignalH.size()/(float)inputSignalLength;
      hpsHandle->ReportProgress(progressInfo);
   }
   //fedisableexcept(FE_INVALID | FE_OVERFLOW); // TODO floating-point debugging only (Linux only)
}

void HPSSCore::executeHPSSVocalRemoval(uint32_t shortFrameSize, uint32_t longFrameSize, MaskType maskType,
                           float finalMultiplier, SignalStream& inputSignal, SignalStream& outVocal, SignalStream& outRest,
                           int whichTrack, EffectBaseHPSS* hpsHandle) {
   SignalStream outIntermediate, outReallyPercussive, outReallyHarmonic;
   ProgressInfo progressInfo = ProgressInfo(whichTrack, 0.0f, 0.4f);
   executeHPSS(shortFrameSize, maskType, finalMultiplier, inputSignal, outIntermediate, outReallyPercussive, progressInfo, hpsHandle);
   progressInfo = ProgressInfo(whichTrack, 0.4f, 1.0f);
   executeHPSS(longFrameSize, maskType, finalMultiplier, outIntermediate, outReallyHarmonic, outVocal, progressInfo, hpsHandle);
   std::cout << "outReallyHarmonic length: " << outReallyHarmonic.size() << std::endl;
   std::cout << "outReallyPercussive length: " << outReallyPercussive.size() << std::endl;
   while (!outReallyHarmonic.empty() || !outReallyPercussive.empty()) {
      outRest.push(ConsumeOneSample(outReallyHarmonic) + ConsumeOneSample(outReallyPercussive));
   }
}