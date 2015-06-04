#include <iostream>
#include <stdexcept>
#include <cmath>
#include <list>
#include <wx/defs.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/validate.h>
#include <wx/valtext.h>
#include <wx/generic/textdlgg.h>
#include <wx/intl.h>
#include <inttypes.h>

#include "../WaveTrack.h"
#include "../Project.h"
#include "../Audacity.h"

#include "HPSS-pluginbase.h"
#include "HPSS-parameter.h"
#include "HPSS-core.h"

// ================================================================================
//  == Parameter class base implementations
// =================================================
void HPSSParameter_ReadOnlyText::TransferDataToWindow(EffectBaseHPSS* pEffect) {
   try {
      m_pText->SetLabel(wxString::Format(_("%6.2f"), pEffect->GetSelectionSampleRate()));
   } catch (const std::runtime_error& ex) {
      m_pText->SetLabel(wxString(ex.what(), wxConvUTF8));
   }
}

void HPSSParameter_FrameSizeSlider::UpdateSamplesControl(EffectBaseHPSS* pEffect) {
   try {
      m_pSamplesControl->SetLabel(wxString::Format(wxT("%i"),
                     pEffect->FrameSizeMS_To_FrameSizeSamples(m_Value, pEffect->GetSelectionSampleRate())));
   } catch(const std::runtime_error& ex) {
      m_pSamplesControl->SetLabel(_("ERROR: variable sample rate"));
   }
}

// ================================================================================
//  == EffectBaseHPSS
// ======================================

EffectBaseHPSS::EffectBaseHPSS(const std::string& name, MaskType defaultMaskType, EffectOutputMode defaultOutputMode):
                     name(name), m_SamplingRateDisplay("Input sampling rate", "Hz"), m_MaskTypeParameter("Mask type", "", defaultMaskType),
                     m_OutputModeParameter("Output mode", "", defaultOutputMode) {
   std::vector<std::string> v;
   v.push_back("Binary mask");
   v.push_back("Wiener mask");
   m_MaskTypeParameter.SetOptionNames(v);
}

std::string EffectBaseHPSS::GetDebugDescription() {
   std::stringstream ss;
   ss << name << " ";
   const std::list<HPSSParameter*> parameters = GetParameters();
   for (std::list<HPSSParameter*>::const_iterator it = parameters.begin(); it != parameters.end(); it++) {
      ss << (*it)->ToNameValueString();
   }
   return ss.str();
}

bool EffectBaseHPSS::Process() {
   try {
      this->CopyInputTracks(); // ... to mOutputTracks.

      SelectedTrackListOfKindIterator iter(Track::Wave, mOutputTracks);
      WaveTrack *track = (WaveTrack *) iter.First();
      int count = 0;
      while (track) {
         double trackStart = track->GetStartTime();
         double trackEnd = track->GetEndTime();
         double t0 = mT0 < trackStart? trackStart: mT0;
         double t1 = mT1 > trackEnd? trackEnd: mT1;
         
         if (t1 <= t0) { // can this even happen?
            track = (WaveTrack *) iter.Next(false);
            continue;
         }
         sampleCount start = track->TimeToLongSamples(t0);
         sampleCount end = track->TimeToLongSamples(t1);
         sampleCount len = (sampleCount)(end - start);

         switch(m_OutputModeParameter.GetValue()) {
            case KeepBothTracks: {
               wxString nameSuffix1 = _(" (") + GetOutputName(1) + _(")");
               wxString nameSuffix2 = _(" (") + GetOutputName(2) + _(")");
               if (track->GetLinked()) { // Stereo track (no support for surround?!)
                  WaveTrack* track2 = (WaveTrack*) track->GetLink();
                  SignalStream comp1FromInput1, comp1FromInput2, comp2FromInput1, comp2FromInput2;
                  ProcessOne(count, track, start, len, comp1FromInput1, comp2FromInput1);
                  count++;
                  ProcessOne(count, track2, start, len, comp1FromInput2, comp2FromInput2);
                  count++;
                  WaveTrack* wt1 = MY_AddToOutputTracks(track, comp1FromInput1, start, len, nameSuffix1);
                  MY_AddToOutputTracks(track2, comp1FromInput2, start, len, nameSuffix1);
                  WaveTrack* wt2 = MY_AddToOutputTracks(track, comp2FromInput1, start, len, nameSuffix2);
                  MY_AddToOutputTracks(track2, comp2FromInput2, start, len,  nameSuffix2);
                  wt1->SetLinked(true);
                  wt2->SetLinked(true);
               } else { // Mono track
                  SignalStream outputSignal1, outputSignal2;
                  ProcessOne(count, track, start, len, outputSignal1, outputSignal2);
                  MY_AddToOutputTracks(track, outputSignal1, start, len, nameSuffix1);
                  MY_AddToOutputTracks(track, outputSignal2, start, len, nameSuffix2);
                  count++;
               }
               track = (WaveTrack *) iter.Next(true); // skip linked (we already processed it)
            } break;
            case FirstTrackOnly: {
               SignalStream outputSignal1, outputSignal2;
               ProcessOne(count, track, start, len, outputSignal1, outputSignal2);
               SignalStreamToWaveTrack(outputSignal1, track, start, len, false);
               count++;
               track = (WaveTrack *) iter.Next(false); // DON'T skip linked
            } break;
            case SecondTrackOnly: {
               SignalStream outputSignal1, outputSignal2;
               ProcessOne(count, track, start, len, outputSignal1, outputSignal2);
               SignalStreamToWaveTrack(outputSignal2, track, start, len, false);
               count++;
               track = (WaveTrack *) iter.Next(false); // DON'T skip linked
            } break;
            default: {
               std::cerr << "Internal error: m_EffectOutputMode is invalid." << std::endl;
               throw 42; // TODO better way
            }
         }
      }
      this->ReplaceProcessedTracks(true);
      return true;
   } catch (const CancelledException& ex) {
      std::cout << "HPSS effect cancelled" << std::endl;
      return false;
   }
}

