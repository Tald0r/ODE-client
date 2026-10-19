//----------------------------------------------------------------------
// test_audiotypes.cpp
//----------------------------------------------------------------------
//
// Tests for the DirectSound-unit conversions in basic/AudioTypes.h.
//
// The game still expresses volume as DirectSound attenuation (hundredths
// of a dB, DSBVOLUME_MIN..DSBVOLUME_MAX) and pan as DSBPAN_LEFT..
// DSBPAN_RIGHT, while the SDL_mixer backend wants linear percentages.
// The sound adapter used to clamp the dB values straight into 0..100,
// which turned every setting the game ever passes (all negative) into
// silence. These tests pin the mapping the adapter now relies on.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "AudioTypes.h"

//----------------------------------------------------------------------
// Volume
//----------------------------------------------------------------------

TEST(AudioTypes, VolumeEndpointsAreFullScaleAndSilence)
{
	CHECK_EQ(100, AudioVolumeToPercent(DSBVOLUME_MAX));
	CHECK_EQ(0, AudioVolumeToPercent(DSBVOLUME_MIN));
}

TEST(AudioTypes, VolumeOutsideTheDirectSoundRangeIsClamped)
{
	CHECK_EQ(100, AudioVolumeToPercent(DSBVOLUME_MAX + 500));
	CHECK_EQ(0, AudioVolumeToPercent(DSBVOLUME_MIN - 5000));
}

TEST(AudioTypes, VolumeFollowsTheDecibelCurve)
{
	// -6 dB is half amplitude, -20 dB is a tenth.
	CHECK_EQ(50, AudioVolumeToPercent(-600));
	CHECK_EQ(10, AudioVolumeToPercent(-2000));
	// The quietest position of the in-game slider is -25 dB: quiet, not off.
	CHECK_EQ(6, AudioVolumeToPercent(-2500));
}

TEST(AudioTypes, VolumeIsMonotonicAcrossTheGameSlider)
{
	// UIMessageManager computes value*SOUND_DEGREE + SOUND_MIN for 0..15.
	int previous = -1;
	for (int value = 0; value < 16; ++value)
	{
		int percent = AudioVolumeToPercent(value * SOUND_DEGREE + SOUND_MIN);
		CHECK(percent > previous);
		previous = percent;
	}
	// The top of the slider lands within rounding of full scale.
	CHECK(previous >= 99);
}

TEST(AudioTypes, VolumeIsNeverSilentAboveTheSliderMinimum)
{
	// Distance attenuation subtracts (dist << 2) * VolumeSound from the
	// limit; a nearby sound at a mid slider setting must still be heard.
	CHECK(AudioVolumeToPercent(-2500 + 8 * SOUND_DEGREE - (3 << 2) * 8) > 0);
}

//----------------------------------------------------------------------
// Pan
//----------------------------------------------------------------------

TEST(AudioTypes, PanEndpointsAndCentre)
{
	CHECK_EQ(-100, AudioPanToPercent(DSBPAN_LEFT));
	CHECK_EQ(0, AudioPanToPercent(DSBPAN_CENTER));
	CHECK_EQ(100, AudioPanToPercent(DSBPAN_RIGHT));
}

TEST(AudioTypes, PanIsLinearAndClamped)
{
	CHECK_EQ(50, AudioPanToPercent(5000));
	CHECK_EQ(-50, AudioPanToPercent(-5000));
	// MZoneSoundManager passes (gap << 7); a gap of 3 tiles is a gentle pan.
	CHECK_EQ(3, AudioPanToPercent(3 << 7));
	CHECK_EQ(100, AudioPanToPercent(20000));
	CHECK_EQ(-100, AudioPanToPercent(-20000));
}
