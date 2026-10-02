#include "MusicSelection.h"

MUSIC_ID SelectZoneMusic(int zoneID, std::uint8_t hour, bool holyLand, bool waveMusic, bool war)
{
	const MUSIC_ID musicByTime[] =
	{
		MUSIC_LIVE_OR_DEAD,
		MUSIC_WINDMILL,
		MUSIC_WINDMILL,
		MUSIC_TREASURE,
		MUSIC_MARCHING,
		MUSIC_WHISPER,
		MUSIC_HELL_KNIGHT,
		MUSIC_LUNATIC,
		MUSIC_SAY_AGAIN,
		MUSIC_HIDE_AWAY,
		MUSIC_HELL_KNIGHT,
		MUSIC_HOLLOWEEN,
	};
	MUSIC_ID selected;
	if (holyLand && waveMusic)
	{
		switch (zoneID)
		{
		case 1201: case 1211: case 1212:
		case 1205: case 1251: case 1252:
			selected = MUSIC_OCTAVUS;
			break;
		case 1202: case 1221: case 1222:
		case 1206: case 1261: case 1262:
			selected = MUSIC_TERTIUS;
			break;
		case 1203: case 1231: case 1232:
			selected = MUSIC_SEPTIMUS;
			break;
		case 1204: case 1241: case 1242:
			selected = MUSIC_QUARTUS;
			break;
		default:
			selected = war ? MUSIC_HOLYLAND_WAR : MUSIC_HOLYLAND;
			break;
		}
	}
	else
	{
		selected = musicByTime[(hour / 2) % 12];
	}
	// Lair tracks override both the time rotation and Holy Land selection.
	if (zoneID == 1410 || zoneID == 1411) selected = MUSIC_ILLUSIONS_WAY;
	else if (zoneID == 1412 || zoneID == 1413) selected = MUSIC_GDR_LAIR;
	return selected;
}
