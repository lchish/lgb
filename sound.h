#ifndef SOUND_H
#define SOUND_H

#include "types.h"

#define WAVE_SIZE 0x10

typedef struct{
  double frequency;
  double volume_sweep_length;
  double volume_sweep_time;
  double length;
  double sweep_length;
  double sweep_timer;
  unsigned int internal_frequency;
  int volume;
  int internal_volume_sweep_length;
  int length_stop;
  int volume_inc_dec;
  int volume_sweep_on;
  int sweep_inc_dec;
  int sweep_shift;
  int internal_sweep_enabled;
  int sweep_time;
  int enabled;
  int wave_duty
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
} PlayableSound;

/*
This channel can be used to output digital sound, the length of the sample buffer (Wave RAM) is limited to 32 digits. This sound channel can be also used to output normal tones when initializing the Wave RAM by a square wave. This channel doesn't have a volume envelope register.
FF1A - NR30 - Channel 3 Sound on/off (R/W)
  Bit 7 - Sound Channel 3 Off  (0=Stop, 1=Playback)  (Read/Write)
FF1B - NR31 - Channel 3 Sound Length
  Bit 7-0 - Sound length (t1: 0 - 255)
Sound Length = (256-t1)*(1/256) seconds
This value is used only if Bit 6 in NR34 is set.
FF1C - NR32 - Channel 3 Select output level (R/W)
  Bit 6-5 - Select output level (Read/Write)
Possible Output levels are:
  0: Mute (No sound)
  1: 100% Volume (Produce Wave Pattern RAM Data as it is)
  2:  50% Volume (Produce Wave Pattern RAM data shifted once to the right)
  3:  25% Volume (Produce Wave Pattern RAM data shifted twice to the right)
FF1D - NR33 - Channel 3 Frequency's lower data (W)
Lower 8 bits of an 11 bit frequency (x).
FF1E - NR34 - Channel 3 Frequency's higher data (R/W)
  Bit 7   - Initial (1=Restart Sound)     (Write Only)
  Bit 6   - Counter/consecutive selection (Read/Write)
            (1=Stop output when length in NR31 expires)
  Bit 2-0 - Frequency's higher 3 bits (x) (Write Only)
Frequency = 4194304/(64*(2048-x)) Hz = 65536/(2048-x) Hz
FF30-FF3F - Wave Pattern RAM
Contents - Waveform storage for arbitrary sound data
This storage area holds 32 4-bit samples that are played back upper 4 bits first.
*/

typedef struct{
  double length;
  double frequency;
  int on_off;
  int volume;
  int internal_frequency;
  int length_stop;
  u8 *wave_pattern
} ChannelThree;

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
  PlayableSound *channel_two_playable;
  ChannelThree *channel_three;
  PlayableSound *channel_four_playable;
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
void sound_set_polynomial_counter(const SoundChannel, const u8 value);
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
u8 sound_get_sound_output_terminal();

#endif
