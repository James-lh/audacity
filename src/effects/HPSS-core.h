#ifndef HPSS_CORE_H
#define HPSS_CORE_H

#include <queue>
#include <list>
#include <complex>
#include <string>
#include <inttypes.h>

#include "../WaveTrack.h"

struct ProgressInfo;
class EffectBaseHPSS;

typedef float HPSSFloat; // for HPSS core functions
typedef std::queue<HPSSFloat> SignalStream; // plugin-algorithm interface
enum MaskType { BinaryMask, WienerMask };

struct FrameInfo {
   std::vector<HPSSFloat> amplitudes;
   std::vector<HPSSFloat> cosinePhases;
   std::vector<HPSSFloat> sinePhases;
   std::vector<HPSSFloat> harmonicComponent;
   std::vector<HPSSFloat> percussiveComponent;
   
   FrameInfo(uint32_t size): amplitudes(size, 0), cosinePhases(size, 0),
         sinePhases(size, 0), harmonicComponent(size, 0), percussiveComponent(size, 0){}
};

class HPSSCore {
private:
   std::vector<HPSSFloat> analysisWindow, synthesisWindow, wH, wP;
   uint32_t frameSize, shift;
   float finalMultiplier;
   MaskType maskType;
   uint64_t inputSignalLength;
   uint32_t slidingBlockProcessRunCount;
   
   const uint32_t numIterations;
   const uint32_t blockSize;
   const float sigmaP;
   const float sigmaH;
   
   inline HPSSFloat sqr(HPSSFloat x) { return x*x; }
   HPSSFloat ConsumeOneSample(SignalStream& sourceStream);
   std::vector<std::complex<HPSSFloat> > FFT_R2C_Audacity(const std::vector<HPSSFloat>& input);
   std::vector<HPSSFloat> IFFT_C2R_Audacity(const std::vector<std::complex<HPSSFloat> >& input);
   FrameInfo CalcNewFrame(std::list<HPSSFloat>& inputBufferState, SignalStream& inputSignalStream);
   void SlidingBlockProcess(const FrameInfo& incomingFrame, std::list<FrameInfo>& slidingWindow,
                  std::vector<std::complex<HPSSFloat> >& outHarmonic, std::vector<std::complex<HPSSFloat> >& outPercussive);
   void CalcOutput(std::list<HPSSFloat>& out_buf, SignalStream& out_sig, const std::vector<std::complex<HPSSFloat> >& processedFrame);
   
public:
   HPSSCore();
   void executeHPSS(uint32_t frameSize, MaskType maskType, float finalMultiplier,
                    SignalStream& inputSignal, SignalStream& outputSignalH, SignalStream& outputSignalP,
                    ProgressInfo& progressInfo, EffectBaseHPSS* hpsHandle);

   void executeHPSSVocalRemoval(uint32_t shortFrameSize, uint32_t longFrameSize, MaskType maskType, float finalMultiplier,
            SignalStream& inputSignal, SignalStream& outVocal, SignalStream& outRest, int count, EffectBaseHPSS* hpsHandle);
};

#endif