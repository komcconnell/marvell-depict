/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <stdio.h>

#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <pthread.h>

#include <linux/fb.h>
#include <linux/kd.h>

#include "minui.h"
#include "graphics.h"

#include "OSAL_api.h"
#include "com_type.h"

#include "gfx_api.h"
#include "gfx_type.h"
#include "amp_client.h"
#include "amp_client_support.h"
#include "amp_buf_desc.h"
#include "isl/amp_event_listener.h"
#include "isl/amp_event_queue.h"

static gr_surface fbdev_init(minui_backend*);
static gr_surface fbdev_flip(minui_backend*);
static void fbdev_blank(minui_backend*, bool);
static void fbdev_exit(minui_backend*);

#define DEBUG_GFX 1

#ifdef RECOVERY_TV_DISP_OUTPUT
#define FB_WIDTH        1920
#define FB_HEIGHT       1080
#else
#define FB_WIDTH        1280
#define FB_HEIGHT        720
#endif

static minui_backend my_backend = {
    .init = fbdev_init,
    .flip = fbdev_flip,
    .blank = fbdev_blank,
    .exit = fbdev_exit,
};

minui_backend* open_fbdev() {
    return &my_backend;
}

static GRSurface gr_framebuffer[2];
static GRSurface gr_mem_surface;
static unsigned gr_active_fb = 0;

static int uiWidth = -1;
static int uiHeight = -1;
static int uiStride = -1;

AMP_FACTORY hFactory;
static AMP_DISP gDisp;
static void *gHndGFX;
static void* gHndSurface[2];
static void* gHndBlitSurface[2];
static AMP_SHM_HANDLE gBufHnds[2];

#define GFX_PIXEL_FORMAT MV_GFX_2D_PIX_X8R8G8B8
#define GFX_DISPLAY_FORMAT MV_GFX_2D_PIX_X8R8G8B8
#define PIXEL_SIZE 4

static pthread_mutex_t hotplug_mutex;
static pthread_cond_t hotplug_cond;
static pthread_t hotplug_thread;
static int quit_work_thread = 0;
static int hdmi_connected = 0;
HANDLE amp_event_listener_;

#if DEBUG_GFX
static int a_count=0;
#endif

static char *client_argv[] = {"client", "iiop:1.0//127.0.0.1:999/AMP::FACTORY/factory"};

HRESULT disp_event_callback(HANDLE hListener, AMP_EVENT *pEvent, VOID *pUserData)
{
    UINT32 *pPayLoad = (UINT32*)AMP_EVENT_PAYLOAD_PTR(pEvent);
    UINT32 code = AMP_EVENT_GETCODE (*pEvent);
    if (AMP_EVENT_API_DISP_CALLBACK_HDMI == code) {
        UINT32 hotplug_event = pPayLoad[0];
        switch (hotplug_event) {
            case AMP_DISP_EVENT_HDMI_SINK_CONNECTED:
                pthread_mutex_lock(&hotplug_mutex);
                quit_work_thread = 0;
                hdmi_connected = 1;
                pthread_cond_signal(&hotplug_cond);
                pthread_mutex_unlock(&hotplug_mutex);
                break;
            default:
                break;
        }
    }
    return S_OK;
}

static void *hotplug_handler_thread(void *cookie)
{
    HRESULT hres;
    while (1) {
        pthread_mutex_lock(&hotplug_mutex);

        while (!hdmi_connected && !quit_work_thread) {
            pthread_cond_wait(&hotplug_cond, &hotplug_mutex);
        }

        if (quit_work_thread) {
            pthread_mutex_unlock(&hotplug_mutex);
            break;
        }

        AMP_RPC(hres, AMP_DISP_OUT_SetResolution, gDisp, AMP_DISP_PLANE_MAIN,
               AMP_DISP_OUT_RES_720P60 | 0x80000000, AMP_DISP_OUT_BIT_DPE_8);

        AMP_RPC(hres, AMP_DISP_OUT_HDMI_SetVidFmt, gDisp, AMP_DISP_OUT_CLR_FMT_RGB888,
               AMP_DISP_OUT_BIT_DPE_8, 1);

        hdmi_connected = 0;
        pthread_mutex_unlock(&hotplug_mutex);
    }

    return NULL;
}

