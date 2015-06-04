/**********************************************************************
  Audacity: A Digital Audio Editor
  HPSS.cpp
*******************************************************************//**
\class EffectHPSS
\brief Harmonic-Percussive Sound Separation effect
*//*******************************************************************/

#include <iostream>
#include <stdexcept>
#include <cmath>
#include <list>
#include <wx/defs.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/slider.h>
#include <wx/validate.h>
#include <wx/valtext.h>
#include <wx/generic/textdlgg.h>
#include <wx/intl.h>
#include <inttypes.h>

#include "../WaveTrack.h"
#include "../Project.h"
#include "../Audacity.h"

#include "HPSS.h"
#include "HPSS-core.h"
#include "HPSS-pluginbase.h"

// ================================================================================
//  == EffectHPSS
// ======================================

BEGIN_EVENT_TABLE(EffectHPSS, wxEvtHandler)
    EVT_SCROLL(EffectHPSS::OnSliderChanged)
END_EVENT_TABLE()

EffectHPSS::EffectHPSS() : EffectBaseHPSS("Harmonic/percussive sound separation", WienerMask, KeepBothTracks),
                           m_FrameSizeMSParameter("Frame size", "ms", 15) {
   std::vector<std::string> v;
   v.push_back("Harmonic only");
   v.push_back("Percussive only");
   v.push_back("Keep both components (create new tracks)");
   m_OutputModeParameter.SetOptionNames(v);
}

std::list<HPSSParameter*> EffectHPSS::GetParameters() {
   std::list<HPSSParameter*> parameters;
   parameters.push_back(&m_SamplingRateDisplay);
   parameters.push_back(&m_MaskTypeParameter);
   parameters.push_back(&m_OutputModeParameter);
   parameters.push_back(&m_FrameSizeMSParameter);
   parameters.push_back(&m_FinalMultiplierParameter);
   return parameters;
}

void EffectHPSS::RunCore(SignalStream& inputSignal, SignalStream& outputSignal1, SignalStream& outputSignal2,
                         const double sampleRate, const int whichTrack) {
   ProgressInfo progressInfo = ProgressInfo(whichTrack, 0, 1);
   std::cout << "[RunCore] " << GetDebugDescription() << std::endl;
   HPSSCore().executeHPSS(FrameSizeMS_To_FrameSizeSamples(m_FrameSizeMSParameter.GetValue(), sampleRate),
                          m_MaskTypeParameter.GetValue(), (float)m_FinalMultiplierParameter.GetValue() / 100,
                          inputSignal, outputSignal1, outputSignal2, progressInfo, this);
}

bool EffectHPSS::ValidateUI() {
   try {
      GetSelectionSampleRate(); // throws if there is a problem
      return true;
   } catch (const std::runtime_error& ex) {
      ShowValidationError(ex);
      return false;
   }
}

void EffectHPSS::OnSliderChanged(wxScrollEvent& event) {
   TransferDataFromWindow();
   m_FrameSizeMSParameter.UpdateSamplesControl(this);
}