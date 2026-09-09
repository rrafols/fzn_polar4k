//--------------------------------------------------------------------------//
// iq / rgba  .  tiny codes  .  2008                                        //
//--------------------------------------------------------------------------//

#include <windows.h>
#include <math.h>
#include <mmsystem.h>
#define CINTERFACE 1
#include <dsound.h>
#include "minimath.h"
#include "mzk.h"

#define BPM 110
#define NUM_SAMPLES 8
#define NCHANNELS 8
#define TONE_PORT


typedef struct sample SAMPLE;

LPDIRECTSOUND		dsound;
LPDIRECTSOUNDBUFFER dprimary;
LPDIRECTSOUNDBUFFER dsamples[NUM_SAMPLES * NCHANNELS];
DSBUFFERDESC		bufdesc={0};	
WAVEFORMATEX		format={0};


void		*writebuf;
int			size;

int	icnt1,icnt2,icnt3,iacc1;
float cnt105 =		  1.05946309436f;
float cnt33k =		  1378.125;	//16351.5;	//33152;;
int			tID;

struct sample {
	unsigned char wType;
	unsigned short nSamples;
	short freqIni;
	short dFreq;
//	short ddFreq;
	char ampIni;
	char dAmp;
	unsigned char cutIni;
	char dCut;
	char resIni;
	char dRes;
	unsigned char bRandGain;
};


int			order = 0;
int			ol = 0;
int		row = -1;
int		exitIntro = 0;
int			iM;//, row, order;
char		perxxorVars[40];
int			freqtable[12*9];
HANDLE		thH;


SAMPLE smp[NUM_SAMPLES] =	{
{2, 187  , 779, -2000, 107, -105, 77  , -74, 48 , -65 ,   0},
{1, 1000 , 0  ,     0,  68 , -67 , 253 , -49, 25 , -17 , 255},
{0, 8250 , 81 ,     0,  77 , -74 , 68  , -69, 84 , 12  , 0  },
{1, 1000 , 0  ,     0,  68 , -67 , 253 , -49, 25 , -17 , 255},
{0, 1000 , 220,     0, 100, -99 , 50  , -38, -31, 118 , 0  },
{2, 21250, 440,     0, 109, -108, 12  , 30 , 82 , -90 , 255},
{1, 3000 , 440,     0, 100, -99 , 93  , -90, -89, 41  , 0  },
{2, 500  , 110,     0,  53 , 0   , 102 , -81, -40, -123,  13}};

unsigned char channelList[29*17]={
6,197,0,0,0,197,0,200,0,0,0,0,0,0,0,195,0,
6,0,0,200,0,0,0,204,0,0,0,0,0,200,0,0,0,
6,0,0,200,0,0,0,209,0,0,0,0,0,200,0,0,0,
2,197,0,128,0,192,0,128,0,192,0,128,0,192,0,128,0,
2,193,0,128,0,192,0,128,0,192,0,128,0,192,0,128,0,
1,161,15,97,15,97,15,65,0,0,0,97,161,209,0,129,0,
7,15,0,197,0,15,0,197,67,133,0,197,0,15,0,197,0,
4,204,76,0,140,0,204,0,76,204,76,0,140,0,204,0,76,
4,200,72,0,136,0,200,0,72,200,72,0,136,0,200,0,72,
0,193,0,0,0,193,0,0,0,193,0,0,0,193,0,0,0,
1,0,0,0,0,209,15,0,81,15,145,15,0,209,15,0,0,
3,225,97,161,97,225,97,161,97,225,97,161,97,225,97,161,97,
2,213,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
2,209,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
7,15,0,193,0,15,0,193,67,129,0,193,0,15,0,193,0,
2,202,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
4,197,69,0,133,0,197,0,69,197,69,0,133,0,197,0,69,
7,15,0,202,0,15,0,202,72,138,0,202,0,15,0,202,0,
6,197,0,0,0,197,0,202,0,0,0,0,0,0,0,195,0,
6,0,0,202,0,0,0,209,0,0,0,0,0,202,0,0,0,
6,195,0,0,0,195,0,200,0,0,0,0,0,0,0,195,0,
2,200,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
4,195,67,0,131,0,195,0,67,195,67,0,131,0,195,0,67,
7,15,0,200,0,15,0,200,71,136,0,200,0,15,0,200,0,
2,204,64,192,64,192,64,192,64,192,0,0,64,192,64,192,64,
4,199,71,0,135,0,199,0,71,199,71,0,135,0,199,0,71,
7,15,0,204,0,15,0,204,81,140,0,204,0,15,0,204,0,
6,196,0,0,0,196,0,199,0,0,0,0,0,0,0,196,0,
6,0,0,199,0,0,0,204,0,0,0,0,0,199,0,0,0};
int		vols[]={-10000,-1200,-600,0};

