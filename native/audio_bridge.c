#include <jni.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

typedef int32_t aaudio_result_t;
typedef int32_t aaudio_direction_t;
typedef int32_t aaudio_format_t;
typedef void    AAudioStreamBuilder;
typedef void    AAudioStream;

#define AAUDIO_OK               0
#define AAUDIO_DIRECTION_INPUT  1
#define AAUDIO_FORMAT_PCM_I16   1

typedef aaudio_result_t (*fp_createBuilder)(AAudioStreamBuilder**);
typedef void            (*fp_setDirection)(AAudioStreamBuilder*, aaudio_direction_t);
typedef void            (*fp_setSampleRate)(AAudioStreamBuilder*, int32_t);
typedef void            (*fp_setChannelCount)(AAudioStreamBuilder*, int32_t);
typedef void            (*fp_setFormat)(AAudioStreamBuilder*, aaudio_format_t);
typedef void            (*fp_setBufferCapacity)(AAudioStreamBuilder*, int32_t);
typedef aaudio_result_t (*fp_openStream)(AAudioStreamBuilder*, AAudioStream**);
typedef aaudio_result_t (*fp_deleteBuilder)(AAudioStreamBuilder*);
typedef aaudio_result_t (*fp_requestStart)(AAudioStream*);
typedef aaudio_result_t (*fp_requestStop)(AAudioStream*);
typedef aaudio_result_t (*fp_closeStream)(AAudioStream*);
typedef aaudio_result_t (*fp_read)(AAudioStream*, void*, int32_t, int64_t);

static void*         g_alib  = NULL;
static AAudioStream* g_input = NULL;

static fp_createBuilder     fn_createBuilder;
static fp_setDirection      fn_setDirection;
static fp_setSampleRate     fn_setSampleRate;
static fp_setChannelCount   fn_setChannelCount;
static fp_setFormat         fn_setFormat;
static fp_setBufferCapacity fn_setBufferCapacity;
static fp_openStream        fn_openStream;
static fp_deleteBuilder     fn_deleteBuilder;
static fp_requestStart      fn_requestStart;
static fp_requestStop       fn_requestStop;
static fp_closeStream       fn_closeStream;
static fp_read              fn_read;

static int load_aaudio() {
    if (g_alib) return 1;
    g_alib = dlopen("libaaudio.so", RTLD_NOW);
    if (!g_alib) return 0;
    fn_createBuilder     = dlsym(g_alib, "AAudio_createStreamBuilder");
    fn_setDirection      = dlsym(g_alib, "AAudioStreamBuilder_setDirection");
    fn_setSampleRate     = dlsym(g_alib, "AAudioStreamBuilder_setSampleRate");
    fn_setChannelCount   = dlsym(g_alib, "AAudioStreamBuilder_setChannelCount");
    fn_setFormat         = dlsym(g_alib, "AAudioStreamBuilder_setFormat");
    fn_setBufferCapacity = dlsym(g_alib, "AAudioStreamBuilder_setBufferCapacityInFrames");
    fn_openStream        = dlsym(g_alib, "AAudioStreamBuilder_openStream");
    fn_deleteBuilder     = dlsym(g_alib, "AAudioStreamBuilder_delete");
    fn_requestStart      = dlsym(g_alib, "AAudioStream_requestStart");
    fn_requestStop       = dlsym(g_alib, "AAudioStream_requestStop");
    fn_closeStream       = dlsym(g_alib, "AAudioStream_close");
    fn_read              = dlsym(g_alib, "AAudioStream_read");
    return fn_createBuilder != NULL && fn_read != NULL;
}

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudio_open(JNIEnv* e, jclass c, jint rate, jint frames) {
    if (!load_aaudio()) return -100;
    AAudioStreamBuilder* b = NULL;
    if (fn_createBuilder(&b) != AAUDIO_OK || !b) return -101;
    fn_setDirection(b, AAUDIO_DIRECTION_INPUT);
    fn_setSampleRate(b, rate);
    fn_setChannelCount(b, 1);
    fn_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    if (frames > 0) fn_setBufferCapacity(b, frames);
    aaudio_result_t r = fn_openStream(b, &g_input);
    fn_deleteBuilder(b);
    return (jint)r;
}

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudio_start(JNIEnv* e, jclass c) {
    return g_input ? (jint)fn_requestStart(g_input) : -1;
}

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudio_stop(JNIEnv* e, jclass c) {
    return g_input ? (jint)fn_requestStop(g_input) : -1;
}

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudio_close(JNIEnv* e, jclass c) {
    if (!g_input) return 0;
    aaudio_result_t r = fn_closeStream(g_input);
    g_input = NULL;
    return (jint)r;
}

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudio_read(JNIEnv* env, jclass c, jbyteArray buf, jint off, jint len) {
    if (!g_input) return -1;
    jbyte* d = (*env)->GetByteArrayElements(env, buf, NULL);
    if (!d) return -2;
    aaudio_result_t r = fn_read(g_input, d + off, len / 2, 1000000000LL);
    (*env)->ReleaseByteArrayElements(env, buf, d, 0);
    return r < 0 ? (jint)r : (jint)(r * 2);
}

