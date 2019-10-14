#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_mixer.h>
#include <stdlib.h>

#include "sound.h"

#define SAMPLE_RATE 44100
#define CHANNELS 2
#define SAMPLES 2048

const int AMPLITUDE = 500;

Sound *sound;

void sound_sdl_callback(void *data, Uint8 *raw_buffer, int bytes)
{
  Sint16 *buffer = (Sint16*)raw_buffer;
  int length = bytes / 2; // 2 bytes per sample for AUDIO_S16SYS
  Sound *sound = (Sound*)data;
  for(int i = 0; i < length; i++, sound->counter++)
    {
      double time = (double)sound->counter / (double)SAMPLE_RATE;
      //channel 1
      Sint16 channel1 = (Sint16)
	(AMPLITUDE * sound->channel_one_playable->volume *
	 sin(2.0f * M_PI * (double)sound->channel_one_playable->frequency *
	     time));
      // channel 2
      Sint16 channel2  = (Sint16)
	(AMPLITUDE * sound->channel_two_playable->volume *
	 sin(M_PI * (double)sound->channel_two_playable->frequency *
	     time));
      buffer[i] = channel1 + channel2;
    }
}

void sound_init()
{
  SDL_Init(SDL_INIT_AUDIO);

  sound = malloc(sizeof(Sound));
  sound->tone_sweep = malloc(sizeof(ToneAndSweep));
  sound->tone = malloc(sizeof(Tone));
  sound->wave = malloc(sizeof(Wave));
  sound->wave->wave_pattern = malloc(sizeof(u8) * 0x10); // 16 bytes
  sound->noise = malloc(sizeof(Noise));
  sound->control = malloc(sizeof(SoundControl));
  sound->channel_one_playable = malloc(sizeof(PlayableSound));
  sound->channel_two_playable = malloc(sizeof(PlayableSound));
  sound->channel_three_playable = malloc(sizeof(PlayableSound));
  sound->channel_four_playable = malloc(sizeof(PlayableSound));
  sound->counter = 0;

  sound->tone_sweep->sweep = 0x80;
  sound->tone_sweep->sound_length = 0xBF;
  sound->tone_sweep->volume_envelope = 0xF3;
  sound->tone_sweep->frequency_low = 0;
  sound->tone_sweep->frequency_high = 0xBF;

  sound->tone->sound_length = 0x3F;
  sound->tone->volume_envelope = 0;
  sound->tone->frequency_low = 0;
  sound->tone->frequency_high = 0xBF;

  sound->wave->sound_on_off = 0x7F;
  sound->wave->sound_length = 0xFF;
  sound->wave->output_level = 0x9F;
  sound->wave->frequency_low = 0xBF;
  sound->wave->frequency_high = 0;
  sound->wave->wave_pattern[0] = 0;// TODO put wave pattern in

  sound->noise->sound_length = 0xFF;
  sound->noise->volume_envelope = 0;
  sound->noise->polynomial_counter = 0;
  sound->noise->counter_consecutive = 0xBF;

  sound->control->channel_control = 0x77;
  sound->control->output_terminal = 0xF3;
  sound->control->sound_on_off = 0xF1;

  // Start background sound program
  SDL_AudioSpec want;
  want.freq = SAMPLE_RATE; // number of samples per second
  want.format = AUDIO_S16SYS; // sample type (here: signed short i.e. 16 bit)
  want.channels = CHANNELS; // stereo audio
  want.samples = SAMPLES; // buffer-size
  want.callback = sound_sdl_callback; // function SDL calls periodically to refill the buffer
  want.userdata = sound; // callback data to refill buffer

  SDL_AudioSpec have;
  if(SDL_OpenAudio(&want, &have) != 0) SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to open audio: %s", SDL_GetError());
  if(want.format != have.format) SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to get the desired AudioSpec");

}

