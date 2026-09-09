//--------------------------------------------------------------------------//
// iq / rgba  .  tiny codes  .  2008                                        //
//--------------------------------------------------------------------------//
#ifdef __cplusplus
extern "C" 
{
#endif
#ifndef _MZK_H_
#define _MZK_H_

#define MZK_DURATION    104
#define MZK_RATE        44100
#define MZK_NUMCHANNELS 2

#define MZK_NUMSAMPLES  (MZK_DURATION*MZK_RATE)
#define MZK_NUMSAMPLESC (MZK_NUMSAMPLES*MZK_NUMCHANNELS)

int mzk_init_loader(HWND dhWND);
void mzk_start(void);
void mzk_end(void);
long mzk_play(void);

extern int order;

#endif
#ifdef __cplusplus
}
#endif
