#include "sdk_cfg.h"
#include "music_ui.h"
#include "fat/tff.h"
#include "notice_player.h"

FS_DISP_NAME music_name_buf;//buf need malloc


MUSIC_DIS_VAR music_ui;
/* void *music_reverb; */
u8 play_mode = REPEAT_ALL;
u8 eq_mode = 0;
void music_enter_updata(void)
{
    /* echo_exit_api(&music_reverb); */
}