static void fbdev_blank(minui_backend* backend __unused, bool blank)
{
    perror("do not support blank");
}

static void set_active_framebuffer(unsigned n)
{
    MV_GFX_2D_RECT srcRect = {0, 0, FB_WIDTH, FB_HEIGHT};
    MV_GFX_2D_RECT dstRect = {0, 0, FB_WIDTH, FB_HEIGHT};

    if (n > 1) return;
#if DEBUG_GFX
    MV_GFX_2D_DisplayCel(gHndGFX, gHndSurface[n],
                        &srcRect, &dstRect, MV_GFX_2D_CHANNEL_GRAPHICS);
#else
    MV_GFX_2D_FillCel(gHndGFX, gHndBlitSurface[n], 0xff000000, &dstRect, &dstRect);
    MV_GFX_2D_Blit(gHndGFX, gHndSurface[n], NULL, gHndBlitSurface[n], NULL, NULL, 0xEE);
    MV_GFX_2D_Sync(gHndGFX, 1);
    MV_GFX_2D_DisplayCel(gHndGFX, gHndBlitSurface[n],
                        &srcRect, &dstRect, MV_GFX_2D_CHANNEL_GRAPHICS);
#endif
}

static void get_framebuffer(GRSurface *fb)
{
    MV_GFX_2D_CelInfo celInfo;
    void* surface;
    int ret;

    memset(&celInfo, 0x00, sizeof(MV_GFX_2D_CelInfo));
    celInfo.eType = MV_GFX_2D_BUFTYPE_CACHEDSHM;
    celInfo.uiWidth = FB_WIDTH;
    celInfo.uiHeight = FB_HEIGHT;
    celInfo.ePixelFormat = GFX_PIXEL_FORMAT;
    celInfo.uiShmKey = -1;

    // Allocate frame 1 for drawing
    ret = MV_GFX_2D_CreateCel(gHndGFX, &celInfo, &surface);
    if (ret ||(!surface)) {
        fprintf(stderr, "MV_GFX_2D_CreateCel() failed: error %x\n", ret);
        return;
    }

    ret = MV_GFX_2D_GetCelInfo(gHndGFX, surface, &celInfo);
    if (ret != S_OK) {
        fprintf(stderr, "MV_GFX_2D_CreateCel() failed: error %x\n", ret);
        return;
    }
    gHndSurface[0] = surface;
    memset((void *)celInfo.uiVirtualAddr, 0XFF, celInfo.uiSize);

    gBufHnds[0] = celInfo.uiShmKey;

    fb->width = celInfo.uiWidth;
    fb->height = celInfo.uiHeight;
    fb->row_bytes = celInfo.uiStride;
    fb->pixel_bytes = PIXEL_SIZE;
    fb->data = (void *)celInfo.uiVirtualAddr;

    fb++;

    // Allocate frame 2 for drawing
    ret = MV_GFX_2D_CreateCel(gHndGFX, &celInfo, &surface);
    if(ret ||(!surface)) {
        fprintf(stderr, "MV_GFX_2D_CreateCel() failed: error %x\n", ret);
        return;
    }

    ret = MV_GFX_2D_GetCelInfo(gHndGFX, surface, &celInfo);
    if (ret != S_OK) {
        fprintf(stderr, "MV_GFX_2D_CreateCel() failed: error %x\n", ret);
        return;
    }
    gHndSurface[1] = surface;
    memset((void *)celInfo.uiVirtualAddr, 0XFF, celInfo.uiSize);

    gBufHnds[1] = celInfo.uiShmKey;

    fb->width = celInfo.uiWidth;
    fb->height = celInfo.uiHeight;
    fb->row_bytes = celInfo.uiStride;
    fb->pixel_bytes = PIXEL_SIZE;
    fb->data = (void *)celInfo.uiVirtualAddr;

    // Allocate frame 1 use to blit
    memset(&celInfo, 0x00, sizeof(MV_GFX_2D_CelInfo));
    celInfo.eType = MV_GFX_2D_BUFTYPE_CACHEDSHM;
    celInfo.uiWidth = FB_WIDTH;
    celInfo.uiHeight = FB_HEIGHT;
    celInfo.ePixelFormat = GFX_DISPLAY_FORMAT;
    celInfo.uiShmKey = -1;

    ret = MV_GFX_2D_CreateCel(gHndGFX, &celInfo, &surface);
    if (ret != S_OK) {
        fprintf(stderr, "MV_GFX_2D_CreateCel() failed: error %x\n", ret);
        return;
    }

    ret = MV_GFX_2D_GetCelInfo(gHndGFX, surface, &celInfo);
    if (ret != S_OK) {
        fprintf(stderr, "MV_GFX_2D_CreateCel() failed: error %x\n", ret);
        return;
    }
    gHndBlitSurface[0] = surface;
    memset((void*)celInfo.uiVirtualAddr, 0XFF, celInfo.uiSize);

    // Allocate frame 2 use to blit
    memset(&celInfo, 0x00, sizeof(MV_GFX_2D_CelInfo));
    celInfo.eType = MV_GFX_2D_BUFTYPE_CACHEDSHM;
    celInfo.uiWidth = FB_WIDTH;
    celInfo.uiHeight = FB_HEIGHT;
    celInfo.ePixelFormat = GFX_DISPLAY_FORMAT;
    celInfo.uiShmKey = -1;

    ret = MV_GFX_2D_CreateCel(gHndGFX, &celInfo, &surface);
    if (ret != S_OK) {
        fprintf(stderr, "MV_GFX_2D_CreateCel() failed: error %x\n", ret);
        return;
    }

    ret = MV_GFX_2D_GetCelInfo(gHndGFX, surface, &celInfo);
    if (ret != S_OK) {
        fprintf(stderr, "MV_GFX_2D_CreateCel() failed: error %x\n", ret);
        return;
    }
    gHndBlitSurface[1] = surface;
    memset((void*)celInfo.uiVirtualAddr, 0XFF, celInfo.uiSize);

    uiWidth = celInfo.uiWidth;
    uiHeight = celInfo.uiHeight;
    uiStride = celInfo.uiStride;
}