void EffectBaseHPSS::ReportProgress(const ProgressInfo& progressInfo) {
   if (TrackProgress(progressInfo.whichTrack, progressInfo.toGlobalProgress())) {
      throw CancelledException();
   }
}

void EffectBaseHPSS::SignalStreamToWaveTrack(SignalStream& sourceStream, WaveTrack* targetTrack,
                                           sampleCount start, sampleCount len, bool useClip) {
   // Convert to float[] and add result to output 
   float* tempArray = new float[len]; // on HEAP, not on stack, due to size)
   for (int i = 0; i < len; i++) {
      tempArray[i] = sourceStream.front();
      sourceStream.pop();
   }
   if (useClip) { // create a new clip at the right position
      WaveClip* outputClip1 = targetTrack->CreateClip();
      outputClip1->Offset(targetTrack->LongSamplesToTime(start));
      outputClip1->Append((samplePtr)tempArray, floatSample, len);
      outputClip1->Flush();
   } else { // simply set the samples
      targetTrack->Set((samplePtr)tempArray, floatSample, start, len);
   }
   delete [] tempArray;
}

void EffectBaseHPSS::ProcessOne(int whichTrack, WaveTrack * track, sampleCount start, sampleCount len,
                                SignalStream& outputSignal1, SignalStream& outputSignal2) {
   std::cout << "[HPSSDebug] ProcessOne called with start " << start << ", len " << len << std::endl;
   SignalStream inputSignal;
   // Get input float data and convert to signal stream (on HEAP, not on stack, due to size)
   float* inputSignalFloat = new float[len];
   track->Get((samplePtr)inputSignalFloat, floatSample, start, len);
   for (int i = 0; i < len; i++) {
      if (inputSignalFloat[i] != inputSignalFloat[i]) {
         std::cout << "INVALID SAMPLE IN INPUT. signal[" << i << "]=" << inputSignalFloat[i] << std::endl;
      }
      inputSignal.push(inputSignalFloat[i]);
   }
   delete [] inputSignalFloat;
   // Run effect
   RunCore(inputSignal, outputSignal1, outputSignal2, track->GetRate(), whichTrack);
}

WaveTrack* EffectBaseHPSS::MY_AddToOutputTracks(WaveTrack* inputTrack, SignalStream& data,
                  sampleCount start, sampleCount len, const wxString& nameSuffix) {
   WaveTrack* newTrack = GetActiveProject()->GetTrackFactory()->NewWaveTrack(floatSample, inputTrack->GetRate());
   newTrack->SetName(inputTrack->GetName() + nameSuffix);
   SignalStreamToWaveTrack(data, newTrack, start, len, true);
   AddToOutputTracks(newTrack);
   return newTrack;
}

