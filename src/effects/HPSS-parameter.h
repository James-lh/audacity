#ifndef HPSS_PARAMETER_H
#define HPSS_PARAMETER_H

#include <vector>
#include <string>
#include <sstream>
#include <inttypes.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/slider.h>

#include "Effect.h"

class EffectBaseHPSS;

// =============================================================================
//  == Parameter classes
// ==================================
class HPSSParameter {
protected:
   std::string m_Name, m_Unit;
   const int32_t defaultControlWidth;
public:
   HPSSParameter(const std::string& name, const std::string& unit) : m_Name(name), m_Unit(unit), defaultControlWidth(500) {}
   virtual std::string GetNameWithUnit() {
      return m_Unit.empty() ? m_Name : (m_Name + std::string(" [") + m_Unit + std::string("]"));
   }
   virtual std::string GetValueString() = 0;
   virtual std::string ToNameValueString() {
      return std::string("[") + m_Name + std::string(": ") + GetValueString() + m_Unit + std::string("]");
   }
   virtual void PopulateOrExchange(ShuttleGui& S) = 0;
   virtual void TransferDataToWindow(EffectBaseHPSS* pEffect) = 0;   
   virtual void TransferDataFromWindow() = 0;
   virtual void GetAutomationParameters(EffectAutomationParameters & parms) = 0;
   virtual void SetAutomationParameters(EffectAutomationParameters & parms) = 0;
};

class HPSSParameter_ReadOnlyText : public HPSSParameter {
protected:
   wxStaticText* m_pText;
public:
   HPSSParameter_ReadOnlyText(const std::string& name, const std::string& unit) : HPSSParameter(name, unit), m_pText(NULL) {}
   virtual std::string GetValueString() { return "N/A"; }
   virtual void PopulateOrExchange(ShuttleGui& S) {
      S.AddPrompt(wxString(GetNameWithUnit().c_str(), wxConvUTF8) + _(":"));
      m_pText = S.AddVariableText(_("PLACEHOLDER"));
      m_pText->SetMinSize(wxSize(defaultControlWidth, -1));
   }
   virtual void TransferDataToWindow(EffectBaseHPSS* pEffect);
   virtual void TransferDataFromWindow() {}
   virtual void GetAutomationParameters(EffectAutomationParameters & parms) {}
   virtual void SetAutomationParameters(EffectAutomationParameters & parms) {} 
};

class HPSSParameter_NumericSlider : public HPSSParameter {
protected:
   int m_Value;
   int m_MinValue, m_MaxValue;
   wxSlider* m_pSliderControl;
public:
   HPSSParameter_NumericSlider(const std::string& name, const std::string& unit, const int& value, int minValue, int maxValue):
                             HPSSParameter(name, unit), m_Value(value), m_MinValue(minValue), m_MaxValue(maxValue),
                             m_pSliderControl(NULL) {}
   virtual int GetValue() { return m_Value; }
   virtual std::string GetValueString() { std::stringstream ss; ss << GetValue(); return ss.str(); }
   virtual void PopulateOrExchange(ShuttleGui& S) {
      m_pSliderControl = S.AddSlider(wxString(GetNameWithUnit().c_str(), wxConvUTF8) + _(":"), m_Value, m_MaxValue, m_MinValue);
      m_pSliderControl->SetMinSize(wxSize(defaultControlWidth, -1));
   }
   virtual void TransferDataToWindow(EffectBaseHPSS* pEffect) { m_pSliderControl->SetValue(m_Value); }
   virtual void TransferDataFromWindow() { m_Value = m_pSliderControl->GetValue(); }
   virtual void GetAutomationParameters(EffectAutomationParameters & parms) {
      parms.WriteFloat(wxString(m_Name.c_str(), wxConvUTF8), m_Value);
   }
   virtual void SetAutomationParameters(EffectAutomationParameters & parms) {
      float temp; parms.ReadFloat(wxString(m_Name.c_str(), wxConvUTF8), &temp); m_Value = (int)temp;
   } 
};