#define MAX_OUTPUT_STREAMS 16
#define QUEUE_BUFFERS      4

typedef struct {
    SLObjectItf  engineObj;
    SLEngineItf  engine;
    SLObjectItf  mixObj;
    SLObjectItf  playerObj;
    SLPlayItf    play;
    SLAndroidSimpleBufferQueueItf bq;
    int32_t      channels;
    int32_t      frameSize;
    int16_t*     bufs[QUEUE_BUFFERS];
    int32_t      bufBytes;
    int32_t      writeBuf;
} OutputCtx;

static OutputCtx* g_outputs[MAX_OUTPUT_STREAMS];

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudioOutput_open(JNIEnv* e, jclass c, jint rate, jint channels, jint frames) {
    int slot = -1;
    for (int i = 0; i < MAX_OUTPUT_STREAMS; i++)
        if (!g_outputs[i]) { slot = i; break; }
    if (slot < 0) return -200;

    OutputCtx* ctx = (OutputCtx*)calloc(1, sizeof(OutputCtx));
    if (!ctx) return -201;
    ctx->channels  = channels > 0 ? channels : 2;
    ctx->frameSize = ctx->channels * 2;
    ctx->bufBytes  = (frames > 0 ? frames : 960) * ctx->frameSize;
    for (int i = 0; i < QUEUE_BUFFERS; i++) {
        ctx->bufs[i] = (int16_t*)calloc(1, ctx->bufBytes);
        if (!ctx->bufs[i]) { free(ctx); return -202; }
    }

    if (slCreateEngine(&ctx->engineObj, 0, NULL, 0, NULL, NULL) != SL_RESULT_SUCCESS) goto fail;
    if ((*ctx->engineObj)->Realize(ctx->engineObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) goto fail;
    if ((*ctx->engineObj)->GetInterface(ctx->engineObj, SL_IID_ENGINE, &ctx->engine) != SL_RESULT_SUCCESS) goto fail;

    if ((*ctx->engine)->CreateOutputMix(ctx->engine, &ctx->mixObj, 0, NULL, NULL) != SL_RESULT_SUCCESS) goto fail;
    if ((*ctx->mixObj)->Realize(ctx->mixObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) goto fail;

    {
        SLDataLocator_AndroidSimpleBufferQueue bqLoc = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, QUEUE_BUFFERS
        };
        SLDataFormat_PCM pcmFmt = {
            SL_DATAFORMAT_PCM,
            (SLuint32)ctx->channels,
            (SLuint32)(rate * 1000),
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            ctx->channels == 2
                ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT)
                : SL_SPEAKER_FRONT_CENTER,
            SL_BYTEORDER_LITTLEENDIAN
        };
        SLDataSource audioSrc = { &bqLoc, &pcmFmt };
        SLDataLocator_OutputMix outLoc = { SL_DATALOCATOR_OUTPUTMIX, ctx->mixObj };
        SLDataSink audioSnk = { &outLoc, NULL };
        const SLInterfaceID ids[1]  = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
        const SLboolean     reqs[1] = { SL_BOOLEAN_TRUE };

        if ((*ctx->engine)->CreateAudioPlayer(ctx->engine, &ctx->playerObj,
                &audioSrc, &audioSnk, 1, ids, reqs) != SL_RESULT_SUCCESS) goto fail;
    }

    if ((*ctx->playerObj)->Realize(ctx->playerObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) goto fail;
    if ((*ctx->playerObj)->GetInterface(ctx->playerObj, SL_IID_PLAY, &ctx->play) != SL_RESULT_SUCCESS) goto fail;
    if ((*ctx->playerObj)->GetInterface(ctx->playerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &ctx->bq) != SL_RESULT_SUCCESS) goto fail;

    g_outputs[slot] = ctx;
    return slot;

fail:
    if (ctx->playerObj) (*ctx->playerObj)->Destroy(ctx->playerObj);
    if (ctx->mixObj)    (*ctx->mixObj)->Destroy(ctx->mixObj);
    if (ctx->engineObj) (*ctx->engineObj)->Destroy(ctx->engineObj);
    for (int i = 0; i < QUEUE_BUFFERS; i++) free(ctx->bufs[i]);
    free(ctx);
    return -300;
}

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudioOutput_start(JNIEnv* e, jclass c, jint h) {
    if (h < 0 || h >= MAX_OUTPUT_STREAMS || !g_outputs[h]) return -1;
    return (jint)(*g_outputs[h]->play)->SetPlayState(g_outputs[h]->play, SL_PLAYSTATE_PLAYING);
}

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudioOutput_stop(JNIEnv* e, jclass c, jint h) {
    if (h < 0 || h >= MAX_OUTPUT_STREAMS || !g_outputs[h]) return -1;
    return (jint)(*g_outputs[h]->play)->SetPlayState(g_outputs[h]->play, SL_PLAYSTATE_STOPPED);
}

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudioOutput_close(JNIEnv* e, jclass c, jint h) {
    if (h < 0 || h >= MAX_OUTPUT_STREAMS || !g_outputs[h]) return 0;
    OutputCtx* ctx = g_outputs[h];
    (*ctx->playerObj)->Destroy(ctx->playerObj);
    (*ctx->mixObj)->Destroy(ctx->mixObj);
    (*ctx->engineObj)->Destroy(ctx->engineObj);
    for (int i = 0; i < QUEUE_BUFFERS; i++) free(ctx->bufs[i]);
    free(ctx);
    g_outputs[h] = NULL;
    return 0;
}

JNIEXPORT jint JNICALL
Java_de_maxhenkel_shim_NativeAudioOutput_write2(JNIEnv* env, jclass c,
                                                 jint h, jbyteArray buf,
                                                 jint off, jint len, jint frameSize) {
    if (h < 0 || h >= MAX_OUTPUT_STREAMS || !g_outputs[h]) return -1;
    OutputCtx* ctx = g_outputs[h];
    if (len <= 0) return 0;
    int copyLen = len < ctx->bufBytes ? len : ctx->bufBytes;
    int idx = ctx->writeBuf % QUEUE_BUFFERS;
    jbyte* d = (*env)->GetByteArrayElements(env, buf, NULL);
    if (!d) return -2;
    memcpy(ctx->bufs[idx], d + off, copyLen);
    (*env)->ReleaseByteArrayElements(env, buf, d, JNI_ABORT);
    SLresult r = (*ctx->bq)->Enqueue(ctx->bq, ctx->bufs[idx], (SLuint32)copyLen);
    if (r != SL_RESULT_SUCCESS) return -3;
    ctx->writeBuf++;
    return copyLen;
}
