#include "media_player.h"
#include "desktop_api.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdint.h>

static char media_artist[96],media_title[96];
static int media_volume=100,media_duration_ms;
static void meta_copy(char *out,size_t cap,const unsigned char *in,size_t len) {
    if(!cap)return;
    size_t n=0;
    while(n<len&&n+1<cap&&in[n]){unsigned char c=in[n++];out[n-1]=(c<32&&c!='\t')?' ': (char)c;}
    while(n&&out[n-1]==' ')n--;
    out[n]=0;
}
static unsigned be32(const unsigned char *p){return ((unsigned)p[0]<<24)|((unsigned)p[1]<<16)|((unsigned)p[2]<<8)|p[3];}
static unsigned le32(const unsigned char *p){return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static unsigned synchsafe(const unsigned char*p){return ((unsigned)(p[0]&127)<<21)|((unsigned)(p[1]&127)<<14)|((unsigned)(p[2]&127)<<7)|(p[3]&127);}
static void parse_id3(FILE *f,long start) {
    unsigned char h[10];if(fseek(f,start,SEEK_SET)||fread(h,1,10,f)!=10||memcmp(h,"ID3",3))return;
    unsigned version=h[3],remain=synchsafe(h+6);if(remain>1024*1024)return;
    long at=start+10;while(remain>6){unsigned char frame[10];if(fseek(f,at,SEEK_SET))break;
        size_t header=version==2?6u:10u;if(fread(frame,1,header,f)!=header)break;
        char id[5]={0};memcpy(id,frame,version==2?3u:4u);int empty=1;for(unsigned i=0;id[i];i++)if(id[i]!=' '&&id[i])empty=0;if(empty)break;
        unsigned size=version==2?((unsigned)frame[3]<<16)|((unsigned)frame[4]<<8)|frame[5]:version==4?synchsafe(frame+4):be32(frame+4);
        if(!size||size>remain-header)break;
        if((!strcmp(id,"TPE1")||!strcmp(id,"TP1")||!strcmp(id,"TIT2")||!strcmp(id,"TT2"))&&size>1&&size<4096){unsigned char text[4096];if(fread(text,1,size,f)!=size)break;char *dest=(!strcmp(id,"TPE1")||!strcmp(id,"TP1"))?media_artist:media_title;size_t cap=96,n=size-1;unsigned enc=text[0];if(enc==0||enc==3)meta_copy(dest,cap,text+1,n);else if(enc==1||enc==2){size_t o=0;int little=enc==1&&n>=2&&text[1]==0xff&&text[2]==0xfe;size_t i=(enc==1&&n>=2)?2:0;while(i+1<n&&o+1<cap){unsigned cp=little?(unsigned)text[1+i]|((unsigned)text[2+i]<<8):((unsigned)text[1+i]<<8)|text[2+i];i+=2;if(!cp)break;if(cp<128)dest[o++]=(char)cp;else if(cp<2048&&o+2<cap){dest[o++]=(char)(0xc0|(cp>>6));dest[o++]=(char)(0x80|(cp&63));}else if(o+3<cap){dest[o++]=(char)(0xe0|(cp>>12));dest[o++]=(char)(0x80|((cp>>6)&63));dest[o++]=(char)(0x80|(cp&63));}}dest[o]=0;}}
        at+=(long)header+size;remain-=header+size;
    }
}
static void parse_wave_info(FILE*f) {
    unsigned char h[12];if(fseek(f,0,SEEK_SET)||fread(h,1,12,f)!=12||memcmp(h,"RIFF",4)||memcmp(h+8,"WAVE",4))return;
    for(int chunks=0;chunks<2048;chunks++){unsigned char c[8];if(fread(c,1,8,f)!=8)break;unsigned size=le32(c+4);long next=ftell(f)+(long)size+(long)(size&1u);if(!memcmp(c,"LIST",4)&&size>=4&&size<1024*1024){unsigned char type[4],sub[8],value[96];if(fread(type,1,4,f)!=4)break;if(!memcmp(type,"INFO",4)){long end=ftell(f)+(long)size-4;while(ftell(f)>=0&&ftell(f)+8<=end){if(fread(sub,1,8,f)!=8)break;unsigned n=le32(sub+4);if(n>(unsigned)(end-ftell(f)))break;if(!memcmp(sub,"IART",4)||!memcmp(sub,"INAM",4)){size_t take=n<sizeof(value)?n:sizeof(value)-1;if(fread(value,1,take,f)!=take)break;value[take]=0;meta_copy(!memcmp(sub,"IART",4)?media_artist:media_title,96,value,take);if(n>take&&fseek(f,(long)(n-take),SEEK_CUR))break;}else if(fseek(f,(long)n,SEEK_CUR))break;if(n&1u)if(fseek(f,1,SEEK_CUR))break;}}}if(fseek(f,next,SEEK_SET))break;}
}
static void parse_id3v1(FILE*f) {
    if(media_artist[0]&&media_title[0])return;
    if(fseek(f,-128,SEEK_END))return;
    unsigned char tag[128];
    if(fread(tag,1,128,f)!=128||memcmp(tag,"TAG",3))return;
    if(!media_title[0])meta_copy(media_title,sizeof(media_title),tag+3,30);
    if(!media_artist[0])meta_copy(media_artist,sizeof(media_artist),tag+33,30);
}
static void read_media_metadata(const char*path) {
    media_artist[0]=media_title[0]=0;FILE*f=fopen(path,"rb");if(!f)return;unsigned char head[12];size_t n=fread(head,1,sizeof(head),f);if(n>=3&&!memcmp(head,"ID3",3))parse_id3(f,0);else if(n==12&&!memcmp(head,"RIFF",4))parse_wave_info(f);parse_id3v1(f);fclose(f);
}
void dm_media_player_metadata(char*artist,unsigned ac,char*title,unsigned tc){if(artist&&ac)snprintf(artist,ac,"%s",media_artist);if(title&&tc)snprintf(title,tc,"%s",media_title);}

#ifdef DESKTOP_PREVIEW
#include <SDL.h>
#include <SDL_mixer.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static Mix_Music *preview_music;
static SDL_AudioDeviceID preview_device;
static SNDFILE *preview_pcm_file;
static SF_INFO preview_pcm_info;
static uint64_t preview_pcm_queued_frames;
static int preview_pcm_eof;
static char preview_rmi_temp[128];
static int preview_state;
static char preview_error[96];
static const char *preview_path(const char *path) {
    static char mapped[1200];
    if (!strncmp(path,"ux0:/",5)) snprintf(mapped,sizeof(mapped),"native/desktop/demo/ux0/%s",path+5);
    else if (!strncmp(path,"ur0:/",5)) snprintf(mapped,sizeof(mapped),"native/desktop/demo/ur0/%s",path+5);
    else snprintf(mapped,sizeof(mapped),"%s",path);
    return mapped;
}
static int is_pcm_container(const char *path) {
    const char *dot=strrchr(path?path:"",'.');
    return dot&&(!dm_ascii_casecmp(dot,".aif")||!dm_ascii_casecmp(dot,".aiff")||!dm_ascii_casecmp(dot,".aifc")||!dm_ascii_casecmp(dot,".au")||!dm_ascii_casecmp(dot,".snd")||!dm_ascii_casecmp(dot,".voc"));
}
static int is_rmi(const char *path) { const char *dot=strrchr(path?path:"",'.');return dot&&!dm_ascii_casecmp(dot,".rmi"); }
static int unwrap_rmi(const char *source) {
    unsigned char header[12],chunk[8];FILE *in=fopen(source,"rb");if(!in)return -1;
    if(fread(header,1,sizeof(header),in)!=sizeof(header)||memcmp(header,"RIFF",4)||memcmp(header+8,"RMID",4)){fclose(in);return -1;}
    snprintf(preview_rmi_temp,sizeof(preview_rmi_temp),"/tmp/desktop-mode-rmi-%ld.mid",(long)getpid());FILE *out=fopen(preview_rmi_temp,"wb");if(!out){fclose(in);return -1;}
    int ok=-1;
    while(fread(chunk,1,sizeof(chunk),in)==sizeof(chunk)){
        uint32_t size=(uint32_t)chunk[4]|((uint32_t)chunk[5]<<8)|((uint32_t)chunk[6]<<16)|((uint32_t)chunk[7]<<24);
        if(size>64u*1024u*1024u)break;
        if(!memcmp(chunk,"data",4)){
            unsigned char buffer[8192];uint32_t remain=size;ok=0;
            while(remain){size_t n=remain<sizeof(buffer)?remain:sizeof(buffer);if(fread(buffer,1,n,in)!=n||fwrite(buffer,1,n,out)!=n){ok=-1;break;}remain-=(uint32_t)n;}
            break;
        }
        if(fseek(in,(long)(size+(size&1u)),SEEK_CUR)!=0)break;
    }
    fclose(in);if(fclose(out)!=0)ok=-1;if(ok<0){remove(preview_rmi_temp);preview_rmi_temp[0]=0;}return ok;
}
int dm_media_player_open(const char *path) {
    dm_media_player_close();
    if (!path || !*path) return -1;
    media_duration_ms=0;
    read_media_metadata(preview_path(path));
    if(is_pcm_container(path)){
        preview_pcm_file=sf_open(preview_path(path),SFM_READ,&preview_pcm_info);
        if(!preview_pcm_file||preview_pcm_info.channels<1||preview_pcm_info.channels>2||preview_pcm_info.samplerate<8000||preview_pcm_info.samplerate>192000){
            if(preview_pcm_file)sf_close(preview_pcm_file);
            preview_pcm_file=NULL;
            snprintf(preview_error,sizeof(preview_error),"AIFF/AU/VOC non leggibile o con parametri non supportati");preview_state=DM_MEDIA_ERROR;return -1;
        }
        SDL_AudioSpec wanted;SDL_zero(wanted);wanted.freq=preview_pcm_info.samplerate;wanted.format=AUDIO_S16SYS;wanted.channels=(Uint8)preview_pcm_info.channels;
        preview_device=SDL_OpenAudioDevice(NULL,0,&wanted,NULL,0);
        if(!preview_device){sf_close(preview_pcm_file);preview_pcm_file=NULL;snprintf(preview_error,sizeof(preview_error),"Uscita audio SDL non disponibile");preview_state=DM_MEDIA_ERROR;return -1;}
        preview_pcm_queued_frames=0;preview_pcm_eof=0;media_duration_ms=(int)(preview_pcm_info.frames*1000/preview_pcm_info.samplerate);preview_state=DM_MEDIA_PLAYING;preview_error[0]=0;dm_media_player_tick();SDL_PauseAudioDevice(preview_device,0);return 0;
    }
    /* SDL_mixer provides streaming decoders for compressed audio and MIDI.
       Initialize it only after the separate libsndfile PCM path above. */
    if (!Mix_QuerySpec(NULL,NULL,NULL) && Mix_OpenAudio(44100,AUDIO_S16SYS,2,2048)<0) {
        snprintf(preview_error,sizeof(preview_error),"Uscita audio SDL non disponibile");
        preview_state=DM_MEDIA_ERROR; return -1;
    }
    Mix_Init(MIX_INIT_FLAC|MIX_INIT_MOD|MIX_INIT_MP3|MIX_INIT_OGG|MIX_INIT_MID|MIX_INIT_OPUS);
    const char *load_path=preview_path(path);
    if(is_rmi(path)){if(unwrap_rmi(load_path)<0){snprintf(preview_error,sizeof(preview_error),"File RMI/RIFF MIDI non valido");preview_state=DM_MEDIA_ERROR;return -1;}load_path=preview_rmi_temp;}
    preview_music=Mix_LoadMUS(load_path);
    if (!preview_music || Mix_PlayMusic(preview_music,0)<0) {
        snprintf(preview_error,sizeof(preview_error),"Decoder non disponibile: %.60s",Mix_GetError());
        if(preview_music)Mix_FreeMusic(preview_music);
        preview_music=NULL;preview_state=DM_MEDIA_ERROR;return -1;
    }
    {double duration=Mix_MusicDuration(preview_music);if(duration>0&&duration<2147483.0)media_duration_ms=(int)(duration*1000.0);Mix_VolumeMusic(MIX_MAX_VOLUME*media_volume/100);}
    preview_state=DM_MEDIA_PLAYING;preview_error[0]=0;return 0;
}
int dm_media_player_action(int action) {
    if(preview_device){
        if(action==DM_MEDIA_PAUSE){SDL_PauseAudioDevice(preview_device,1);preview_state=DM_MEDIA_PAUSED;return 0;}
        if(action==DM_MEDIA_PLAY){SDL_PauseAudioDevice(preview_device,0);preview_state=DM_MEDIA_PLAYING;return 0;}
        if(action==DM_MEDIA_STOP){dm_media_player_close();return 0;}return -1;
    }
    if (!preview_music) return -1;
    if (action==DM_MEDIA_PAUSE) { Mix_PauseMusic(); preview_state=DM_MEDIA_PAUSED; }
    else if (action==DM_MEDIA_PLAY) { if(!Mix_PlayingMusic())Mix_PlayMusic(preview_music,0);else Mix_ResumeMusic(); preview_state=DM_MEDIA_PLAYING; }
    else if (action==DM_MEDIA_STOP) dm_media_player_close();
    else return -1;
    return 0;
}
int dm_media_player_status(void) { if(preview_music&&preview_state==DM_MEDIA_PLAYING&&!Mix_PlayingMusic())preview_state=DM_MEDIA_STOPPED;if(preview_device&&preview_pcm_eof&&SDL_GetQueuedAudioSize(preview_device)==0)preview_state=DM_MEDIA_STOPPED;return preview_state; }
int dm_media_player_time_ms(void) { if(preview_device&&preview_pcm_info.samplerate>0){uint64_t queued=SDL_GetQueuedAudioSize(preview_device)/(2u*(unsigned)preview_pcm_info.channels);uint64_t done=preview_pcm_queued_frames>queued?preview_pcm_queued_frames-queued:0;return (int)(done*1000u/(unsigned)preview_pcm_info.samplerate);}double seconds=preview_music?Mix_GetMusicPosition(preview_music):-1.0;return seconds>=0.0?(int)(seconds*1000.0):0; }
int dm_media_player_duration_ms(void){return media_duration_ms;}
int dm_media_player_seek_ms(int ms){if(ms<0)ms=0;if(media_duration_ms>0&&ms>media_duration_ms)ms=media_duration_ms;if(preview_device&&preview_pcm_file&&preview_pcm_info.samplerate>0){sf_count_t frame=(sf_count_t)((int64_t)ms*preview_pcm_info.samplerate/1000);if(sf_seek(preview_pcm_file,frame,SEEK_SET)<0)return -1;SDL_ClearQueuedAudio(preview_device);preview_pcm_queued_frames=(uint64_t)frame;preview_pcm_eof=0;return 0;}if(preview_music)return Mix_SetMusicPosition((double)ms/1000.0);return -1;}
int dm_media_player_set_volume(int percent){if(percent<0||percent>100)return -1;media_volume=percent;if(preview_music)Mix_VolumeMusic(MIX_MAX_VOLUME*percent/100);return 0;}
int dm_media_player_get_volume(void){return media_volume;}
void dm_media_player_tick(void) { if(!preview_device||!preview_pcm_file||preview_pcm_eof||SDL_GetQueuedAudioSize(preview_device)>32768u)return;int16_t samples[8192*2];sf_count_t got=sf_readf_short(preview_pcm_file,samples,8192);if(got<=0){preview_pcm_eof=1;return;}for(sf_count_t i=0;i<got*preview_pcm_info.channels;i++)samples[i]=(int16_t)((int32_t)samples[i]*media_volume/100);if(SDL_QueueAudio(preview_device,samples,(Uint32)(got*preview_pcm_info.channels*2))<0)preview_pcm_eof=1;else preview_pcm_queued_frames+=(uint64_t)got; }
void dm_media_player_close(void) { if(preview_music){Mix_HaltMusic();Mix_FreeMusic(preview_music);preview_music=NULL;}if(preview_device){SDL_ClearQueuedAudio(preview_device);SDL_CloseAudioDevice(preview_device);preview_device=0;}if(preview_pcm_file){sf_close(preview_pcm_file);preview_pcm_file=NULL;}if(preview_rmi_temp[0]){remove(preview_rmi_temp);preview_rmi_temp[0]=0;}memset(&preview_pcm_info,0,sizeof(preview_pcm_info));preview_pcm_queued_frames=0;preview_pcm_eof=0;preview_state=DM_MEDIA_STOPPED; }
#else
#include <psp2/avplayer.h>
#include <psp2/audioout.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/types.h>
#include <stdio.h>
#ifdef DM_HAVE_VITA_SNDFILE
#include <sndfile.h>
#endif
static SceAvPlayerHandle player=-1;
static int audio_port=-1,player_state;
static int output_rate,output_channels,output_length;
static int module_loaded;
static int16_t output_buffer[32768*2] __attribute__((aligned(64)));
#ifdef DM_HAVE_VITA_SNDFILE
static SNDFILE *vita_pcm_file;
static SF_INFO vita_pcm_info;
static uint64_t vita_pcm_frames;
static int vita_pcm_eof;
static int vita_pcm_extension(const char *path){const char*dot=strrchr(path?path:"",'.');return dot&&(!dm_ascii_casecmp(dot,".aif")||!dm_ascii_casecmp(dot,".aiff")||!dm_ascii_casecmp(dot,".aifc")||!dm_ascii_casecmp(dot,".au")||!dm_ascii_casecmp(dot,".snd")||!dm_ascii_casecmp(dot,".voc"));}
#endif

static int supported_path(const char *path) {
    const char *dot=strrchr(path?path:"",'.');
    return dot&&(!dm_ascii_casecmp(dot,".wav")||!dm_ascii_casecmp(dot,".mp3")||!dm_ascii_casecmp(dot,".m4a")||!dm_ascii_casecmp(dot,".mp4")||!dm_ascii_casecmp(dot,".wma")||!dm_ascii_casecmp(dot,".asf")||!dm_ascii_casecmp(dot,".avi"));
}
int dm_media_player_open(const char *path) {
    dm_media_player_close();
    if(!path) { player_state=DM_MEDIA_ERROR;return -1; }
#ifdef DM_HAVE_VITA_SNDFILE
    if(vita_pcm_extension(path)){
        vita_pcm_info=(SF_INFO){0};vita_pcm_file=sf_open(path,SFM_READ,&vita_pcm_info);
        if(!vita_pcm_file||vita_pcm_info.channels<1||vita_pcm_info.channels>2||vita_pcm_info.samplerate<8000||vita_pcm_info.samplerate>192000){if(vita_pcm_file)sf_close(vita_pcm_file);vita_pcm_file=NULL;player_state=DM_MEDIA_ERROR;return -1;}
        read_media_metadata(path);vita_pcm_frames=0;vita_pcm_eof=0;player_state=DM_MEDIA_PLAYING;return 0;
    }
#endif
    if(!supported_path(path)) { player_state=DM_MEDIA_ERROR;return -1; }
    read_media_metadata(path);
    if(sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER)<0) { player_state=DM_MEDIA_ERROR;return -1; }module_loaded=1;
    SceAvPlayerInitData init;memset(&init,0,sizeof(init));init.autoStart=SCE_FALSE;init.numOutputVideoFrameBuffers=2;init.basePriority=0x10000100;
    player=sceAvPlayerInit(&init);
    if(player<0||sceAvPlayerAddSource(player,path)<0||sceAvPlayerStart(player)<0) { dm_media_player_close();player_state=DM_MEDIA_ERROR;return -1; }
    {SceAvPlayerStreamInfo info;memset(&info,0,sizeof(info));if(sceAvPlayerGetStreamInfo(player,0,&info)>=0&&info.duration<2147483648ULL)media_duration_ms=(int)info.duration;}
    player_state=DM_MEDIA_PLAYING;return 0;
}
int dm_media_player_action(int action) {
#ifdef DM_HAVE_VITA_SNDFILE
    if(vita_pcm_file){if(action==DM_MEDIA_PAUSE){player_state=DM_MEDIA_PAUSED;return 0;}if(action==DM_MEDIA_PLAY){player_state=DM_MEDIA_PLAYING;return 0;}if(action==DM_MEDIA_STOP){dm_media_player_close();return 0;}return -1;}
#endif
    if(player<0)return -1;
    int result=0;
    if(action==DM_MEDIA_PAUSE){result=sceAvPlayerPause(player);if(result>=0)player_state=DM_MEDIA_PAUSED;}
    else if(action==DM_MEDIA_PLAY){result=sceAvPlayerResume(player);if(result>=0)player_state=DM_MEDIA_PLAYING;}
    else if(action==DM_MEDIA_STOP){dm_media_player_close();}
    else result=-1;
    return result;
}
int dm_media_player_status(void) {
#ifdef DM_HAVE_VITA_SNDFILE
    if(vita_pcm_file&&vita_pcm_eof)player_state=DM_MEDIA_STOPPED;
#endif
    if(player>=0&&!sceAvPlayerIsActive(player))player_state=DM_MEDIA_STOPPED;
    return player_state;
}
int dm_media_player_time_ms(void) {
#ifdef DM_HAVE_VITA_SNDFILE
    if(vita_pcm_file&&vita_pcm_info.samplerate>0)return (int)(vita_pcm_frames*1000u/(unsigned)vita_pcm_info.samplerate);
#endif
    return player>=0?(int)sceAvPlayerCurrentTime(player):0;
}
int dm_media_player_duration_ms(void){
#ifdef DM_HAVE_VITA_SNDFILE
    if(vita_pcm_file&&vita_pcm_info.samplerate>0)return (int)(vita_pcm_info.frames*1000/vita_pcm_info.samplerate);
#endif
    return media_duration_ms;
}
int dm_media_player_seek_ms(int ms){if(ms<0)ms=0;if(media_duration_ms>0&&ms>media_duration_ms)ms=media_duration_ms;
#ifdef DM_HAVE_VITA_SNDFILE
    if(vita_pcm_file&&vita_pcm_info.samplerate>0){sf_count_t frame=(sf_count_t)((int64_t)ms*vita_pcm_info.samplerate/1000);if(sf_seek(vita_pcm_file,frame,SEEK_SET)<0)return -1;vita_pcm_frames=(uint64_t)frame;vita_pcm_eof=0;return 0;}
#endif
    return player>=0?sceAvPlayerJumpToTime(player,(uint64_t)ms):-1;
}
int dm_media_player_set_volume(int percent){if(percent<0||percent>100)return -1;media_volume=percent;return 0;}
int dm_media_player_get_volume(void){return media_volume;}
void dm_media_player_tick(void) {
#ifdef DM_HAVE_VITA_SNDFILE
    if(vita_pcm_file&&player_state==DM_MEDIA_PLAYING&&!vita_pcm_eof){
        int16_t pcm[2048*2];sf_count_t got=sf_readf_short(vita_pcm_file,pcm,2048);if(got<=0){vita_pcm_eof=1;return;}
        int frames=(int)got,port_length=(frames+63)&~63;if(port_length<SCE_AUDIO_MIN_LEN)port_length=SCE_AUDIO_MIN_LEN;if(port_length>SCE_AUDIO_MAX_LEN)return;
        if(vita_pcm_info.samplerate!=output_rate||vita_pcm_info.channels!=output_channels||port_length!=output_length){if(audio_port>=0)sceAudioOutReleasePort(audio_port);audio_port=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM,port_length,vita_pcm_info.samplerate,vita_pcm_info.channels==1?SCE_AUDIO_OUT_MODE_MONO:SCE_AUDIO_OUT_MODE_STEREO);output_rate=vita_pcm_info.samplerate;output_channels=vita_pcm_info.channels;output_length=port_length;}
        if(audio_port>=0){int samples=port_length*vita_pcm_info.channels;memset(output_buffer,0,(size_t)samples*sizeof(output_buffer[0]));memcpy(output_buffer,pcm,(size_t)frames*vita_pcm_info.channels*sizeof(pcm[0]));for(int i=0;i<frames*vita_pcm_info.channels;i++)output_buffer[i]=(int16_t)((int32_t)output_buffer[i]*media_volume/100);sceAudioOutOutput(audio_port,output_buffer);vita_pcm_frames+=(uint64_t)frames;}return;
    }