void sound_set_sweep(const SoundChannel channel, const u8 value)
{
  switch(channel){
  case SOUND_CHANNEL_1:
    sound->tone_sweep->sweep = value;
    int sweep_time = (value >> 4) & 7; // 3 bit
    int increase_decrease = value == 8; // addition 0 subtraction 1
    int sweep_shift = value & 7; // 3 bit
    printf("sweep! time %d increase_decrease %d sweep_shift %d\n",
	   sweep_time, increase_decrease, sweep_shift);
    break;
  default:
    fprintf(stderr, "sound_set_sweep channel %d not used\n", channel);
  }
}

void sound_set_length(const SoundChannel channel, const u8 value)
{
  switch(channel){
  case SOUND_CHANNEL_1:
  case SOUND_CHANNEL_2:
    sound->tone_sweep->sound_length = value;
    int wave_duty = (value >> 6) & 3;
    int sound_length = value & 0x3F;
    printf("Sound length %d\n", sound_length);
    break;

    /*
     *  Bit 7-0 - Sound length (t1: 0 - 255)
     *
     * Sound Length = (256-t1)*(1/256) seconds
     * This value is used only if Bit 6 in NR34 is set.
     */
  case SOUND_CHANNEL_3:
    {
    int sound_length = value;
    }
    break;
    /*
     * FF20 - NR41 - Channel 4 Sound Length (R/W)
     *   Bit 5-0 - Sound length data (t1: 0-63)
     * Sound Length = (64-t1)*(1/256) seconds
     * The Length value is used only if Bit 6 in NR44 is set.
     */
  case SOUND_CHANNEL_4:
    sound_length = value & 3;
    break;
  default:
    fprintf(stderr, "sound_set_length channel %d not used\n", channel);
  }
}

void sound_set_volume(const SoundChannel channel, const u8 value)
{
  int initial_volume = (value >> 4) & 0x0F; // 0 is no sound
  int envelope_direction = value == 4; // 0 decrease, 1 increase
  int num_envelope_sweep = value & 7;
  switch(channel){
  case SOUND_CHANNEL_1:
    sound->channel_one_playable->volume = (double)initial_volume;
    break;
  case SOUND_CHANNEL_2:
    sound->channel_two_playable->volume = (double)initial_volume;
    break;
  case SOUND_CHANNEL_4:
    {
    printf("volume %d\n", initial_volume);
    }
    break;
  case SOUND_CHANNEL_3:
    {
      int volume = (value >> 5) & 0x03;
    }
    break;
  default:
    fprintf(stderr, "sound_set_volume channel %d not used\n", channel);

  }
}
static void update_frequency(const SoundChannel channel)
{
  switch(channel){
  case SOUND_CHANNEL_1:
    {
      double freq = 131072.0 / (2048.0 - (sound->tone->frequency_low + (sound->tone->frequency_high << 8)));
      printf("c1 new freq %f\n", freq);
      sound->channel_one_playable->frequency = freq;
      }
    break;
     break;
  case SOUND_CHANNEL_2:
    {
      double freq = 131072.0 / (2048.0 - (sound->tone->frequency_low + (sound->tone->frequency_high << 8)));
      printf("c2 freq %f\n", freq);
      sound->channel_two_playable->frequency = freq;
      }
    break;
  case SOUND_CHANNEL_3:
    break;
  default:
    fprintf(stderr, "sound.c static update_frequency channel %d not used\n", channel);
  }
}

void sound_set_frequency_low(const SoundChannel channel, const u8 value)
{
  switch(channel){
  case SOUND_CHANNEL_1:
     sound->tone_sweep->frequency_low = value;
     break;
  case SOUND_CHANNEL_2:
    sound->tone->frequency_low = value;
    break;
  case SOUND_CHANNEL_3:
    {
    sound->wave->frequency_low = value;
    }
    break;
  default:
    fprintf(stderr, "sound_set_frequency_low channel %d not used\n", channel);
  }
  update_frequency(channel);
}
void sound_set_frequency_high(const SoundChannel channel, const u8 value)
{
  int initial = (value & 0x80) == 0x80; // 1 = restart sound
  // 1 = stop output when length in NR11 sound_length stops
  int counter = (value & 0x40) == 0x40;
  int high_frequency = value & 7;
  switch(channel){
  case SOUND_CHANNEL_1:
    sound->tone_sweep->frequency_high = high_frequency;
    break;
  case SOUND_CHANNEL_2:
    sound->tone->frequency_high = high_frequency;
    break;
  case SOUND_CHANNEL_3:
    sound->wave->frequency_high = high_frequency;
    break;
  default:
    fprintf(stderr, "sound_set_frequency_high channel %d not used\n", channel);
  }
}

