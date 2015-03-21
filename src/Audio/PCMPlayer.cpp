/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2015 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "PCMPlayer.hpp"
#include "PCMSynthesiser.hpp"
#include "Util/Macros.hpp"
#include "LogFile.hpp"

#ifdef ANDROID
#include "SLES/Init.hpp"
#include "SLES/Engine.hpp"

#include <SLES/OpenSLES_Android.h>
#elif defined(WIN32)
#elif defined(ENABLE_ALSA)
#include <alsa/asoundlib.h>
#elif defined(ENABLE_SDL)
#include <SDL_audio.h>
#endif

#include <assert.h>

PCMPlayer::PCMPlayer():sample_rate(0), synthesiser(nullptr) {}

PCMPlayer::~PCMPlayer()
{
  Stop();
}

#ifdef ANDROID

/**
 * OpenSL/ES callback which gets invoked when a buffer has been
 * consumed.  It synthesises and enqueues the next buffer.
 */
static void
PlayedCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext)
{
  PCMPlayer *player = (PCMPlayer *)pContext;

  player->Enqueue();
}

#elif defined(WIN32)
#elif defined(ENABLE_ALSA)
static void
ALSA_callback (snd_async_handler_t *pcm_callback)
{
  snd_pcm_t *pcm_handle = snd_async_handler_get_pcm(pcm_callback);
  snd_pcm_sframes_t avail;
 // snd_pcm_uframes_t buffer_size;
  snd_pcm_uframes_t period_size;
  //snd_pcm_hw_params_t hw_params;


  PCMPlayer &player = *(PCMPlayer *)snd_async_handler_get_callback_private(pcm_callback);

//   buffer_size=player.buffer_size;
   period_size=player.period_size;

  /*snd_pcm_hw_params_current(pcm_handle, &hw_params);

  snd_pcm_hw_params_get_period_size( &hw_params,&period_size, NULL);
  snd_pcm_hw_params_get_buffer_size( &hw_params,&buffer_size); 	
*/
  
  if ((avail = snd_pcm_avail_update(pcm_handle)) < 0) {
    if (avail == -EPIPE) {
      /*underrun occured, reset the sound device*/ 					
      snd_pcm_prepare(pcm_handle);
    }
  }

  player.Synthesise((player.pcm_buffer), period_size);

  int i=0;
  while (avail >= (snd_pcm_sframes_t) period_size) {
    snd_pcm_writei(pcm_handle, player.pcm_buffer, period_size);
    i++;
    if ((avail = snd_pcm_avail_update(pcm_handle)) < 0) {
      if (avail == -EPIPE) {
        /*underrun occured, reset the sound device*/ 					
        snd_pcm_prepare(pcm_handle);
        printf("underrun\n");
      }
     }
  }	
	
}
#else

static void
Synthesise(void *udata, Uint8 *stream, int len)
{
  PCMPlayer &player = *(PCMPlayer *)udata;

  const size_t num_frames = len / 4;
  int16_t *stereo = (int16_t *)(void *)stream;
  int16_t *mono = stereo + num_frames, *end = mono + num_frames;

  player.Synthesise(mono, num_frames);

  while (mono != end) {
    int16_t sample = *mono++;
    *stereo++ = sample;
    *stereo++ = sample;
  }
}

#endif

