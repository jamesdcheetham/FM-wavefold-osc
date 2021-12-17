/*
BSD 3-Clause License

Copyright (c) 2018, KORG INC.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "userosc.h"
#include "math.h"
#include "FMWF.hpp"

static Osc carrierOscillator;
static Osc modulatorOscillator;

void OSC_INIT(uint32_t platform, uint32_t api) {
	carrierOscillator.waveFold = 0.f;
	carrierOscillator.phase = 0.f;
	carrierOscillator.octave = 0;
	carrierOscillator.semitone = 0;
	carrierOscillator.cent = 0;
	carrierOscillator.waveControl = 0.f;
  
	modulatorOscillator.phase = 0.f;
	modulatorOscillator.fmDepth = 0.f;
	modulatorOscillator.semitone = 0;
	modulatorOscillator.cent = 0;
}


//main oscillator - uses modulus to create saw and triangle waves. 
float osc_main(float phase) {
	float waveControl = carrierOscillator.waveControl * 10.f;
	float gainAdjustment;

	
		if (waveControl < 0.5f) {
			gainAdjustment = fmod(waveControl,0.5f)*2.f;
	}
		else {
			gainAdjustment = 1.f + fmod(-waveControl,0.5f)*2.f;
	}
	
	float osc_mainOut;
		if (carrierOscillator.phase < waveControl) {
			osc_mainOut = fmod(carrierOscillator.phase,1.f) * (1.f + gainAdjustment) - (1.f + gainAdjustment)/2;
	}
		else {
			osc_mainOut = fmod(-carrierOscillator.phase,1.f) * (1.f + gainAdjustment) + (1.f + gainAdjustment)/2;
	}
	return osc_mainOut;
}

void OSC_CYCLE(const user_osc_param_t * const params, int32_t *yn, const uint32_t frames) { 
  float freqOsc1;
  float modOsc1;
  float fmOsc;
   
  //get frequency of carrier oscillator
  const float oscNote = ((params->pitch)>>8) + carrierOscillator.semitone + carrierOscillator.octave;
  const float oscMod = (params->pitch + carrierOscillator.cent) & 0xFF;
  
  //compute phase increments for carrier oscillator (from freq)
  const float f0 = osc_notehzf(oscNote);
  const float f1 = osc_notehzf(oscNote+1);
  const float f = clipmaxf(linintf(oscMod * k_note_mod_fscale, f0, f1), k_note_max_hz);
  
  //compute phase increments for FM oscillator (from note)
  const float w1 = osc_w0f_for_note(oscNote + modulatorOscillator.semitone, oscMod + modulatorOscillator.cent);  

   //get LFO value
  const float lfo = q31_to_f32(params->shape_lfo);  
  
  //setting buffer
  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;
  
	for (; y != y_e; ) {

		//FM oscillator
		float fmOsc = osc_sinf(modulatorOscillator.phase);
		modulatorOscillator.phase += w1;
		modulatorOscillator.phase -= (uint32_t)modulatorOscillator.phase;
		
		// Main (carrier) Oscillator
        float wavefold_audioIn  = 0.5f * osc_main(carrierOscillator.phase) * (1.f + carrierOscillator.waveFold);
		
		// wavefolder to main out
		const float wavefoldStage1 = (wavefold_audioIn < -0.5f) ? -1.f - wavefold_audioIn : (wavefold_audioIn >  0.5f) ? 1.f - wavefold_audioIn: wavefold_audioIn;
		const float audioSignal = (wavefoldStage1 < -0.5f) ? -1.f - wavefoldStage1 : (wavefoldStage1 >  0.5f) ? 1.f - wavefoldStage1: wavefoldStage1;		
		
		//set carrier phase after modulation
		const float w0 = (f + ((fmOsc + lfo) * modulatorOscillator.fmDepth)) * k_samplerate_recipf; 

        //set phase position of wave
		carrierOscillator.phase += w0;
		carrierOscillator.phase -= (uint32_t)carrierOscillator.phase;		
		
		//fill buffer
		*(y++) = f32_to_q31(audioSignal);
  }
}

void OSC_NOTEON(const user_osc_param_t * const params) {
}

void OSC_NOTEOFF(const user_osc_param_t * const params) {
  (void)params;
}

void OSC_PARAM(uint16_t index, uint16_t value) {
	  const float valf = param_val_to_f32(value);
	  
  switch (index) {
  case k_user_osc_param_id1:
	carrierOscillator.octave = value * 12;
	break;  
  case k_user_osc_param_id2:
	carrierOscillator.semitone = value;
	break;
  case k_user_osc_param_id3:
	carrierOscillator.cent = value;
	break;
  case k_user_osc_param_id4:
	carrierOscillator.waveControl = valf;
	break;
  case k_user_osc_param_id5:
	modulatorOscillator.semitone = value;
	break;
  case k_user_osc_param_id6:
	modulatorOscillator.cent = value;
	break;	
   case k_user_osc_param_shape:
	modulatorOscillator.fmDepth = value;
    break;
  case k_user_osc_param_shiftshape:
    carrierOscillator.waveFold = valf * 5.f;
	break;
  default:
    break;
  }
  
}