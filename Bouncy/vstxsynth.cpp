#include "vstxsynth.h"

#if WIN32
	#include "Registry.h"
	#include <string>
#endif

VstXSynth::VstXSynth (audioMasterCallback audioMaster) : AudioEffectX (audioMaster, kNumPrograms, kNumParams)
{
	setNumInputs (2);
	setNumOutputs (2);
	programsAreChunks(true);
	canProcessReplacing();
	setUniqueID ('BNCY');

	delayL = new Bouncy(getSampleRate());
	delayR = new Bouncy(getSampleRate());

	for (int i = 0; i < kNumPrograms; i++)
	{
		strcpy(save[i].name, "default");
		save[i].params[kMaxDelay] = 0.58f;
		save[i].params[kDelayShape] = 0.42f;
		save[i].params[kAmpShape] = 0.08f;
		save[i].params[kRandAmp] = 0.f;
	}

	curProgram = 0;

#if WIN32
	pf = 0;

	Registry reg("Smartelectronix\\Bouncy");

	if(reg.getLong("Debug") == 1)
	{
		pf = fopen("c:\\bouncy.log","wb");

		fprintf(pf,"Application-path : %s\n",__argv[0]);

		if( Multitap::SSEDetect() )
			fprintf(pf,"This processor supports SSE\n");
		else
			fprintf(pf,"This processor does not support SSE\n");

		if(reg.getLong("ForceSSE") == 1)
		{
			fprintf(pf,"Forcing SSE processing\n");
			delayL->forceSSE(true);
			delayR->forceSSE(true);
		}

		if(reg.getLong("ForceFPU") == 1)
		{
			fprintf(pf,"Forcing FPU processing\n");
			delayL->forceSSE(false);
			delayR->forceSSE(false);
		}
	}
#endif

	setParam();

	resume();
}

VstXSynth::~VstXSynth ()
{
	delete delayL;
	delete delayR;

#if WIN32
	if(pf != 0)
		fclose(pf);
#endif
}

void VstXSynth::getParameterLabel (VstInt32 index, char *label)
{
	switch(index)
	{
		case kMaxDelay : strcpy(label," beats"); break;
		default : strcpy(label,""); break;
	}
}

void VstXSynth::getParameterDisplay (VstInt32 index, char *text)
{
	switch(index)
	{
		case kMaxDelay : float2string(save[curProgram].params[index]*NUM_BEATS,text, 10); break;
		case kDelayShape : float2string(save[curProgram].params[index]*2.f - 1.f,text, 10); break;
		case kAmpShape : float2string(save[curProgram].params[index]*2.f - 1.f,text, 10); break;
		case kRenewRand :
			if(save[curProgram].params[kRenewRand] > 0.5f)
				strcpy(text,"on");
			else
				strcpy(text,"off");
			break;
		default : float2string(save[curProgram].params[index],text, 10); break;
	}
}

void VstXSynth::getParameterName (VstInt32 index, char *label)
{
	switch(index)
	{
		case kMaxDelay : strcpy(label,"max delay"); break;
		case kDelayShape : strcpy(label,"delay shape"); break;
		case kAmpShape : strcpy(label,"amp shape"); break;
		case kRandAmp : strcpy(label,"rand amp"); break;
		case kRenewRand : strcpy(label,"renew rand"); break;
	}
}

void VstXSynth::setParameter (VstInt32 index, float value)
{
	if(index < kNumParams)
	{
		save[curProgram].params[index] = value;

		if(index == kMaxDelay)
		{
			if ((save[curProgram].params[kMaxDelay] * 60.f * NUM_BEATS) / (BPM * 5.f) > 1.f)
				save[curProgram].params[kMaxDelay] = (BPM*5.f)/(60.f*NUM_BEATS);

			float beatzf = save[curProgram].params[kMaxDelay] * NUM_BEATS;
			long  beatzi = (long)beatzf;
			float diff = fabsf(beatzf - (float)beatzi);

			if(diff < 0.1f)
				save[curProgram].params[kMaxDelay] = (float) beatzi / (NUM_BEATS);
		}

		if(index == kRenewRand && save[curProgram].params[kRenewRand] > 0.5f)
		{
			delayL->fillRand();
			delayR->fillRand();
		}

		//if (editor)
		//	((AEffGUIEditor *)editor)->setParameter(index, value);

		setParam();
	}
}

float VstXSynth::getParameter (VstInt32 index)
{
	if(index < kNumParams)
		return save[curProgram].params[index];
	else
		return 0.f;
}

bool VstXSynth::getOutputProperties (VstInt32 index, VstPinProperties* properties)
{
	if(index < kNumOutputs)
	{
		properties->flags = kVstPinIsActive;
		return true;
	}
	return false;
}

bool VstXSynth::getEffectName (char* name)
{
	strcpy (name, "Bouncy");
	return true;
}

bool VstXSynth::getVendorString (char* text)
{
	strcpy (text, "Bram @ Smartelectronix");
	return true;
}

bool VstXSynth::getProductString (char* text)
{
	strcpy (text, "Bouncing ball delay");
	return true;
}

VstInt32 VstXSynth::canDo (char* text)
{
	if (!strcmp(text, "receiveVstTimeInfo"))  return 1;
	if (!strcmp(text, "receiveVstMidiEvent")) return 1;
	if (!strcmp(text, "plugAsChannelInsert")) return 1;
	if (!strcmp(text, "plugAsSend")) return 1;
	if (!strcmp(text, "1in2out")) return 1;
	if (!strcmp(text, "2in2out")) return 1;

	return -1;	// explicitly can't do; 0 => don't know
}

void VstXSynth::setProgram (VstInt32 program)
{
	curProgram = program;
	setParam();
}

void VstXSynth::setProgramName (char *name)
{
	strcpy(save[curProgram].name, name);
	setParam();
}

void VstXSynth::getProgramName (char *name)
{
	strcpy (name, save[curProgram].name);
}

// Override getChunk to save parameters
VstInt32 VstXSynth::getChunk(void** data, bool isPreset) {
	*data = malloc(sizeof(BouncyProgram));
	if (*data) {
		memcpy(*data, &save[curProgram], sizeof(BouncyProgram));
		return sizeof(BouncyProgram);
	}
	return 0; // Failed to allocate memory
}

// Override setChunk to load parameters
VstInt32 VstXSynth::setChunk(void* data, VstInt32 byteSize, bool isPreset) {
	if (byteSize == sizeof(BouncyProgram)) {
		memcpy(&save[curProgram], data, byteSize);
		setParam();
		return 1; // Success
	}
	return 0; // Chunk size mismatch
}

VstInt32 VstXSynth::processEvents (VstEvents* ev)
{
	for (long i = 0; i < ev->numEvents; i++)
	{
		if ((ev->events[i])->type != kVstMidiType)
			continue;

		VstMidiEvent* event = (VstMidiEvent*)ev->events[i];
		unsigned char* midiData = (unsigned char *)event->midiData;

		unsigned char status = midiData[0] & 0xF0;
		unsigned char cc = midiData[1] & 0x7F;	// CC number

		if(status == 0xB0) //CC but not all-notes-off
		{
			float value = (float)(midiData[2] & 0x7F) / 127.0f;

			switch(cc)
			{
				case 73 : setParameter(kMaxDelay, value); break;
				case 74 : setParameter(kDelayShape, value); break;
				case 75 : setParameter(kAmpShape, value); break;
				case 76 : setParameter(kRandAmp, value); break;
				case 77 : setParameter(kRenewRand, value); break;
			}
		}
	}
	return 1;
}