bool
PCMPlayer::Start(PCMSynthesiser &_synthesiser, unsigned _sample_rate)
{
#ifdef ANDROID

  /* why, oh why is OpenSL/ES so complicated? */

  SLObjectItf _object;
  SLresult result = SLES::CreateEngine(&_object, 0, nullptr,
                                       0, nullptr, nullptr);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: slCreateEngine() result=%#x", (int)result);
    return false;
  }

  engine_object = SLES::Object(_object);

  result = engine_object.Realize(false);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: Engine.Realize() result=%#x", (int)result);
    engine_object.Destroy();
    return false;
  }

  SLEngineItf _engine;
  result = engine_object.GetInterface(*SLES::IID_ENGINE, &_engine);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: Engine.GetInterface(IID_ENGINE) result=%#x",
               (int)result);
    engine_object.Destroy();
    return false;
  }

  SLES::Engine engine(_engine);

  result = engine.CreateOutputMix(&_object, 0, nullptr, nullptr);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: CreateOutputMix() result=%#x", (int)result);
    engine_object.Destroy();
    return false;
  }

  mix_object = SLES::Object(_object);

  result = mix_object.Realize(false);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: Mix.Realize() result=%#x", (int)result);
    mix_object.Destroy();
    engine_object.Destroy();
    return false;
  }

  SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
    SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
    ARRAY_SIZE(buffers) - 1,
  };

  SLDataFormat_PCM format_pcm;
  format_pcm.formatType = SL_DATAFORMAT_PCM;
  format_pcm.numChannels = 1;
  /* from the Android NDK docs: "Note that the field samplesPerSec is
     actually in units of milliHz, despite the misleading name." */
  format_pcm.samplesPerSec = _sample_rate * 1000;
  format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  format_pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
  format_pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
  format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN; // XXX

  SLDataSource audioSrc = { &loc_bufq, &format_pcm };

  SLDataLocator_OutputMix loc_outmix = {
    SL_DATALOCATOR_OUTPUTMIX,
    mix_object,
  };

  SLDataSink audioSnk = {
    &loc_outmix,
    nullptr,
  };

  const SLInterfaceID ids2[] = {
    *SLES::IID_PLAY,
    *SLES::IID_ANDROIDSIMPLEBUFFERQUEUE,
  };
  static constexpr SLboolean req2[] = {
    SL_BOOLEAN_TRUE,
    SL_BOOLEAN_TRUE,
  };
  result = engine.CreateAudioPlayer(&_object, &audioSrc, &audioSnk,
                                    ARRAY_SIZE(ids2), ids2, req2);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: CreateAudioPlayer() result=%#x", (int)result);
    mix_object.Destroy();
    engine_object.Destroy();
    return false;
  }

  play_object = SLES::Object(_object);

  result = play_object.Realize(false);

  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: Play.Realize() result=%#x", (int)result);
    play_object.Destroy();
    mix_object.Destroy();
    engine_object.Destroy();
    return false;
  }

  SLPlayItf _play;
  result = play_object.GetInterface(*SLES::IID_PLAY, &_play);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: Play.GetInterface(IID_PLAY) result=%#x",
               (int)result);
    play_object.Destroy();
    mix_object.Destroy();
    engine_object.Destroy();
    return false;
  }

  play = SLES::Play(_play);

  SLAndroidSimpleBufferQueueItf _queue;
  result = play_object.GetInterface(*SLES::IID_ANDROIDSIMPLEBUFFERQUEUE,
                                    &_queue);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: Play.GetInterface(IID_ANDROIDSIMPLEBUFFERQUEUE) result=%#x",
               (int)result);
    play_object.Destroy();
    mix_object.Destroy();
    engine_object.Destroy();
    return false;
  }

  queue = SLES::AndroidSimpleBufferQueue(_queue);
  result = queue.RegisterCallback(PlayedCallback, (void *)this);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: Play.RegisterCallback() result=%#x", (int)result);
    play_object.Destroy();
    mix_object.Destroy();
    engine_object.Destroy();
    return false;
  }

  synthesiser = &_synthesiser;

  result = play.SetPlayState(SL_PLAYSTATE_PLAYING);
  if (result != SL_RESULT_SUCCESS) {
    LogFormat("PCMPlayer: Play.SetPlayState(PLAYING) result=%#x",
               (int)result);
    play_object.Destroy();
    mix_object.Destroy();
    engine_object.Destroy();
    synthesiser = nullptr;
    return false;
  }

  next = 0;
  filled = false;
  for (unsigned i = 0; i < ARRAY_SIZE(buffers) - 1; ++i)
    Enqueue();

  return true;
