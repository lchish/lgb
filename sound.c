#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_mixer.h>
#include <stdlib.h>

#include "sound.h"

#define SAMPLE_RATE 44100
#define CHANNELS 1
#define SAMPLES 1024

const int AMPLITUDE = 500;

Sound *sound;

static void update_frequency(const SoundChannel channel);


static Sint16 calc_channel_one(double time)
{
  Sint16 channel_value = 0;

  // sweep v2
  if(sound->channel_one->sweep_timer > // trigger event
	 sound->channel_one->sweep_length){
    ChannelOnePlayable *c1 = sound->channel_one;
    unsigned int shadow = c1->internal_frequency;
    c1->sweep_timer = 0.0;
    c1->internal_sweep_enabled = c1->sweep_shift != 0 || c1->sweep_time != 0;
    if(c1->sweep_shift != 0){
      unsigned int shadow_s2 = shadow >> c1->sweep_shift;
      if(sound->channel_one->sweep_inc_dec)
	shadow -= shadow_s2;
      else
	shadow += shadow_s2;
      if(shadow > 2047)
	c1->enabled = 0;
      if(c1->internal_sweep_enabled){
	if(shadow <= 2047){
	  c1->internal_frequency = shadow;
	  update_frequency(SOUND_CHANNEL_1);
	}
      }
    }
  }
  sound->channel_one->sweep_timer += 1.0 / (double)SAMPLE_RATE;

  //calculate volume
  if(sound->channel_one->volume_sweep_on){
    if(sound->channel_one->volume_sweep_time >
       sound->channel_one->volume_sweep_length){
      if(sound->channel_one->volume_inc_dec)
	sound->channel_one->volume =
	  (sound->channel_one->volume + 1) > 0xF ? 0xF :
	  (sound->channel_one->volume + 1);
      else
	sound->channel_one->volume =
	  (sound->channel_one->volume - 1) <= 0 ? 0 :
	  (sound->channel_one->volume - 1);
      sound->channel_one->volume_sweep_time = 0;
    }
    else
      sound->channel_one->volume_sweep_time += 1.0 / SAMPLE_RATE;
  }

  if(sound->channel_one->volume > 0 && sound->channel_one->enabled &&
     (!sound->channel_one->length_stop ||
      sound->channel_one->length > 0)){
    channel_value = (Sint16)
      (AMPLITUDE * sound->channel_one->volume *
       sin(2.0f * M_PI * (double)sound->channel_one->frequency *
	   time));
  }
  sound->channel_one->length -= 1.0 / (double)SAMPLE_RATE;

  sound->channel_one->enabled = 1;
  return channel_value;
}

static Sint16 calc_channel_two(double time)
{
  Sint16 channel_value = 0;

  //calculate volume
  if(sound->channel_two_playable->volume_sweep_on){
    if(sound->channel_two_playable->volume_sweep_time >
       sound->channel_two_playable->volume_sweep_length){
      if(sound->channel_two_playable->volume_inc_dec)
	sound->channel_two_playable->volume =
	  (sound->channel_two_playable->volume + 1) > 0xF ? 0xF :
	  (sound->channel_two_playable->volume + 1);
      else
	sound->channel_two_playable->volume =
	  (sound->channel_two_playable->volume - 1) <= 0 ? 0 :
	  (sound->channel_two_playable->volume - 1);
      sound->channel_two_playable->volume_sweep_time = 0;
      //printf("Vol %d\n",sound->channel_two_playable->volume);
    }
    else
      sound->channel_two_playable->volume_sweep_time += 1.0 / SAMPLE_RATE;
      }

  if(sound->channel_two_playable->volume > 0 &&
     (!sound->channel_two_playable->length_stop ||
      sound->channel_two_playable->length > 0)){
    //printf("c2 freq %d\n", sound->channel_two_playable->frequency);
    channel_value += (Sint16)
      (AMPLITUDE * sound->channel_two_playable->volume *
       sin(2.0f * M_PI * (double)sound->channel_two_playable->frequency *
	   time));
  }
  sound->channel_two_playable->length -= 1.0 / (double)SAMPLE_RATE;
  return channel_value;

}


static void sound_sdl_callback(void *data, Uint8 *raw_buffer, int bytes)
{
  Sint16 *buffer = (Sint16*)raw_buffer;
  int length = bytes / 2; // 2 bytes per sample for AUDIO_S16SYS
  Sound *sound = (Sound*)data;
  for(int i = 0; i < length; i++, sound->counter++)
    {
      double time = (double)sound->counter / (double)SAMPLE_RATE;
      buffer[i] = calc_channel_one(time) + calc_channel_two(time);
    }
}

