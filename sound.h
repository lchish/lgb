#ifndef SOUND_H
#define SOUND_H

#include "types.h"

#define WAVE_SIZE 0x10

typedef struct{
  double frequency;
  double volume;
} PlayableSound;

typedef enum {
    SOUND_CHANNEL_1,
    SOUND_CHANNEL_2,
    SOUND_CHANNEL_3,
    SOUND_CHANNEL_4
}SoundChannel;

/* Sound Channel 1 */
typedef struct{
  u8 sweep; // NR10
  u8 sound_length; //AKA wave pattern duty NR11
  u8 volume_envelope; // NR12
  u8 frequency_low; // NR13
  u8 frequency_high; // NR14
} ToneAndSweep;

/* Sound Channel 2 */
typedef struct{
  u8 sound_length; //AKA wave pattern duty  NR21
  u8 volume_envelope; // NR22
  u8 frequency_low; // NR23
  u8 frequency_high; // NR24
} Tone;

/* Sound Channel 3 */
typedef struct{
  u8 sound_on_off; // NR30
  u8 sound_length; // NR31
  u8 output_level; // NR32
  u8 frequency_low; // NR33
  u8 frequency_high; //NR34
  u8 *wave_pattern; // Wave pattern ram
} Wave;

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
  ToneAndSweep *tone_sweep;
  Tone *tone;
  Wave *wave;
  Noise *noise;
  SoundControl *control;
  PlayableSound *channel_one_playable;
  PlayableSound *channel_two_playable;
  PlayableSound *channel_three_playable;
  PlayableSound *channel_four_playable;
  int counter;
} Sound;

extern Sound *sound;

void sound_init();
ToneAndSweep *sound_get_channel_1();
Tone *sound_get_channel_2();
Wave *sound_get_channel_3();
Noise *sound_get_channel_4();
SoundControl *sound_get_control();
void sound_set_wave_pattern(const u16 address, const u8 value);
u8 sound_get_wave_pattern(const u16 addres);

void sound_set_sweep(const SoundChannel channel, const u8 value);
void sound_set_length(const SoundChannel channel, const u8 value);
void sound_set_volume(const SoundChannel channel, const u8 value);
void sound_set_frequency_low(const SoundChannel channel, const u8 value);
void sound_set_frequency_high(const SoundChannel channel, const u8 value);
void sound_set_on_off(const SoundChannel channel, const u8 value);
void sound_set_polynomial_counter(const SoundChannel, const u8 value);
void sound_set_counter_consecutive(const SoundChannel, const u8 value);
void sound_set_channel_control(const u8 value);
void sound_set_output_terminal(const u8 value);
void sound_set_master_on_off(const u8 value);


#endif
