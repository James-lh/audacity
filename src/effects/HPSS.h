#ifndef HPSS_H
#define HPSS_H

#include "HPSS-parameter.h"
#include "HPSS-pluginbase.h"

static const wxString HPSS_PLUGIN_SYMBOL = XO("Harmonic-Percussive Sound Separation (HPSS)");

// =============================================================================
//  == EffectHPSS
// ==================================
class EffectHPSS:public EffectBaseHPSS {
private:
   HPSSParameter_FrameSizeSlider m_FrameSizeMSParameter;
public:
   EffectHPSS();
   
   virtual wxString GetSymbol() { return HPSS_PLUGIN_SYMBOL; }
   virtual wxString GetName() { return XO("Harmonic-Percussive Sound Separation (HPSS)"); }
   virtual wxString GetDescription() { return XO("Separates harmonic and percussive components of a music track"); }
   virtual EffectType GetType() { return EffectTypeProcess; }
   virtual int GetAudioInCount() { return 1; }
   virtual int GetAudioOutCount() { return 2; }
   
   virtual wxString GetOutputName(int index) { return index == 1 ? _("Harmonic") : _("Percussive"); }
   virtual std::list<HPSSParameter*> GetParameters();
   virtual void RunCore(SignalStream& inputSignal, SignalStream& outputSignal1, SignalStream& outputSignal2,
                        const double sampleRate, const int whichTrack);
   virtual bool ValidateUI();
   virtual void OnSliderChanged(wxScrollEvent& event);
private:
   DECLARE_EVENT_TABLE()
};

#endif