double EffectBaseHPSS::GetSelectionSampleRate() {
   double selectionSampleRate = ((WaveTrack*)SelectedTrackListOfKindIterator(Track::Wave, mTracks).First())->GetRate();
   SelectedTrackListOfKindIterator iter(Track::Wave, mTracks);
   for (WaveTrack *track = (WaveTrack *) iter.First(); track!= NULL; track = (WaveTrack *) iter.Next()) {
      if (track->GetRate() != selectionSampleRate) {
         throw std::runtime_error("Variable sample rate not allowed. The sampling rate must be the same in all selected tracks.");
      }
   }
   return selectionSampleRate;
}

uint32_t EffectBaseHPSS::FrameSizeSamples_To_FrameSizeMS(uint32_t frameSizeSamples, double sampleRate) {
   double frameSizeMS = (double)frameSizeSamples/(double)sampleRate * 1000.0f;
   return (uint32_t)round(frameSizeMS);
}

uint32_t EffectBaseHPSS::FrameSizeMS_To_FrameSizeSamples(uint32_t frameSizeMS, double sampleRate) {
   int64_t desiredSamples = (int64_t)round((double)frameSizeMS / 1000.0f*(double)sampleRate);
   int64_t closestSamples = 0;
   for (int32_t i = 2; i < pow(2, 30); i *= 2) { // enumerate powers of two
      if (std::abs(i-desiredSamples) < std::abs(closestSamples-desiredSamples)) {
         closestSamples = i;
      }
   }
   return closestSamples;
}

bool EffectBaseHPSS::GetAutomationParameters(EffectAutomationParameters & parms) {
   std::cout << "SetAutomationParameters" << std::endl;
   const std::list<HPSSParameter*> parameters = GetParameters();
   for (std::list<HPSSParameter*>::const_iterator it = parameters.begin(); it != parameters.end(); it++) {
      (*it)->GetAutomationParameters(parms);
   }
   return true;
}

bool EffectBaseHPSS::SetAutomationParameters(EffectAutomationParameters & parms) {
   std::cout << "SetAutomationParameters" << std::endl;
   const std::list<HPSSParameter*> parameters = GetParameters();
   for (std::list<HPSSParameter*>::const_iterator it = parameters.begin(); it != parameters.end(); it++) {
      (*it)->SetAutomationParameters(parms);
   }
   return true;
}

void EffectBaseHPSS::PopulateOrExchange(ShuttleGui & S) {
   S.StartMultiColumn(2, wxALIGN_CENTER);
   {
      std::cout << "PopulateOrExchange" << std::endl;
      const std::list<HPSSParameter*> parameters = GetParameters();
      for (std::list<HPSSParameter*>::const_iterator it = parameters.begin(); it != parameters.end(); it++) {
         (*it)->PopulateOrExchange(S);
      }
   }
   S.EndMultiColumn();
}

bool EffectBaseHPSS::TransferDataToWindow() {
   std::cout << "TransferDataToWindow start" << std::endl;
   const std::list<HPSSParameter*> parameters = GetParameters();
   for (std::list<HPSSParameter*>::const_iterator it = parameters.begin(); it != parameters.end(); it++) {
      (*it)->TransferDataToWindow(this);
   }
   std::cout << "TransferDataToWindow end" << std::endl;
   return true;
}

bool EffectBaseHPSS::TransferDataFromWindow() {
   std::cout << "TransferDataFromWindow start" << std::endl;
   const std::list<HPSSParameter*> parameters = GetParameters();
   for (std::list<HPSSParameter*>::const_iterator it = parameters.begin(); it != parameters.end(); it++) {
      (*it)->TransferDataFromWindow();
   }
   std::cout << "TransferDataFromWindow end" << std::endl;
   return true;
}

void EffectBaseHPSS::ShowValidationError(const std::exception& ex) {
   wxMessageBox(wxString(ex.what(), wxConvUTF8), _("Validation error"), wxOK | wxICON_EXCLAMATION, mUIParent /* TODO ?!?!*/);
}