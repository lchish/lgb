#ifndef SOUND_H
#define SOUND_H

#include "types.h"

#define WAVE_SIZE 0x10

typedef struct{
  double frequency;
  double volume_sweep_length;
  double volume_sweep_time;
  double length;
  double frequency_sweep_length;
  double frequency_sweep_timer;
  unsigned int internal_frequency;
  int volume;
  int internal_volume_sweep_length;
  int length_stop;
  int volume_inc_dec;
  int volume_sweep_on;
  int frequency_sweep_inc_dec;
  int frequency_sweep_shift;
  int frequency_sweep_enabled;
  int frequency_sweep_time;
  int enabled;
  int wave_duty;
  int length_expired;
} ChannelOne;

typedef struct{
  double frequency;
  double volume_sweep_length;
  double volume_sweep_time;
  double length;
  unsigned int internal_frequency;
  int volume;
  int internal_volume_sweep_length;
  int length_stop;
  int volume_inc_dec;
  int volume_sweep_on;
  int wave_duty;
  int length_expired;
} PlayableSound;


typedef struct{
  double length;
  double frequency;
  int on_off;
  int volume;
  int internal_frequency;
  int length_stop;
  int internal_length;
  u8 *wave_pattern;
  int length_expired;
} ChannelThree;

typedef struct{
  double frequency;
  double frequency_counter;
  double length;
  double volume_sweep_length;
  double volume_sweep_time;
  Sint16 previous_noise_value;
  int internal_length;
  int volume;
  int volume_inc_dec; // envelope direction
  int internal_volume_sweep_length;
  int shift_frequency;
  int counter_step;
  int frequency_divider_ratio;
  int length_stop;
  int length_expired;
} ChannelFour;

typedef enum {
    SOUND_CHANNEL_1 = 1,
    SOUND_CHANNEL_2,
    SOUND_CHANNEL_3,
    SOUND_CHANNEL_4
}SoundChannel;

/* Sound Channel 4 */
typedef struct{
  u8 sound_length; //NR41
  u8 volume_envelope; // NR42
  u8 polynomial_counter; // NR43
  u8 counter_consecutive; // NR44
} Noise;

typedef struct{
  u8 channel_control; // NR50
  u8 output_terminal; // NR51
  u8 sound_on_off; // NR52
} SoundControl;

typedef struct{
  Noise *noise;
  SoundControl *control;
  ChannelOne *channel_one;
  PlayableSound *channel_two;
  ChannelThree *channel_three;
  ChannelFour *channel_four;
  int counter;
  SDL_mutex *mutex;

  int output_c4_to_s2;
  int output_c3_to_s2;
  int output_c2_to_s2;
  int output_c1_to_s2;
  int output_c4_to_s1;
  int output_c3_to_s1;
  int output_c2_to_s1;
  int output_c1_to_s1;

  int enable_s2_vin;
  int s2_volume;
  int enable_s1_vin;
  int s1_volume;

  int master_on_off;
} Sound;

extern Sound *sound;

void sound_init();
Noise *sound_get_channel_4();
void sound_set_wave_pattern(const u16 address, const u8 value);
u8 sound_get_wave_pattern(const u16 addres);

void sound_set_sweep(const u8 value);
void sound_set_length(const SoundChannel channel, const u8 value);
void sound_set_volume(const SoundChannel channel, const u8 value);
void sound_set_frequency_low(const SoundChannel channel, const u8 value);
void sound_set_frequency_high(const SoundChannel channel, const u8 value);
void sound_set_on_off(const SoundChannel channel, const u8 value);
void sound_set_polynomial_counter(const u8 value);
void sound_set_counter_consecutive(const SoundChannel, const u8 value);
void sound_set_channel_control(const u8 value);
void sound_set_output_terminal(const u8 value);
void sound_set_master_on_off(const u8 value);

u8 sound_get_sweep();
u8 sound_get_length_wave_pattern(const SoundChannel channelm);
u8 sound_get_volume(const SoundChannel channel);
u8 sound_get_frequency_low();
u8 sound_get_frequency_high(const SoundChannel channel);
u8 sound_get_channel_control();
u8 sound_get_output_terminal();
u8 sound_get_on_off(const SoundChannel channel);

#endif
