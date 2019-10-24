/*
 * Trace viewer utility for visualizing output on the TRACESWO pin via the
 * Black Magic Probe. This utility is built with Nuklear for a cross-platform
 * GUI.
 *
 * Copyright 2019 CompuPhase
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined WIN32 || defined _WIN32
  #define STRICT
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <direct.h>
  #include <io.h>
  #include <malloc.h>
  #if defined __MINGW32__ || defined __MINGW64__
    #include "strlcpy.h"
  #elif defined _MSC_VER
    #include "strlcpy.h"
    #define access(p,m)       _access((p),(m))
    #define mkdir(p)          _mkdir(p)
  #endif
#elif defined __linux__
  #include <alloca.h>
  #include <pthread.h>
  #include <unistd.h>
  #include <bsd/string.h>
  #include <sys/stat.h>
  #include <sys/time.h>
#endif

#include "guidriver.h"
#include "bmp-script.h"
#include "bmp-support.h"
#include "bmscan.h"
#include "gdb-rsp.h"
#include "minIni.h"
#include "noc_file_dialog.h"
#include "rs232.h"
#include "specialfolder.h"

#include "parsetsdl.h"
#include "decodectf.h"
#include "swotrace.h"

#include "res/btn_folder.h"
#if defined __linux__ || defined __unix__
  #include "res/icon_download_64.h"
#endif

#ifndef NK_ASSERT
  #include <assert.h>
  #define NK_ASSERT(expr) assert(expr)
#endif

#if defined __linux__ || defined __FreeBSD__ || defined __APPLE__
  #define stricmp(s1,s2)    strcasecmp((s1),(s2))
#endif

#if !defined sizearray
  #define sizearray(e)    (sizeof(e) / sizeof((e)[0]))
#endif


static int recent_statuscode = 0;

int ctf_error_notify(int code, int linenr, const char *message)
{
  static int ctf_statusset = 0;

  if (code == CTFERR_NONE) {
    ctf_statusset = 0;
  } else if (!ctf_statusset) {
    char msg[200];
    ctf_statusset = 1;
    if (linenr > 0)
      sprintf(msg, "TSDL file error, line %d: ", linenr);
    else
      strcpy(msg, "TSDL file error: ");
    strlcat(msg, message, sizearray(msg));
    tracelog_statusmsg(TRACESTATMSG_CTF, msg, 0);
  }
  return 0;
}

static int bmp_callback(int code, const char *message)
{
  recent_statuscode = code;
  tracelog_statusmsg(TRACESTATMSG_BMP, message, code);
  return code >= 0;
}


#define WINDOW_WIDTH  600   /* default window size (window is resizable) */
#define WINDOW_HEIGHT 300
#define FONT_HEIGHT   14
#define ROW_HEIGHT    (1.6 * FONT_HEIGHT)


static float *nk_ratio(int count, ...)
{
  #define MAX_ROW_FIELDS 10
  static float r_array[MAX_ROW_FIELDS];
  va_list ap;
  int i;

  NK_ASSERT(count < MAX_ROW_FIELDS);
  va_start(ap, count);
  for (i = 0; i < count; i++)
    r_array[i] = (float) va_arg(ap, double);
  va_end(ap);
  return r_array;
}

