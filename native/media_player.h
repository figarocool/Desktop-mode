#ifndef DM_MEDIA_PLAYER_H
#define DM_MEDIA_PLAYER_H

int dm_media_player_open(const char *path);
int dm_media_player_action(int action);
int dm_media_player_status(void);
int dm_media_player_time_ms(void);
int dm_media_player_duration_ms(void);
int dm_media_player_seek_ms(int milliseconds);
int dm_media_player_set_volume(int percent);
int dm_media_player_get_volume(void);
void dm_media_player_tick(void);
void dm_media_player_close(void);
void dm_media_player_metadata(char *artist, unsigned artist_capacity,
                              char *title, unsigned title_capacity);

enum { DM_MEDIA_PLAY = 1, DM_MEDIA_PAUSE, DM_MEDIA_STOP };
enum { DM_MEDIA_STOPPED = 0, DM_MEDIA_PLAYING, DM_MEDIA_PAUSED, DM_MEDIA_ERROR };

#endif