void sound_init()
{
  SDL_Init(SDL_INIT_AUDIO);

  sound = malloc(sizeof(Sound));
  sound->wave = malloc(sizeof(Wave));
  sound->wave->wave_pattern = malloc(sizeof(u8) * 0x10); // 16 bytes
  sound->noise = malloc(sizeof(Noise));
  sound->control = malloc(sizeof(SoundControl));
  sound->channel_one = malloc(sizeof(ChannelOnePlayable));
  sound->channel_two_playable = malloc(sizeof(PlayableSound));
  sound->channel_three_playable = malloc(sizeof(PlayableSound));
  sound->channel_four_playable = malloc(sizeof(PlayableSound));
  sound->counter = 0;

  //sound->tone_sweep->sweep = 0x80;
  //sound->tone_sweep->sound_length = 0xBF;
  //sound->tone_sweep->volume_envelope = 0xF3;
  //sound->tone_sweep->frequency_low = 0;
  //sound->tone_sweep->frequency_high = 0xBF;

  //sound->tone->sound_length = 0x3F;
  //sound->tone->volume_envelope = 0;
  //sound->tone->frequency_low = 0;
  //sound->tone->frequency_high = 0xBF;

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

void sound_set_sweep(const u8 value)
{
  int sweep_time = ((value >> 4) & 7);
  sound->channel_one->sweep_time = sweep_time;
  sound->channel_one->sweep_length = sweep_time / 128.0;
  sound->channel_one->sweep_inc_dec = (value & 8) == 8; // 0 add, 1 sub
  sound->channel_one->sweep_shift = value & 7;
  /*printf("sweep! value %X sweep_time %d length %f increase_decrease %d sweep_shift %d\n",
    value,
    sweep_time,
    sound->channel_one->sweep_length,
    sound->channel_one->sweep_inc_dec,
    sound->channel_one->sweep_shift);*/
}

u8 sound_get_sweep() // only channel 1
{
  return (sound->channel_one->sweep_time & 7) << 4 |
    sound->channel_one->sweep_inc_dec << 3 |
    (sound->channel_one->sweep_shift & 7);
}

void sound_set_length(const SoundChannel channel, const u8 value)
{
  int wave_duty = (value >> 6) & 3;
  int sound_length = value & 0x3F;
  double sound_length_s = (64.0 - sound_length) * (1.0 / 256.0);
  //printf("Sound length %f\n", sound_length_s);
  switch(channel){
  case SOUND_CHANNEL_1:
    sound->channel_one->length = sound_length_s;
    sound->channel_one->wave_duty = wave_duty;
    break;
  case SOUND_CHANNEL_2:
    sound->channel_two_playable->length = sound_length_s;
    break;
  case SOUND_CHANNEL_3:
    {
    int sound_length = value;
    }
    break;
  case SOUND_CHANNEL_4:
    sound_length = value & 3;
    break;
  default:
    fprintf(stderr, "sound_set_length channel %d not used\n", channel);
  }
}

u8 sound_get_length_wave_pattern(const SoundChannel channel)
{
  switch(channel){
  case SOUND_CHANNEL_1:
    return sound->channel_one->wave_duty;
  case SOUND_CHANNEL_2:
    return sound->channel_two_playable->wave_duty;
  default:
    fprintf(stderr, "sound_get_length_wave_pattern channel %d not used\n", channel);
    return 0;
  }
}

void sound_set_volume(const SoundChannel channel, const u8 value)
{
  int initial_volume = (value >> 4) & 0x0F; // 0 is no sound
  int envelope_direction = value == 4; // 0 decrease, 1 increase
  int num_envelope_sweep = value & 7;
  switch(channel){
  case SOUND_CHANNEL_1:
    sound->channel_one->volume = initial_volume;
    sound->channel_one->volume_inc_dec = envelope_direction;
    sound->channel_one->volume_sweep_on = num_envelope_sweep != 0;
    sound->channel_one->internal_volume_sweep_length = num_envelope_sweep;
    sound->channel_one->volume_sweep_length = num_envelope_sweep *
      (1.0 / 64.0);
    sound->channel_one->volume_sweep_time = 0;
    break;
  case SOUND_CHANNEL_2:
    sound->channel_two_playable->volume = initial_volume;
    sound->channel_two_playable->volume_inc_dec = envelope_direction;
    sound->channel_two_playable->volume_sweep_on = num_envelope_sweep != 0;
    sound->channel_two_playable->internal_volume_sweep_length = num_envelope_sweep;
    sound->channel_two_playable->volume_sweep_length = num_envelope_sweep *
      (1.0 / 64.0);
    sound->channel_one->volume_sweep_time = 0;
    break;
  case SOUND_CHANNEL_4:
    {
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

u8 sound_get_volume(const SoundChannel channel)
{
  switch(channel){
  case SOUND_CHANNEL_1:
    return (sound->channel_one->volume & 0xF) << 4 |
      sound->channel_one->volume_inc_dec << 3 |
      (sound->channel_one->internal_volume_sweep_length & 7);
  case SOUND_CHANNEL_2:
    return (sound->channel_two_playable->volume & 0xF) << 4 |
      sound->channel_two_playable->volume_inc_dec << 3 |
      (sound->channel_two_playable->internal_volume_sweep_length & 7);
  default:
      printf("sound_get_volume_envelope channel %d not supported yet\n",
	     channel);
      return 0;
  }
}

static void update_frequency(const SoundChannel channel)
{
  switch(channel){
  case SOUND_CHANNEL_1:
    {
      double freq = 131072.0 /
	(2048.0 - sound->channel_one->internal_frequency);
      //printf("c1 new freq %f %d\n", freq, sound->channel_one->internal_frequency);
      sound->channel_one->frequency = freq;
      }
     break;
  case SOUND_CHANNEL_2:
    {
      double freq = 131072.0 /
	(2048.0 - sound->channel_two_playable->internal_frequency);
      //printf("c2 new freq %f\n", freq);
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
    sound->channel_one->internal_frequency &= 0x700; //zero low bits
    sound->channel_one->internal_frequency |= value & 0xFF;
    break;
  case SOUND_CHANNEL_2:
    sound->channel_two_playable->internal_frequency &= 0x700; //zero low bits
    sound->channel_two_playable->internal_frequency |= value & 0xFF;
    break;
  case SOUND_CHANNEL_3:
    {
    sound->wave->frequency_low = value;
    }
    break;
  default:
    fprintf(stderr, "sound_set_frequency_low channel %d not used\n", channel);
  }
}
void sound_set_frequency_high(const SoundChannel channel, const u8 value)
{
  int initial = (value & 0x80) == 0x80; // 1 = restart sound
  // 1 = stop output when length in NR11 sound_length stops
  int counter = (value & 0x40) == 0x40;
  int high_frequency = value & 7;
  switch(channel){
  case SOUND_CHANNEL_1:
    sound->channel_one->internal_frequency &= 0xFF; //zero high bits
    sound->channel_one->internal_frequency |= high_frequency << 8;
    sound->channel_one->length_stop = counter;
    break;
  case SOUND_CHANNEL_2:
    sound->channel_two_playable->internal_frequency &= 0xFF; //zero high bits
    sound->channel_two_playable->internal_frequency |= high_frequency << 8;
    sound->channel_two_playable->length_stop = counter;
    break;
  case SOUND_CHANNEL_3:
    sound->wave->frequency_high = high_frequency;
    break;
  default:
    fprintf(stderr, "sound_set_frequency_high channel %d not used\n", channel);
  }
  if(initial)
    update_frequency(channel);
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
  //printf("output_c1_to_s2 %d\n",output_c1_to_s2);
  int output_c4_to_s1 = (value & 8) == 8;
  int output_c3_to_s1 = (value & 4) == 4;
  int output_c2_to_s1= (value & 2) == 2;
  int output_c1_to_s1 = (value & 1) == 1;
  //printf("output_c1_to_s1 %d\n", output_c1_to_s1);
}

void sound_set_master_on_off(const u8 value)
{
  int master_on_off = (value & 0x80) == 0x80;
  if(master_on_off)
    SDL_PauseAudio(0);
  else
    SDL_PauseAudio(1);
}

u8 sound_get_frequency_low()
{
  printf("sound_get_frequency_low is write only\n");
  return 0;
}

u8 sound_get_frequency_high(const SoundChannel channel)
{
  switch(channel){
  case SOUND_CHANNEL_1:
    return sound->channel_one->length_stop << 6;
  case SOUND_CHANNEL_2:
    return sound->channel_two_playable->length_stop << 6;
  default:
      printf("sound_get_frequency_high channel %d not supported yet\n",
	     channel);
      return 0;
  }
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