char orderList[54]={
0,						//loading (0)
1,0,0,0,1,0,0,0,		//logo fuzzion (1-8 order)
1,0,0,0,				//pulpoide (9-12)
1,0,0,0,0,0,0,			//dacube 3 (13 - 19)
1,1,0,0,0,1,0,0,0,1,0,0,0, //pinxos (20
1,0,1,0,1,1,1,1,1,1,1,1,-12,0,0,0,0,0,0,-6,0};
unsigned char patternList[19 * 8]={
255,255,255,255,255, 255, 0,  1,
255,255,255,255,255,  0,  1,  3,
255,255,255,255,255,  0,  2,  4,
255,255,255,255,  0,  1,  3,  5,
255,  0,  1,  3,  4,  5,  6,  7,
255,255,255,255,255,  0,  1,  4,
  0,  1,  6,  7,  9, 10, 11, 12,
  0,  2,  8,  9, 10, 11, 13, 14,
  9, 10, 11, 15, 16, 17, 18, 19,
  1,  9, 10, 11, 20, 21, 22, 23,
  9, 10, 11, 24, 25, 26, 27, 28,
255,255,255,255,255,  0,  1,  5,
255,255,255,255,255,  1,  5, 20,
255,255,255,255,255,  5, 18, 19,
255,255,255,255,255,  5, 27, 28,
255,255,255,255,  0,  1,  5,  6,
255,255,255,255,  1,  5, 20,  23,
255,255,255,255,  5, 17, 18, 19,
255,255,255,255,  5, 26, 27, 28};

float PI =			  3.1415926535897932384626433832795f;

LPTHREAD_START_ROUTINE threadmain() {
	int i;
	int pt;
	int sample;
	int note, vol, oct;

	while(1) {
		row++;

		if(row > 15) {
			row = 0;
			//order 0 == loading
			if(order > 0) {
				ol += (char) orderList[order] << 3; //deltaencoding
				order++;
			}
			if(order > 54) {exitIntro = 1;}
		}

		//printf("%d, %d => %d\n", order, row, ol);
		for(i = 0; i < 8; i++) {
			pt = patternList[ol + i];
			if(pt == 255) continue;


			pt = (pt << 4) + pt; //17;	//33;
			
			sample = channelList[pt] * NCHANNELS + i;
			note = channelList[pt + row + 1];
			//tone = channelList[pt + row + 16 + 1];

			vol = (note >> 6) & 0x03;
			oct = (((note >> 4) & 0x03) + 4) * 12;
			note = (note & 0x0f);

			if(vol != 0) {
				dsamples[sample]->lpVtbl->SetVolume(dsamples[sample], vols[vol]);
			}
			if(note != 0) {	//!noteon
				dsamples[sample]->lpVtbl->Stop(dsamples[sample]);
				dsamples[sample]->lpVtbl->SetCurrentPosition(dsamples[sample], 0);

				if(note != 0xf) {	//!noteoff
					dsamples[sample]->lpVtbl->SetFrequency(dsamples[sample], freqtable[oct + note - 1]);
					dsamples[sample]->lpVtbl->Play(dsamples[sample], 0, 0, 0);
				}
			}

			/*
			if(note == 0) {	//noteon
				if(vol != 0) {
					dsamples[sample]->lpVtbl->SetVolume(dsamples[sample], vols[vol]);
				}
			} else {
				if(note == 0xf) {	//noteoff
					dsamples[sample]->lpVtbl->Stop(dsamples[sample]);
				} else {
					dsamples[sample]->lpVtbl->Stop(dsamples[sample]);
					dsamples[sample]->lpVtbl->SetCurrentPosition(dsamples[sample], 0);
					dsamples[sample]->lpVtbl->SetVolume(dsamples[sample], vols[vol]);
					dsamples[sample]->lpVtbl->SetFrequency(dsamples[sample], freqtable[oct + note]);
					dsamples[sample]->lpVtbl->Play(dsamples[sample], 0, 0, 0);
				}
			}
			*/
		}
		
		timeBeginPeriod (1);
		WaitForSingleObject(thH, BPM);
//		Sleep (BPM);
//		timeEndPeriod (1);
	}
}