#elif defined(WIN32)
#elif defined(ENABLE_ALSA)
 if (synthesiser != nullptr) {
    if (_sample_rate == sample_rate) {
      /* already open, just change the synthesiser */
      snd_pcm_nonblock(pcm_handle,0);
      synthesiser = &_synthesiser;
      snd_pcm_nonblock(pcm_handle,1);
      return true;
    }

    Stop();
  }
  sample_rate = _sample_rate;


  if (snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0 ||
      snd_pcm_hw_params_malloc(&hw_params) < 0 ||   		
      snd_pcm_hw_params_any(pcm_handle, hw_params) < 0 ||
      snd_pcm_hw_params_set_access(pcm_handle, hw_params, 
                                   SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
      snd_pcm_hw_params_set_format(pcm_handle, hw_params, 
                                   SND_PCM_FORMAT_S16_LE) < 0 ||
      snd_pcm_hw_params_set_rate_near(pcm_handle, 
                                      hw_params, &sample_rate, 0) < 0 ||
      snd_pcm_hw_params_set_channels(pcm_handle, hw_params, 1) < 0 ||
      snd_pcm_hw_params_set_buffer_size_near (pcm_handle, hw_params, 
                                              &buffer_size) <0 ||
      snd_pcm_hw_params_set_period_size_near (pcm_handle, hw_params, 
                                              &period_size, NULL) <0 ||
      snd_pcm_hw_params (pcm_handle, hw_params) < 0) {
    return false;
  }
  snd_pcm_hw_params_free(hw_params);
  if (snd_pcm_sw_params_malloc(&sw_params) < 0 ||
      snd_pcm_sw_params_current(pcm_handle, sw_params) < 0 ||
      snd_pcm_sw_params_set_avail_min(pcm_handle, 
                                       sw_params, 128) < 0 ||
      snd_pcm_sw_params_set_start_threshold(pcm_handle, 
                                            sw_params, 0U) < 0 ||
      snd_pcm_sw_params(pcm_handle, sw_params)  < 0 ||
      snd_pcm_prepare(pcm_handle) < 0) {
    return false;
  }
  snd_pcm_sw_params_free(sw_params);
  
  for (int i=0;i<4096;i++)
     pcm_buffer[i]=0; //(short)(20*i);
  synthesiser = &_synthesiser;
  Synthesise(pcm_buffer,buffer_size);
  snd_pcm_writei(pcm_handle, pcm_buffer, 2*period_size);
  snd_pcm_pause(pcm_handle,0);
  //.....
  snd_async_add_pcm_handler(&pcm_callback, pcm_handle, ALSA_callback, (void *)this);
  snd_pcm_start(pcm_handle);
  printf("pcm playback started\n");
  return true;
#else
  if (synthesiser != nullptr) {
    if (_sample_rate == sample_rate) {
      /* already open, just change the synthesiser */
      SDL_LockAudio();
      synthesiser = &_synthesiser;
      SDL_UnlockAudio();
      return true;
    }

    Stop();
  }

  sample_rate = _sample_rate;

  SDL_AudioSpec spec;
  spec.freq = sample_rate;
  spec.format = AUDIO_S16SYS;
  spec.channels = 2;
  spec.samples = 4096;
  spec.callback = ::Synthesise;
  spec.userdata = this;

  if (SDL_OpenAudio(&spec, nullptr) < 0 )
    return false;

  synthesiser = &_synthesiser;
  SDL_PauseAudio(0);

  return true;
#endif
}

void
PCMPlayer::Stop()
{
#ifdef ANDROID
  if (synthesiser == nullptr)
    return;

  play.SetPlayState(SL_PLAYSTATE_PAUSED);
  play_object.Destroy();
  mix_object.Destroy();
  engine_object.Destroy();

  sample_rate = 0;
  synthesiser = nullptr;
#elif defined(WIN32)
#elif defined(ENABLE_ALSA)
  if (synthesiser == nullptr)
    return;
  snd_async_del_handler (pcm_callback); 
  snd_pcm_drop(pcm_handle);
  snd_pcm_close (pcm_handle);
  sample_rate = 0;
  synthesiser = nullptr;
#else
  if (synthesiser == nullptr)
    return;

  SDL_CloseAudio();
  sample_rate = 0;
  synthesiser = nullptr;
#endif
}

#ifdef ANDROID

void
PCMPlayer::Enqueue()
{
  assert(synthesiser != nullptr);

  ScopeLock protect(mutex);

  if (!filled) {
    filled = true;
    synthesiser->Synthesise(buffers[next], ARRAY_SIZE(buffers[next]));
  }

  SLresult result = queue.Enqueue(buffers[next], sizeof(buffers[next]));
  if (result == SL_RESULT_SUCCESS) {
    next = (next + 1) % ARRAY_SIZE(buffers);
    filled = false;
  }

  if (result != SL_RESULT_SUCCESS)
    LogFormat("PCMPlayer: Enqueue() result=%#x", (int)result);
}

#elif defined(WIN32)
#elif defined(ENABLE_ALSA)
void
PCMPlayer::Synthesise(void *buffer, size_t n)
{
  assert(synthesiser != nullptr);

  synthesiser->Synthesise((int16_t *)buffer, n);
}
#else

void
PCMPlayer::Synthesise(void *buffer, size_t n)
{
  assert(synthesiser != nullptr);

  synthesiser->Synthesise((int16_t *)buffer, n);
}

#endif
