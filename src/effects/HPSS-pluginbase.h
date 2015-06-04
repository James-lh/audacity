#ifndef HPSS_PLUGINBASE_H
#define HPSS_PLUGINBASE_H

#include <list>
#include <vector>
#include <string>
#include <stdexcept>
#include <wx/dialog.h>
#include <wx/intl.h>
#include "Effect.h"
#include "HPSS-parameter.h"
#include "HPSS-core.h"

class wxString;
class wxStaticText;
class WaveTrack;

class CancelledException : public std::exception {};
enum EffectOutputMode { FirstTrackOnly, SecondTrackOnly, KeepBothTracks };

struct ProgressInfo {
   int whichTrack;
   float globalProgressStart;
   float globalProgressEnd;
   float localProgress;

   ProgressInfo(int whichTrack, float globalProgressStart, float globalProgressEnd):
      whichTrack(whichTrack), globalProgressStart(globalProgressStart), globalProgressEnd(globalProgressEnd) {}
      
   float toGlobalProgress() const {
      return localProgress * (globalProgressEnd - globalProgressStart) + globalProgressStart;
   }
};

// =============================================================================
//  == Effect class base
// ==================================
class EffectBaseHPSS: public Effect {
private:
   const std::string name;
public:
   HPSSParameter_ReadOnlyText m_SamplingRateDisplay;
   HPSSParameter_Enum<MaskType> m_MaskTypeParameter;
   HPSSParameter_Enum<EffectOutputMode> m_OutputModeParameter;
   HPSSParameter_FinalMultiplierSlider m_FinalMultiplierParameter;
   EffectBaseHPSS(const std::string& name, MaskType defaultMaskType, EffectOutputMode defaultOutputMode);
   virtual std::string GetDebugDescription();
   virtual wxString GetOutputName(int index) = 0;
   virtual std::list<HPSSParameter*> GetParameters() = 0;
   virtual bool GetAutomationParameters(EffectAutomationParameters & parms);
   virtual bool SetAutomationParameters(EffectAutomationParameters & parms);
   virtual double GetSelectionSampleRate();
   virtual void RunCore(SignalStream& inputSignal, SignalStream& outputSignal1, SignalStream& outputSignal2,
                        const double sampleRate, const int whichTrack) = 0;
   virtual bool Process();
   virtual void ReportProgress(const ProgressInfo& progressInfo);
   virtual void SignalStreamToWaveTrack(SignalStream& sourceStream, WaveTrack* targetTrack,
                                      sampleCount start, sampleCount len, bool useClip);
   virtual WaveTrack* MY_AddToOutputTracks(WaveTrack* inputTrack, SignalStream& data,
                  sampleCount start, sampleCount len, const wxString& nameSuffix);
   virtual void ProcessOne(int whichTrack, WaveTrack * track, sampleCount start, sampleCount len,
                                SignalStream& outputSignal1, SignalStream& outputSignal2);
   virtual uint32_t FrameSizeSamples_To_FrameSizeMS(uint32_t frameSizeSamples, double sampleRate);
   virtual uint32_t FrameSizeMS_To_FrameSizeSamples(uint32_t frameSizeMS, double sampleRate);
   virtual void PopulateOrExchange(ShuttleGui & S);
   virtual bool TransferDataFromWindow();
   virtual bool TransferDataToWindow();
   virtual void ShowValidationError(const std::exception& ex);
};

#endif