class HPSSParameter_FrameSizeSlider : public HPSSParameter_NumericSlider {
protected:
   wxStaticText* m_pSamplesControl;
   wxStaticText* m_pMillisecControl;
public:
   HPSSParameter_FrameSizeSlider(const std::string& name, const std::string& unit, const int& value):
                           HPSSParameter_NumericSlider(name, unit, value, 1, 20), m_pSamplesControl(NULL), m_pMillisecControl(NULL) {}
   virtual void PopulateOrExchange(ShuttleGui& S) {
      HPSSParameter_NumericSlider::PopulateOrExchange(S);
      S.AddPrompt(wxString(m_Name.c_str(), wxConvUTF8) + _(" [samples]:"));
      m_pSamplesControl = S.AddVariableText(_("PLACEHOLDER"));
      S.AddPrompt(wxString(m_Name.c_str(), wxConvUTF8) + _(" [ms @ 44.1kHz]:"));
      m_pMillisecControl = S.AddVariableText(_("PLACEHOLDER"));
   }
   
   virtual void TransferDataToWindow(EffectBaseHPSS* pEffect) {
      HPSSParameter_NumericSlider::TransferDataToWindow(pEffect);
      UpdateSamplesControl(pEffect);
   }
   
   virtual void UpdateSamplesControl(EffectBaseHPSS* pEffect);
};

class HPSSParameter_FinalMultiplierSlider : public HPSSParameter_NumericSlider {
public:
   HPSSParameter_FinalMultiplierSlider(): HPSSParameter_NumericSlider("Final multiplication", "%", 80, 1, 200) {}
};

template<typename T> class HPSSParameter_Enum : public HPSSParameter {
private:
   T m_Value;
   std::vector<std::string> m_OptionNames;
   wxChoice* m_pChoiceControl;
   
   virtual wxArrayString GetOptionNamesAsWxArrayString() {
      wxArrayString temp; // WX Array String (!!!)
      for (std::vector<std::string>::const_iterator it = m_OptionNames.begin(); it != m_OptionNames.end(); it++) {
         temp.Add(wxString(it->c_str(), wxConvUTF8));
      }
      return temp;
   }
public:
   HPSSParameter_Enum(const std::string& name, const std::string& unit, const T& value):
                        HPSSParameter(name, unit), m_Value(value), m_pChoiceControl(NULL) {}
   virtual void SetOptionNames(const std::vector<std::string>& optionNames) { m_OptionNames = optionNames; }
   virtual T GetValue() { return m_Value; }
   virtual std::string GetValueString() { return m_OptionNames[GetValue()]; }
   
   virtual void PopulateOrExchange(ShuttleGui& S) {
      wxArrayString optionNamesWXAS = GetOptionNamesAsWxArrayString();
      m_pChoiceControl = S.AddChoice(wxString(GetNameWithUnit().c_str(), wxConvUTF8) + _(":"), optionNamesWXAS[1], &optionNamesWXAS);
      m_pChoiceControl->SetMinSize(wxSize(defaultControlWidth, -1));
   }
   
   virtual void TransferDataToWindow(EffectBaseHPSS* pEffect) { m_pChoiceControl->SetSelection(m_Value); }
   virtual void TransferDataFromWindow() { m_Value = (T)m_pChoiceControl->GetSelection(); }
   
   virtual void GetAutomationParameters(EffectAutomationParameters & parms) {
      wxArrayString optionNamesWXAS = GetOptionNamesAsWxArrayString();
      parms.WriteEnum(wxString(m_Name.c_str(), wxConvUTF8), m_Value, optionNamesWXAS);
   }
   virtual void SetAutomationParameters(EffectAutomationParameters & parms) {
      wxArrayString optionNamesWXAS = GetOptionNamesAsWxArrayString();
      parms.ReadEnum(wxString(m_Name.c_str(), wxConvUTF8), (int*)&m_Value, optionNamesWXAS);
   }
};

#endif