void genSample(int sample, unsigned char *fb) {
	float fVal,fdVal, amp, damp, phase, buf0, buf1, lfc, lfb, f, q, dres, dcut;
	int	 iVal, i;
	signed short uVal;
	float val, tr;
	int ns;

	SAMPLE *s = smp + sample;

	ns = s->nSamples << 4;

	fVal = s->freqIni;
	fdVal = 1.f + s->dFreq / (16.f*256.f*256.f); 
	amp = s->ampIni / 100.f;
	damp = (0.01f * s->dAmp) / ns;
	buf0 = 0.f;
	buf1 = 0.f;
	f = s->cutIni * 85.f / 44100.f;
	q = s->resIni / 100.f;

	dres = (0.01f * s->dRes) / ns;
	dcut = (s->dCut * 85.f / 44100.f) / ns;
	phase = 0.f;

	for (i = 0; i < ns; i++) {
		phase += fVal;
		iVal = float2int(phase);
		uVal = iVal & 0xffff;
		tr = 2.f * uVal / 65536.f; //2.f*sVal/65536.f;

		if (s->wType) {
				tr = fsin(phase * 2 * PI / 44100.f);
		}

		if (s->wType == 2)	{
			if (tr > 0)	tr = 1.f;
			else	tr = -1.f;
		}

//		tr += s->bRandGain * (-1 +(rand ())/(0.5f*RAND_MAX))/127.f;
		tr += s->bRandGain * myRandFloat()/127.f;
		lfc = f;
		lfb = q + q / (1.0f - lfc);
		val = tr;
		buf0 += lfc * (val - buf0 + lfb * (buf0 - buf1));
		buf1 += lfc * (buf0 - buf1);

		tr = buf1; //(buf1 - (buf1*buf1*buf1)/6)*65536.f;
		f += dcut;
		q += dres;

		tr *= amp;
		fVal *= fdVal;
		//comentat pq cap sample te ddfreq.
		//fdVal *= 1.f + s->ddFreq / (256.f*65536.f*256.f); //*65536.f);
		amp += damp;


		tr *= 80.f;
		if(tr > 80.f) {
			tr = 80.f;
		}

		if(tr < -80.f) {
			tr = -80.f;
		}
		tr += 127;

		fb[i] = float2int(tr);
	}
}


const static int wavHeader[11] = {
    0x46464952, 
    MZK_NUMSAMPLESC*2+36, 
    0x45564157, 
    0x20746D66, 
    16, 
    WAVE_FORMAT_PCM|(MZK_NUMCHANNELS<<16), 
    MZK_RATE, 
    MZK_RATE*MZK_NUMCHANNELS*sizeof(short), 
    (MZK_NUMCHANNELS*sizeof(short))|((8*sizeof(short))<<16),
    0x61746164, 
    MZK_NUMSAMPLESC*sizeof(short)
    };

//==============================================================================================

static short myMuzik[MZK_NUMSAMPLESC + 22];

// put here your synth


long to;

int mzk_init_loader(HWND d_hWND)
{

	DirectSoundCreate (0, &dsound, 0);
	
	dsound->lpVtbl->SetCooperativeLevel(dsound, d_hWND, DSSCL_EXCLUSIVE | DSSCL_PRIORITY);

	bufdesc.dwSize=sizeof(DSBUFFERDESC);
	bufdesc.dwFlags=DSBCAPS_PRIMARYBUFFER|DSBCAPS_STICKYFOCUS;
	dsound->lpVtbl->CreateSoundBuffer(dsound, &bufdesc, &dprimary, 0);

	format.wFormatTag=WAVE_FORMAT_PCM;
	format.nChannels=1;
	format.nSamplesPerSec=44100;
	format.nAvgBytesPerSec=44100 * 1;
	format.nBlockAlign=1;
	format.wBitsPerSample=8;
	dprimary->lpVtbl->SetFormat(dprimary, &format);

	bufdesc.dwSize=sizeof(bufdesc);
	bufdesc.dwFlags=DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY |DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_STICKYFOCUS| DSBCAPS_STATIC;	
	bufdesc.lpwfxFormat=&format;
	for(icnt1 = 0; icnt1 < NUM_SAMPLES; icnt1++) {
		for(icnt2 = 0; icnt2 < NCHANNELS; icnt2++) {
			bufdesc.dwBufferBytes = (smp[icnt1].nSamples << 4);
			IDirectSound_CreateSoundBuffer(dsound, &bufdesc, &dsamples[icnt2 + icnt1 * NCHANNELS], NULL);
			dsamples[icnt2 + icnt1 * NCHANNELS]->lpVtbl->Lock(dsamples[icnt2 + icnt1 * NCHANNELS],0,0,&writebuf,(LPDWORD)&size,NULL,NULL,DSBLOCK_ENTIREBUFFER);
			genSample(icnt1, (unsigned char *)writebuf);
			dsamples[icnt2 + icnt1 * NCHANNELS]->lpVtbl->Unlock(dsamples[icnt2 + icnt1 * NCHANNELS], &writebuf,size,0,0);
			
		}
	}

	_asm{ 
		fld		dword ptr [cnt33k]
		mov		edi,offset freqtable
		mov		ecx,9*12
	generateFreq:
		fist	dword ptr [edi]
		add		edi, 4
		fmul	dword ptr [cnt105]
		loop	generateFreq
		fstp	st(0)
	}

	thH = CreateThread (0,0,(LPTHREAD_START_ROUTINE)threadmain,0,0,(LPDWORD)&tID);
	SetThreadPriority(thH,THREAD_PRIORITY_TIME_CRITICAL);

	return (1);

}

void mzk_start(void)
{
		//prevenir que el compilador ho optimitzi
	__asm {
		mov [row],dword ptr 0
		mov [order],dword ptr 1
	}
	to = timeGetTime();
}

void mzk_end(void)
{
    sndPlaySound( 0, 0 );
}

long mzk_play(void)
{
	return(timeGetTime() - to);
}