static void set_style(struct nk_context *ctx)
{
  struct nk_color table[NK_COLOR_COUNT];

  table[NK_COLOR_TEXT]= nk_rgba(201, 243, 255, 255);
  table[NK_COLOR_WINDOW]= nk_rgba(35, 52, 71, 255);
  table[NK_COLOR_HEADER]= nk_rgba(122, 20, 50, 255);
  table[NK_COLOR_BORDER]= nk_rgba(128, 128, 128, 255);
  table[NK_COLOR_BUTTON]= nk_rgba(122, 20, 50, 255);
  table[NK_COLOR_BUTTON_HOVER]= nk_rgba(140, 25, 50, 255);
  table[NK_COLOR_BUTTON_ACTIVE]= nk_rgba(140, 25, 50, 255);
  table[NK_COLOR_TOGGLE]= nk_rgba(20, 29, 38, 255);
  table[NK_COLOR_TOGGLE_HOVER]= nk_rgba(45, 60, 60, 255);
  table[NK_COLOR_TOGGLE_CURSOR]= nk_rgba(122, 20, 50, 255);
  table[NK_COLOR_SELECT]= nk_rgba(20, 29, 38, 255);
  table[NK_COLOR_SELECT_ACTIVE]= nk_rgba(122, 20, 50, 255);
  table[NK_COLOR_SLIDER]= nk_rgba(20, 29, 38, 255);
  table[NK_COLOR_SLIDER_CURSOR]= nk_rgba(122, 20, 50, 255);
  table[NK_COLOR_SLIDER_CURSOR_HOVER]= nk_rgba(140, 25, 50, 255);
  table[NK_COLOR_SLIDER_CURSOR_ACTIVE]= nk_rgba(140, 25, 50, 255);
  table[NK_COLOR_PROPERTY]= nk_rgba(20, 29, 38, 255);
  table[NK_COLOR_EDIT]= nk_rgba(20, 29, 38, 225);
  table[NK_COLOR_EDIT_CURSOR]= nk_rgba(201, 243, 255, 255);
  table[NK_COLOR_COMBO]= nk_rgba(20, 29, 38, 255);
  table[NK_COLOR_CHART]= nk_rgba(20, 29, 38, 255);
  table[NK_COLOR_CHART_COLOR]= nk_rgba(170, 40, 60, 255);
  table[NK_COLOR_CHART_COLOR_HIGHLIGHT]= nk_rgba(255, 0, 0, 255);
  table[NK_COLOR_SCROLLBAR]= nk_rgba(30, 40, 60, 255);
  table[NK_COLOR_SCROLLBAR_CURSOR]= nk_rgba(179, 175, 132, 255);
  table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]= nk_rgba(204, 199, 141, 255);
  table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE]= nk_rgba(204, 199, 141, 255);
  table[NK_COLOR_TAB_HEADER]= nk_rgba(122, 20, 50, 255);
  nk_style_from_table(ctx, table);
}

#define TOOLTIP_DELAY 1000
static int tooltip(struct nk_context *ctx, struct nk_rect bounds, const char *text, struct nk_rect *viewport)
{
  static struct nk_rect recent_bounds;
  static unsigned long start_tstamp;
  unsigned long tstamp;

  #if defined WIN32 || defined _WIN32
    tstamp = GetTickCount();  /* 55ms granularity, but good enough */
  #else
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    tstamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
  #endif

  if (!nk_input_is_mouse_hovering_rect(&ctx->input, bounds))
    return 0;           /* not hovering this control/area */
  if (memcmp(&bounds, &recent_bounds, sizeof(struct nk_rect)) != 0) {
    /* hovering this control/area, but it's a different one from the previous;
       restart timer */
    recent_bounds = bounds;
    start_tstamp = tstamp;
    return 0;
  }
  if (tstamp - start_tstamp < TOOLTIP_DELAY)
    return 0;           /* delay time has not reached its value yet */
  if (text != NULL)
    nk_tooltip(ctx, text, viewport);
  return 1;
}