#endif
    if(player<0||player_state!=DM_MEDIA_PLAYING)return;
    SceAvPlayerFrameInfo frame;
    if(!sceAvPlayerGetAudioData(player,&frame)||!frame.pData||!frame.details.audio.sampleRate||!frame.details.audio.channelCount||!frame.details.audio.size)return;
    int channels=frame.details.audio.channelCount,rate=(int)frame.details.audio.sampleRate;
    int frames=(int)(frame.details.audio.size/(2u*(unsigned)channels));
    int port_length=(frames+63)&~63;
    if(port_length<SCE_AUDIO_MIN_LEN)port_length=SCE_AUDIO_MIN_LEN;
    if(port_length>SCE_AUDIO_MAX_LEN)return;
    if(rate!=output_rate||channels!=output_channels||port_length!=output_length){if(audio_port>=0)sceAudioOutReleasePort(audio_port);audio_port=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM,port_length,rate,channels==1?SCE_AUDIO_OUT_MODE_MONO:SCE_AUDIO_OUT_MODE_STEREO);output_rate=rate;output_channels=channels;output_length=port_length;}
    if(audio_port>=0&&frames>0){int samples=port_length*channels;if(samples>(int)(sizeof(output_buffer)/sizeof(output_buffer[0])))return;memset(output_buffer,0,(size_t)samples*sizeof(output_buffer[0]));int copy_frames=frames<port_length?frames:port_length;memcpy(output_buffer,frame.pData,(size_t)copy_frames*channels*sizeof(output_buffer[0]));for(int i=0;i<copy_frames*channels;i++)output_buffer[i]=(int16_t)((int32_t)output_buffer[i]*media_volume/100);sceAudioOutOutput(audio_port,output_buffer);}
}
void dm_media_player_close(void) {
#ifdef DM_HAVE_VITA_SNDFILE
    if(vita_pcm_file){sf_close(vita_pcm_file);vita_pcm_file=NULL;}
    vita_pcm_info=(SF_INFO){0};vita_pcm_frames=0;vita_pcm_eof=0;
#endif
    if(player>=0){sceAvPlayerStop(player);sceAvPlayerClose(player);player=-1;}
    if(audio_port>=0){sceAudioOutOutput(audio_port,NULL);sceAudioOutReleasePort(audio_port);audio_port=-1;}
    if(module_loaded){sceSysmoduleUnloadModule(SCE_SYSMODULE_AVPLAYER);module_loaded=0;}
    output_rate=output_channels=output_length=0;player_state=DM_MEDIA_STOPPED;
}
#endif