void sound_set_on_off(const SoundChannel channel, const u8 value)
{
  switch(channel){
  case SOUND_CHANNEL_3:
    {
      int sound_on = (value & 0x80) == 0x80; // 0 = stop 1 = playback
    }
  break;
  default:
    fprintf(stderr, "sound_set_on_off channel %d not used\n", channel);
  }
}

void sound_set_polynomial_counter(const SoundChannel channel, const u8 value)
{
  switch(channel){
    /*
       FF22 - NR43 - Channel 4 Polynomial Counter (R/W)
       The amplitude is randomly switched between high and low at the given
       frequency. A higher frequency will make the noise to appear 'softer'.
       When Bit 3 is set, the output will become more regular, and some
       frequencies will sound more like Tone than Noise.

       Bit 7-4 - Shift Clock Frequency (s)
       Bit 3   - Counter Step/Width (0=15 bits, 1=7 bits)
       Bit 2-0 - Dividing Ratio of Frequencies (r)
    */
  case SOUND_CHANNEL_4:
    {
    int shift_frequency = (value >> 4) & 0x0F;
    int counter_step = value == 4; // 0 decrease, 1 increase
    int frequency_divider_ratio = value & 7;
    }
    break;
  default:
    fprintf(stderr, "sound_set_polynomial_counter channel %d not used\n", channel);
  }
}
void sound_set_counter_consecutive(const SoundChannel channel, const u8 value)
{
  switch(channel){
    /*     FF23 - NR44 - Channel 4 Counter/consecutive; Inital (R/W)

     *  Bit 7   - Initial (1=Restart Sound)     (Write Only)
     *  Bit 6   - Counter/consecutive selection (Read/Write)
     *  (1=Stop output when length in NR41 expires) */
  case SOUND_CHANNEL_4:
    {
      int initial = (value & 0x80) == 0x80;
      int counter_consecutive = (value & 0x40) == 0x40;
    }
    break;
  default:
    fprintf(stderr, "sound_set_counter_consecutive channel %d not used\n", channel);
  }
}
void sound_set_channel_control(const u8 value)
{
  int enable_s2 = (value & 0x80) == 0x80;
  int s2_level = (value >> 4) & 7;
  int enable_s1 = (value & 8) == 8;
  int s1_level = value & 7;
}

void sound_set_output_terminal(const u8 value)
{
  int output_c4_to_s2 = (value & 0x80) == 0x80;
  int output_c3_to_s2 = (value & 0x40) == 0x40;
  int output_c2_to_s2 = (value & 0x20) == 0x20;
  int output_c1_to_s2 = (value & 0x10) == 0x10;
  int output_c4_to_s1 = (value & 8) == 8;
  int output_c3_to_s1 = (value & 4) == 4;
  int output_c2_to_s1= (value & 2) == 2;
  int output_c1_to_s1 = (value & 1) == 1;
}

void sound_set_master_on_off(const u8 value)
{
  int master_on_off = (value & 0x80) == 0x80;
  if(master_on_off)
    SDL_PauseAudio(0);
  else
    SDL_PauseAudio(1);
}

ToneAndSweep *sound_get_channel_1(){
  return sound->tone_sweep;
}

Tone *sound_get_channel_2(){
  return sound->tone;
}

Wave *sound_get_channel_3(){
  return sound->wave;
}

Noise *sound_get_channel_4(){
  return sound->noise;
}
SoundControl *sound_get_control(){
  return sound->control;
}
void sound_set_wave_pattern(const u16 address, const u8 value)
{
  sound->wave->wave_pattern[address - 0xFF30] = value;
}
u8 sound_get_wave_pattern(const u16 address)
{
  return sound->wave->wave_pattern[address - 0xFF30];
}