static void get_memory_surface(GRSurface* ms)
{
    ms->width = uiWidth;
    ms->height = uiHeight;
    ms->row_bytes = uiStride;
    ms->pixel_bytes = PIXEL_SIZE;
    ms->data = malloc(uiWidth * uiHeight * PIXEL_SIZE);
}

static gr_surface fbdev_init(minui_backend* backend) {
    HRESULT hres;

    MV_OSAL_Init();
    hres = AMP_Initialize(2, client_argv, &hFactory);
    if (hres != SUCCESS) {
        fprintf(stderr, "MV_PE_Init() failed: error %x\n", hres);
        return NULL;
    }

    // Register hotplug handler
    pthread_mutex_init(&hotplug_mutex, NULL);
    pthread_cond_init(&hotplug_cond, NULL);
    pthread_mutex_lock(&hotplug_mutex);
    quit_work_thread = 0;
    hdmi_connected = 0;
    pthread_mutex_unlock(&hotplug_mutex);
    pthread_create(&hotplug_thread, NULL, hotplug_handler_thread, NULL);
    // Need to pass output handler pointer since callback in another process
    amp_event_listener_ = AMP_Event_CreateListener(16, 0);
    AMP_Event_RegisterCallback(amp_event_listener_,
                              AMP_EVENT_API_DISP_CALLBACK_HDMI,
                              disp_event_callback,
                              NULL);

    AMP_RPC(hres, AMP_FACTORY_CreateDisplayService, hFactory, &gDisp);

#ifdef RECOVERY_TV_DISP_OUTPUT
    AMP_RPC(hres, AMP_DISP_OUT_SetResolution, gDisp, AMP_DISP_PLANE_MAIN,
        AMP_DISP_OUT_RES_1080P60 | 0x80000000, AMP_DISP_OUT_BIT_DPE_8);
    AMP_RPC(hres, AMP_DISP_OUT_HDMI_SetVidFmt, gDisp, AMP_DISP_OUT_CLR_FMT_YCBCR422,
        AMP_DISP_OUT_BIT_DPE_8, 1);
#else
    AMP_RPC(hres, AMP_DISP_OUT_SetResolution, gDisp, AMP_DISP_PLANE_MAIN,
        AMP_DISP_OUT_RES_720P60 | 0x80000000, AMP_DISP_OUT_BIT_DPE_8);

    AMP_RPC(hres, AMP_DISP_OUT_HDMI_SetVidFmt, gDisp, AMP_DISP_OUT_CLR_FMT_RGB888,
        AMP_DISP_OUT_BIT_DPE_8, 1);
#endif

    // Disable HDCP
    AMP_RPC(hres, AMP_DISP_OUT_HDCP_Enable, gDisp, 0);

    // initialize GFX
    MV_GFX_2D_Open("LAYER2", &(gHndGFX));

    get_framebuffer(gr_framebuffer);

    get_memory_surface(&gr_mem_surface);

    set_active_framebuffer(0);

    return &gr_mem_surface;
}

