/**********************************************************************
  Audacity: A Digital Audio Editor
  HPSSVocalRemoval.cpp
*******************************************************************//**
\class EffectHPSSVocalRemoval
\brief Vocal removal based on Harmonic-Percussive Sound Separation
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
#include <wx/validate.h>
#include <wx/valtext.h>
#include <wx/generic/textdlgg.h>
#include <wx/intl.h>
#include <inttypes.h>

#include "../WaveTrack.h"
#include "../Project.h"
#include "../Audacity.h"

#include "HPSS-core.h"
#include "HPSS-pluginbase.h"
#include "HPSSVocalRemoval.h"

// ================================================================================
//  == EffectHPSSVocalRemoval
// ======================================

BEGIN_EVENT_TABLE(EffectHPSSVocalRemoval, wxEvtHandler)
   EVT_SCROLL(EffectHPSSVocalRemoval::OnSliderChanged)
END_EVENT_TABLE()

EffectHPSSVocalRemoval::EffectHPSSVocalRemoval() :
         EffectBaseHPSS("HPSS-based vocal removal", WienerMask, SecondTrackOnly),
         m_FrameSizeShortMSParameter("Frame size (short)", "2^x samples", 9),
         m_FrameSizeLongMSParameter("Frame size (long)", "2^x samples", 13) {
   std::vector<std::string> v;
   v.push_back("Vocal component only");
   v.push_back("Non-vocal component only");
   v.push_back("Keep both components (create new tracks)");
   m_OutputModeParameter.SetOptionNames(v);
}

std::list<HPSSParameter*> EffectHPSSVocalRemoval::GetParameters() {
   std::list<HPSSParameter*> parameters;
   //parameters.push_back(&m_SamplingRateDisplay);
   parameters.push_back(&m_MaskTypeParameter);
   parameters.push_back(&m_OutputModeParameter);
   parameters.push_back(&m_FrameSizeShortMSParameter);
   parameters.push_back(&m_FrameSizeLongMSParameter);
   parameters.push_back(&m_FinalMultiplierParameter);
   return parameters;
}

void EffectHPSSVocalRemoval::RunCore(SignalStream& inputSignal, SignalStream& outputSignal1, SignalStream& outputSignal2,
                                     const double sampleRate, const int whichTrack) {
   HPSSCore().executeHPSSVocalRemoval(
      (int)pow(2, m_FrameSizeShortMSParameter.GetValue()),
      (int)pow(2, m_FrameSizeLongMSParameter.GetValue()),
      m_MaskTypeParameter.GetValue(),
      (float)m_FinalMultiplierParameter.GetValue() / 100,
      inputSignal, outputSignal1, outputSignal2, whichTrack, this);
}

bool EffectHPSSVocalRemoval::ValidateUI() {
   try {
      std::cout << "Validation: Forcing data transfer" << std::endl;
      TransferDataFromWindow();
      std::cout << "Validation: short " << m_FrameSizeShortMSParameter.GetValue() << std::endl;
      std::cout << "Validation: long " << m_FrameSizeLongMSParameter.GetValue() << std::endl;
      if (m_FrameSizeLongMSParameter.GetValue() <= m_FrameSizeShortMSParameter.GetValue()) {
         throw std::runtime_error("Long frame size should be longer than the short one.");
      }
      return true;
   } catch (const std::runtime_error& ex) {
      ShowValidationError(ex);
      return false;
   }
}

void EffectHPSSVocalRemoval::OnSliderChanged(wxScrollEvent& event) {
   TransferDataFromWindow();
   m_FrameSizeShortMSParameter.UpdateSamplesControl(this);
   m_FrameSizeLongMSParameter.UpdateSamplesControl(this);
}