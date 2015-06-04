#ifndef HPSS_VOCAL_REMOVAL_H
#define HPSS_VOCAL_REMOVAL_H

static const wxString HPSSVOCALREMOVAL_PLUGIN_SYMBOL = XO("HPSS_VOCAL_REMOVAL");

// =============================================================================
//  == EffectHPSSVocalRemoval
// ==================================
class EffectHPSSVocalRemoval:public EffectBaseHPSS {
private:
   HPSSParameter_FrameSizeSlider m_FrameSizeShortMSParameter;
   HPSSParameter_FrameSizeSlider m_FrameSizeLongMSParameter;
public:
   EffectHPSSVocalRemoval();
   
   virtual wxString GetSymbol() { return HPSSVOCALREMOVAL_PLUGIN_SYMBOL; }
   virtual wxString GetName() { return XO("HPSS-based vocal removal"); }
   virtual wxString GetDescription() { return XO("Separates vocal and non-vocal components of a music track"); }
   virtual EffectType GetType() { return EffectTypeProcess; }
   virtual int GetAudioInCount() { return 1; }
   virtual int GetAudioOutCount() { return 1; }
   
   virtual wxString GetOutputName(int index) { return index == 1 ? _("Vocal") : _("Rest"); }
   virtual std::list<HPSSParameter*> GetParameters();
   virtual void RunCore(SignalStream& inputSignal, SignalStream& outputSignal1, SignalStream& outputSignal2,
                        const double sampleRate, const int whichTrack);
   virtual bool ValidateUI();
   virtual void OnSliderChanged(wxScrollEvent& event);
private:
   DECLARE_EVENT_TABLE()
};

#endif