static gr_surface fbdev_flip(minui_backend* backend __unused) {
    /* swap front and back buffers */
    gr_active_fb = (gr_active_fb + 1) & 1;

    /* wait the active buffer free to write*/
#if DEBUG_GFX
    MV_GFX_2D_WaitCelFree(gHndGFX, gHndSurface[gr_active_fb]);
#else
    MV_GFX_2D_WaitCelFree(gHndGFX, gHndBlitSurface[gr_active_fb]);
#endif

    /* copy data from the in-memory surface to the buffer we're about
     * to make active. */
    memcpy(gr_framebuffer[gr_active_fb].data, gr_mem_surface.data,
          uiWidth * uiHeight * PIXEL_SIZE);
#if DEBUG_GFX
    {
        char name[64];
        FILE *pFile=NULL;

        memset(name, 0, sizeof(name));
        sprintf(name, "/tmp/fb%d.rgb",a_count);
        pFile = fopen(name,"w");
        if (pFile){
            printf("fb_post dump image to %s, base=0x%x, size=%d\n",
                name,gr_framebuffer[gr_active_fb].data,uiWidth * uiHeight * PIXEL_SIZE);
            fwrite((const void *)gr_framebuffer[gr_active_fb].data,
                   uiWidth * uiHeight * PIXEL_SIZE, 1, pFile);
            fclose(pFile);
            a_count++;
        }
        else{
            printf("fb_post-1 dump file %s failed\n", name);
        }
    }
#endif

    /* flush the active buffer */
    AMP_SHM_CleanCache(gBufHnds[gr_active_fb], 0, uiWidth * uiHeight * PIXEL_SIZE);
    /* inform the display driver */
    set_active_framebuffer(gr_active_fb);

    return &gr_mem_surface;
}

static void fbdev_exit(minui_backend* backend __unused) {
    free(gr_mem_surface.data);
    AMP_Event_UnregisterCallback(amp_event_listener_,
                                 AMP_EVENT_API_DISP_CALLBACK_HDMI,
                                 disp_event_callback);

    pthread_mutex_lock(&hotplug_mutex);
    quit_work_thread = 1;
    hdmi_connected = 0;
    pthread_cond_signal(&hotplug_cond);
    pthread_mutex_unlock(&hotplug_mutex);

    pthread_join(hotplug_thread, NULL);

    pthread_mutex_destroy(&hotplug_mutex);
    pthread_cond_destroy(&hotplug_cond);

    MV_GFX_2D_Close(gHndGFX);
    AMP_FACTORY_Release(hFactory);
    MV_OSAL_Exit();
}