int main(void)
{
  static const char *mode_strings[] = { "Passive listener", "Manchester", "Async." };
  static const char *format_strings[] = { "Plain text", "CTF" };
  struct nk_context *ctx;
  struct nk_image btn_folder;
  int canvas_width, canvas_height;
  char mcu_driver[32], mcu_arch[16];
  char txtConfigFile[256], findtext[128] = "", valstr[128] = "";
  char txtTSDLfile[256] = "";
  char cpuclock_str[15] = "", bitrate_str[15] = "";
  unsigned long cpuclock = 0, bitrate = 0;
  int chan, cur_chan_edit = -1;
  unsigned long channelmask = 0;
  enum { MODE_PASSIVE, MODE_MANCHESTER, MODE_ASYNC } opt_mode = MODE_MANCHESTER;
  int opt_format = 0;
  int trace_status = 0;
  int trace_running = 1;
  int reinitialize =  1;
  int reload_format = 1;
  int cur_match_line = -1;
  int find_popup = 0;

  /* locate the configuration file */
  if (folder_AppConfig(txtConfigFile, sizearray(txtConfigFile))) {
    strlcat(txtConfigFile, DIR_SEPARATOR "BlackMagic", sizearray(txtConfigFile));
    #if defined _WIN32
      mkdir(txtConfigFile);
    #else
      mkdir(txtConfigFile, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    #endif
    strlcat(txtConfigFile, DIR_SEPARATOR "bmtrace.ini", sizearray(txtConfigFile));
  }

  /* read channel configuration */
  for (chan = 0; chan < NUM_CHANNELS; chan++) {
    char key[40];
    unsigned clr;
    int enabled, result;
    channel_set(chan, (chan == 0), NULL, nk_rgb(190, 190, 190)); /* preset: port 0 is enabled by default, others disabled by default */
    sprintf(key, "chan%d", chan);
    ini_gets("Channels", key, "", valstr, sizearray(valstr), txtConfigFile);
    result = sscanf(valstr, "%d #%x %s", &enabled, &clr, key);
    if (result >= 2)
      channel_set(chan, enabled, (result >= 3) ? key : NULL, nk_rgb(clr >> 16,(clr >> 8) & 0xff, clr & 0xff));
  }
  /* other configuration */
  opt_mode = (int)ini_getl("Settings", "mode", MODE_MANCHESTER, txtConfigFile);
  opt_format = (int)ini_getl("Settings", "format", 0, txtConfigFile);
  ini_gets("Settings", "tsdl", "", txtTSDLfile, sizearray(txtTSDLfile), txtConfigFile);
  ini_gets("Settings", "mcu-freq", "48000000", cpuclock_str, sizearray(cpuclock_str), txtConfigFile);
  ini_gets("Settings", "bitrate", "100000", bitrate_str, sizearray(bitrate_str), txtConfigFile);
  ini_gets("Settings", "size", "", valstr, sizearray(valstr), txtConfigFile);
  if (sscanf(valstr, "%d %d", &canvas_width, &canvas_height) != 2 || canvas_width < 100 || canvas_height < 50) {
    canvas_width = WINDOW_WIDTH;
    canvas_height = WINDOW_HEIGHT;
  }

  trace_status = trace_init();
  if (trace_status != TRACESTAT_OK)
    trace_running = 0;
  bmp_setcallback(bmp_callback);
  reinitialize = 2; /* skip first iteration, so window is updated */
  recent_statuscode = BMPSTAT_SUCCESS;  /* must be a non-zero code to display anything */
  tracelog_statusmsg(TRACESTATMSG_BMP, "Initializing...", recent_statuscode);

  ctx = guidriver_init("BlackMagic Trace Viewer", canvas_width, canvas_height, GUIDRV_RESIZEABLE | GUIDRV_TIMER, FONT_HEIGHT);
  set_style(ctx);
  btn_folder = guidriver_image_from_memory(btn_folder_data, btn_folder_datasize);

  for ( ;; ) {
    if (reinitialize == 1) {
      if (rs232_isopen())
        bmp_break();
      if (opt_mode == MODE_PASSIVE) {
        gdbrsp_packetsize(0);
        if (rs232_isopen()) {
          bmp_detach(1);
          rs232_dtr(0);
          rs232_rts(0);
          rs232_close();
        }
      } else {
        int result = bmp_connect();
        if (result)
          result = bmp_attach(2, mcu_driver, sizearray(mcu_driver), mcu_arch, sizearray(mcu_arch)); //??? can check architecture: no SWO on Cortex-M0
        if (result) {
          unsigned long params[2];
          bmp_enabletrace((opt_mode == MODE_ASYNC) ? bitrate : 0);
          bmp_runscript("swo-device", mcu_driver, NULL);
          if ((cpuclock = strtol(cpuclock_str, NULL, 10)) == 0)
            cpuclock = 48000000;
          if ((bitrate = strtol(bitrate_str, NULL, 10)) == 0)
            bitrate = 100000;
          params[0] = opt_mode;
          params[1] = cpuclock / bitrate - 1;
          bmp_runscript("swo-generic", mcu_driver, params);
          /* enable active channels in the target (disable inactive channels) */
          channelmask = 0;
          for (chan = 0; chan < NUM_CHANNELS; chan++)
            if (channel_getenabled(chan))
              channelmask |= (1 << chan);
          params[0] = channelmask;
          bmp_runscript("swo-channels", mcu_driver, params);
          bmp_restart();
        }
      }
      tracestring_clear();
      switch (trace_status) {
      case TRACESTAT_OK:
        recent_statuscode = BMPSTAT_SUCCESS;
        if (opt_mode == MODE_PASSIVE) {
          tracelog_statusmsg(TRACESTATMSG_BMP, "Listening...", recent_statuscode);
        } else if (recent_statuscode >= 0) {
          char msg[100];
          assert(strlen(mcu_driver) > 0);
          sprintf(msg, "Connected [%s]", mcu_driver);
          tracelog_statusmsg(TRACESTATMSG_BMP, msg, recent_statuscode);
        }
        break;
      case TRACESTAT_INIT_FAILED:
      case TRACESTAT_NO_INTERFACE:
      case TRACESTAT_NO_DEVPATH:
      case TRACESTAT_NO_PIPE:
        recent_statuscode = BMPERR_GENERAL;
        tracelog_statusmsg(TRACESTATMSG_BMP, "Trace interface not available", recent_statuscode);
        break;
      case TRACESTAT_NO_ACCESS:
        recent_statuscode = BMPERR_GENERAL;
        tracelog_statusmsg(TRACESTATMSG_BMP, "Trace access denied", recent_statuscode);
        break;
      case TRACESTAT_NO_THREAD:
        recent_statuscode = BMPERR_GENERAL;
        tracelog_statusmsg(TRACESTATMSG_BMP, "Multithreading failed", recent_statuscode);
        break;
      }
      reinitialize = 0;
    } else if (reinitialize > 0) {
      reinitialize -= 1;
    }

    if (reload_format) {
      ctf_parse_cleanup();
      ctf_decode_cleanup();
      tracestring_clear();
      cur_match_line = -1;
      trace_enablectf(0);
      tracelog_statusmsg(TRACESTATMSG_CTF, NULL, 0);
      ctf_error_notify(CTFERR_NONE, 0, NULL);
      if (opt_format == 1 && strlen(txtTSDLfile)> 0 && access(txtTSDLfile, 0) == 0) {
        if (ctf_parse_init(txtTSDLfile) && ctf_parse_run()) {
          const CTF_STREAM *stream;
          int seqnr;
          trace_enablectf(1);
          /* stream names overrule configured channel names */
          for (seqnr = 0; (stream = stream_by_seqnr(seqnr)) != NULL; seqnr++)
            if (stream->name != NULL && strlen(stream->name) > 0)
              channel_setname(seqnr, stream->name);
        } else {
          ctf_parse_cleanup();
        }
      }
      reload_format = 0;
    }

    /* Input */
    nk_input_begin(ctx);
    if (!guidriver_poll(1))
      break;
    nk_input_end(ctx);

    /* GUI */
    guidriver_appsize(&canvas_width, &canvas_height);
    if (nk_begin(ctx, "MainPanel", nk_rect(0, 0, canvas_width, canvas_height), 0)) {
      int numrows, numcolumns, row, result;
      const char *ptr;

      nk_layout_row_begin(ctx, NK_STATIC, ROW_HEIGHT, 6);
      nk_layout_row_push(ctx, 45);
      nk_label(ctx, "Mode", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
      nk_layout_row_push(ctx, 125);
      result = opt_mode;
      opt_mode = nk_combo(ctx, mode_strings, NK_LEN(mode_strings), opt_mode, FONT_HEIGHT, nk_vec2(125,4.5*FONT_HEIGHT));
      if (opt_mode != result)
        reinitialize = 1;
      if (opt_mode > MODE_PASSIVE) {
        nk_layout_row_push(ctx, 70);
        nk_label(ctx, "CPU clock", NK_TEXT_ALIGN_RIGHT | NK_TEXT_ALIGN_MIDDLE);
        nk_layout_row_push(ctx, 80);
        result = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, cpuclock_str, sizearray(cpuclock_str), nk_filter_decimal);
        if ((result & NK_EDIT_COMMITED) != 0 || ((result & NK_EDIT_DEACTIVATED) && strtoul(cpuclock_str, NULL, 10) != cpuclock))
          reinitialize = 1;
        nk_layout_row_push(ctx, 60);
        nk_label(ctx, "Bit rate", NK_TEXT_ALIGN_RIGHT | NK_TEXT_ALIGN_MIDDLE);
        nk_layout_row_push(ctx, 75);
        result = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, bitrate_str, sizearray(bitrate_str), nk_filter_decimal);
        if ((result & NK_EDIT_COMMITED) != 0 || ((result & NK_EDIT_DEACTIVATED) && strtoul(bitrate_str, NULL, 10) != bitrate))
          reinitialize = 1;
      }

      nk_layout_row_begin(ctx, NK_STATIC, ROW_HEIGHT, 5);
      nk_layout_row_push(ctx, 45);
      nk_label(ctx, "Format", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
      nk_layout_row_push(ctx, 125);
      result = opt_format;
      opt_format = nk_combo(ctx, format_strings, NK_LEN(format_strings), opt_format, FONT_HEIGHT, nk_vec2(125,3*FONT_HEIGHT));
      if (opt_format != result)
        reload_format = 1;
      if (opt_format > 0) {
        nk_layout_row_push(ctx, 70);
        nk_label(ctx, "TSDL file", NK_TEXT_ALIGN_RIGHT | NK_TEXT_ALIGN_MIDDLE);
        nk_layout_row_push(ctx, canvas_width - 309);
        result = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, txtTSDLfile, sizearray(txtTSDLfile), nk_filter_ascii);
        if (result & (NK_EDIT_COMMITED | NK_EDIT_DEACTIVATED))
          reload_format = 1;
        nk_layout_row_push(ctx, 25);
        if (nk_button_image(ctx, btn_folder)) {
          const char *s = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
                                               "TSDL files\0*.tsdl;*.ctf\0All files\0*.*\0",
                                               NULL, NULL, NULL, guidriver_apphandle());
          if (s != NULL && strlen(s) < sizearray(txtTSDLfile)) {
            strcpy(txtTSDLfile, s);
            reload_format = 1;
            free((void*)s);
          }
        }
      }
      nk_layout_row_end(ctx);

      numrows = (canvas_width >= 1200) ? 1 : ((canvas_width >= 600) ? 2 : 4);
      if (numrows > 4)
        numrows = 4;
      numcolumns = 32 / numrows;
      chan = 0;
      for (row = 0; row < numrows; row++) {
        int col;
        char label[32];
        if (canvas_width >= 300)
          nk_layout_row_dynamic(ctx, FONT_HEIGHT, numcolumns);
        else
          nk_layout_row_static(ctx, FONT_HEIGHT, 35, numcolumns);
        for (col = 0; col < numcolumns; col++) {
          struct nk_rect bounds, rc_canvas;
          int enabled = channel_getenabled(chan);
          nk_style_push_color(ctx, &ctx->style.checkbox.cursor_normal.data.color, channel_getcolor(chan));
          nk_style_push_color(ctx, &ctx->style.checkbox.cursor_hover.data.color, channel_getcolor(chan));
          sprintf(label, "%d", chan);
          bounds = nk_widget_bounds(ctx);
          if (nk_checkbox_label(ctx, label, &enabled) && opt_mode > MODE_PASSIVE) {
            /* enable/disable channel in the target */
            channel_setenabled(chan, enabled);
            if (enabled)
              channelmask |= (1 << chan);
            else
              channelmask &= ~(1 << chan);
            bmp_runscript("swo-channels", mcu_driver, &channelmask);
          }
          nk_style_pop_color(ctx);
          nk_style_pop_color(ctx);
          rc_canvas = nk_rect(0, 0, canvas_width, canvas_height);
          if (nk_contextual_begin_fitview(ctx, 0, nk_vec2(125, 5*ROW_HEIGHT), bounds, &rc_canvas)) {
            struct nk_color color = channel_getcolor(chan);
            nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);
            color.r = (nk_byte)nk_propertyi(ctx, "#R", 0, color.r, 255, 1, 1);
            nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);
            color.g = (nk_byte)nk_propertyi(ctx, "#G", 0, color.g, 255, 1, 1);
            nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);
            color.b = (nk_byte)nk_propertyi(ctx, "#B", 0, color.b, 255, 1, 1);
            channel_setcolor(chan, color);
            /* the name in the channels array must only be changed on closing
               the popup, so it is copied to a local variable on first opening */
            if (cur_chan_edit == -1) {
              cur_chan_edit = chan;
              channel_getname(chan, valstr, sizearray(valstr));
            }
            nk_layout_row(ctx, NK_DYNAMIC, ROW_HEIGHT, 2, nk_ratio(2, 0.35, 0.65));
            nk_label(ctx, "name", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, valstr, sizearray(valstr), nk_filter_ascii);
            nk_contextual_end(ctx);
          } else if (cur_chan_edit == chan) {
            /* contextual popup is closed, copy the name back */
            if (strlen(valstr) == 0) {
              channel_setname(chan, NULL);
            } else {
              char *pspace;
              while ((pspace = strchr(valstr, ' ')) != NULL)
                *pspace = '-'; /* can't handle spaces in the channel names */
              channel_setname(chan, valstr);
            }
            cur_chan_edit = -1;
          }
          tooltip(ctx, bounds, "Click right for options", &rc_canvas);
          chan++;
        }
      }

      /* spacer */
      nk_layout_row_dynamic(ctx, 2, 1);
      nk_spacing(ctx, 1);

      tracestring_process(trace_running);
      nk_layout_row_dynamic(ctx, canvas_height - 4.1 * ROW_HEIGHT - 1.25 * numrows * FONT_HEIGHT - 20, 1);
      tracelog_widget(ctx, "tracelog", FONT_HEIGHT, cur_match_line, NK_WINDOW_BORDER);

      nk_layout_row(ctx, NK_DYNAMIC, ROW_HEIGHT, 7, nk_ratio(7, 0.19, 0.08, 0.19, 0.08, 0.19, 0.08, 0.19));
      ptr = trace_running ? "Stop" : tracestring_isempty() ? "Start" : "Resume";
      if (nk_button_label(ctx, ptr) || nk_input_is_key_pressed(&ctx->input, NK_KEY_F5)) {
        trace_running = !trace_running;
        if (trace_running && trace_status != TRACESTAT_OK) {
          trace_status = trace_init();
          if (trace_status != TRACESTAT_OK)
            trace_running = 0;
        }
      }
      nk_spacing(ctx, 1);
      if (nk_button_label(ctx, "Clear")) {
        tracestring_clear();
        cur_match_line = -1;
      }
      nk_spacing(ctx, 1);
      if (nk_button_label(ctx, "Search") || nk_input_is_key_pressed(&ctx->input, NK_KEY_FIND))
        find_popup = 1;
      nk_spacing(ctx, 1);
      if (nk_button_label(ctx, "Save") || nk_input_is_key_pressed(&ctx->input, NK_KEY_SAVE)) {
        const char *s = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                                             "CSV files\0*.csv\0All files\0*.*\0",
                                             NULL, NULL, NULL, guidriver_apphandle());
        if (s != NULL) {
          trace_save(s);
          free((void*)s);
        }
      }
      //??? histogram, showing trace density
      //??? show numeric traces in a graph

      /* popup dialogs */
      if (find_popup > 0) {
        struct nk_rect rc;
        rc.x = canvas_width - 250;
        rc.y = canvas_height - 6.5 * ROW_HEIGHT;
        rc.w = 200;
        rc.h = 3.6 * ROW_HEIGHT;
        if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Search", NK_WINDOW_NO_SCROLLBAR, rc)) {
          nk_layout_row(ctx, NK_DYNAMIC, ROW_HEIGHT, 2, nk_ratio(2, 0.2, 0.8));
          nk_label(ctx, "Text", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
          nk_edit_focus(ctx, 0);
          nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, findtext, sizearray(findtext), nk_filter_ascii);
          nk_layout_row(ctx, NK_DYNAMIC, FONT_HEIGHT, 2, nk_ratio(2, 0.2, 0.8));
          nk_spacing(ctx, 1);
          if (find_popup == 2)
            nk_label_colored(ctx, "Text not found", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_rgb(255, 100, 128));
          nk_layout_row_dynamic(ctx, ROW_HEIGHT, 3);
          nk_spacing(ctx, 1);
          if (nk_button_label(ctx, "Find") || nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER)) {
            if (strlen(findtext) > 0) {
              int line = tracestring_find(findtext, cur_match_line);
              if (line != cur_match_line) {
                cur_match_line = line;
                find_popup = 0;
                trace_running = 0;
              } else {
                cur_match_line = -1;
                find_popup = 2; /* to mark "string not found" */
              }
              nk_popup_close(ctx);
            } /* if (len > 0) */
          }
          if (nk_button_label(ctx, "Cancel") || nk_input_is_key_pressed(&ctx->input, NK_KEY_ESCAPE)) {
            find_popup = 0;
            nk_popup_close(ctx);
          }
          nk_popup_end(ctx);
        } else {
          find_popup = 0;
        }
      }

    }
    nk_end(ctx);

    /* Draw */
    guidriver_render(nk_rgb(30,30,30));
  }

  for (chan = 0; chan < NUM_CHANNELS; chan++) {
    char key[32];
    struct nk_color color = channel_getcolor(chan);
    sprintf(key, "chan%d", chan);
    sprintf(valstr, "%d #%06x %s", channel_getenabled(chan),
            ((int)color.r << 16) | ((int)color.g << 8) | color.b,
            channel_getname(chan, NULL, 0));
    ini_puts("Channels", key, valstr, txtConfigFile);
  }
  ini_putl("Settings", "mode", opt_mode, txtConfigFile);
  ini_putl("Settings", "format", opt_format, txtConfigFile);
  ini_puts("Settings", "tsdl", txtTSDLfile, txtConfigFile);
  ini_puts("Settings", "mcu-freq", cpuclock_str, txtConfigFile);
  ini_puts("Settings", "bitrate", bitrate_str, txtConfigFile);
  sprintf(valstr, "%d %d", canvas_width, canvas_height);
  ini_puts("Settings", "size", valstr, txtConfigFile);

  trace_close();
  guidriver_close();
  tracestring_clear();
  gdbrsp_packetsize(0);
  ctf_parse_cleanup();
  ctf_decode_cleanup();
  if (rs232_isopen()) {
    rs232_dtr(0);
    rs232_rts(0);
    rs232_close();
  }
  return 0;
}

