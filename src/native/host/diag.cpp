/* src/native/host/diag.cpp — the diagnostics overlay (diag.h).
 *
 * A Dear ImGui panel with an ImPlot frame-time graph, drawn through PsyZ's
 * SDL_GPU overlay hooks. The panel takes no input; ImGui only draws. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "implot.h"

extern "C" {
#include "diag.h"
#include "diag_stats.h"
#include "native_host.h"

/* PsyZ's psyz/overlay*.h (its include/ shadows system headers, so it stays
   off this file's path). */
typedef void (*PsyzOverlayFrameCB)(void);
typedef void (*PsyzOverlayDestroyCB)(void);
typedef void (*PsyzOverlayEventCB_SDL3)(const SDL_Event *event);
typedef void (*PsyzOverlayInitCB_SDL3GPU)(SDL_Window *window,
                                          SDL_GPUDevice *device);
typedef void (*PsyzOverlayRenderCB_SDL3GPU)(SDL_GPUCommandBuffer *cmd,
                                            SDL_GPURenderPass *pass);
PsyzOverlayFrameCB Psyz_OverlayFrameCB(PsyzOverlayFrameCB cb);
PsyzOverlayDestroyCB Psyz_OverlayDestroyCB(PsyzOverlayDestroyCB cb);
PsyzOverlayEventCB_SDL3 Psyz_OverlayEvent_SDL3(PsyzOverlayEventCB_SDL3 cb);
PsyzOverlayInitCB_SDL3GPU
Psyz_OverlayInit_SDL3GPU(PsyzOverlayInitCB_SDL3GPU cb);
PsyzOverlayRenderCB_SDL3GPU
Psyz_OverlayRender_SDL3GPU(PsyzOverlayRenderCB_SDL3GPU cb);
void Psyz_SetTitle(const char *str);
}

/* The game's g_nGamestate and g_nActiveLevelId (include/globals.h). */
#define GAMESTATE_ADDR 0x800757d8u
#define ACTIVE_LEVEL_ADDR 0x8007596cu

static const float MARGIN = 8; /* points from the game image's corner */
/* The game leaves the top 8 of its 240 lines black (a ~224-line picture),
   so the panel starts below that band. */
static const float GAME_TOP_BAND = 8.0f / 240.0f;
static const float FONT_SIZE = 12; /* the panel's text */
static const float GRAPH_FPS = 70; /* the graph's height, unless exceeded */
static const ImVec2 GRAPH_SIZE(170, 40); /* until the rows are measured */

/* The system's monospace font where there is one, else ImGui's own. */
static const char *const font_paths[] = {
    "/System/Library/Fonts/SFNSMono.ttf",
    "/System/Library/Fonts/Menlo.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
};

static int shown;
static DiagStats stats;
static SDL_Window *window;
static SDL_GPUDevice *device;
static int ready;      /* ImGui is up */
static int drawn;      /* ImGui built this frame's draw data */
static int title_mode; /* no ImGui: the numbers go in the window title */
static char base_title[256];

static void on_destroy(void) {
  if (!ready) {
    return;
  }
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  ready = 0;
}

static void set_style(void) {
  ImGuiStyle &style = ImGui::GetStyle();
  ImPlotStyle &plot = ImPlot::GetStyle();

  ImGui::StyleColorsDark();
  style.WindowRounding = 8;
  style.WindowBorderSize = 0;
  style.WindowPadding = ImVec2(10, 8);
  style.ItemSpacing = ImVec2(6, 2);
  style.CellPadding = ImVec2(4, 1);
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.8f);
  style.Colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.94f, 1);
  style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.56f, 0.62f, 1);
  style.FontSizeBase = FONT_SIZE;

  ImPlot::StyleColorsDark();
  plot.PlotPadding = ImVec2(0, 2);
  plot.PlotBorderSize = 0;
  plot.Colors[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, 0);
  plot.Colors[ImPlotCol_PlotBg] = ImVec4(1, 1, 1, 0.04f);
  plot.Colors[ImPlotCol_AxisGrid] = ImVec4(1, 1, 1, 0.18f);
  plot.Colors[ImPlotCol_AxisText] = ImVec4(0.55f, 0.56f, 0.62f, 1);
}

static void on_init(SDL_Window *w, SDL_GPUDevice *d) {
  ImGui_ImplSDLGPU3_InitInfo info;

  window = w;
  device = d;
  snprintf(base_title, sizeof(base_title), "%s", SDL_GetWindowTitle(w));
  /* No swapchain (e.g. SDL_VIDEODRIVER=offscreen): the renderer would accept
     the invalid format in a Release build, so fall back here. */
  info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(d, w);
  if (info.ColorTargetFormat == SDL_GPU_TEXTUREFORMAT_INVALID) {
    fprintf(stderr, "native: diagnostics overlay: no swapchain\n");
    title_mode = 1;
    return;
  }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  {
    ImGuiIO &io = ImGui::GetIO();

    io.IniFilename = NULL;
    io.LogFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard |
                      ImGuiConfigFlags_NoMouseCursorChange;
    ImFont *font = NULL;

    for (size_t i = 0;
         font == NULL && i < sizeof(font_paths) / sizeof(font_paths[0]); i++) {
      if (SDL_GetPathInfo(font_paths[i], NULL)) {
        font = io.Fonts->AddFontFromFileTTF(font_paths[i]);
      }
    }
    if (font == NULL) {
      io.Fonts->AddFontDefaultVector();
    }
  }
  set_style();
  info.Device = d;
  if (!ImGui_ImplSDL3_InitForSDLGPU(w)) {
    fprintf(stderr, "native: diagnostics overlay: no ImGui platform\n");
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    title_mode = 1;
    return;
  }
  if (!ImGui_ImplSDLGPU3_Init(&info)) {
    fprintf(stderr, "native: diagnostics overlay: no ImGui renderer\n");
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    title_mode = 1;
    return;
  }
  ready = 1;
}

static void update_title(void) {
  char title[512];

  if (!shown) {
    Psyz_SetTitle(base_title);
    return;
  }
  snprintf(title, sizeof(title), "%s - %.2f fps, frame %.2f ms (max %.2f)",
           base_title, stats.fps, stats.frame_ms, stats.frame_max_ms);
  Psyz_SetTitle(title);
}

static void toggle(void) {
  shown = !shown;
  if (title_mode) {
    update_title();
  }
}

/* The stick buttons held on the gamepad that last pressed one, from the
   events rather than SDL's live state: when L3 and R3 land in one poll, the
   live state already shows both at the first event, which would toggle
   twice. */
static SDL_JoystickID sticks_pad;
static unsigned sticks_held;

static unsigned stick_bit(Uint8 button) {
  switch (button) {
  case SDL_GAMEPAD_BUTTON_LEFT_STICK:
    return 1;
  case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
    return 2;
  default:
    return 0;
  }
}

static void on_event(const SDL_Event *e) {
  if (ready) {
    /* Only NewFrame drains ImGui's input queue, so it takes no events while
       hidden; the backend still sees them all. */
    ImGui::GetIO().SetAppAcceptingEvents(shown != 0);
    ImGui_ImplSDL3_ProcessEvent(e);
  }
  if (e->type == SDL_EVENT_KEY_DOWN && e->key.scancode == SDL_SCANCODE_GRAVE &&
      !e->key.repeat) {
    toggle();
  } else if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
             stick_bit(e->gbutton.button) != 0) {
    if (e->gbutton.which != sticks_pad) {
      sticks_pad = e->gbutton.which;
      sticks_held = 0;
    }
    sticks_held |= stick_bit(e->gbutton.button);
    if (sticks_held == 3) {
      toggle();
    }
  } else if (e->type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
             e->gbutton.which == sticks_pad) {
    sticks_held &= ~stick_bit(e->gbutton.button);
  } else if (e->type == SDL_EVENT_GAMEPAD_REMOVED &&
             e->gdevice.which == sticks_pad) {
    sticks_held = 0;
  }
}

static int read_game_int(uint32_t addr) {
  int v;

  memcpy(&v, psx_ptr(native_memory(), addr), sizeof(v));
  return v;
}

/* Green at full speed, amber at half, red below. */
static ImVec4 rate_color(double fps) {
  if (fps >= 55) {
    return ImVec4(0.42f, 0.86f, 0.47f, 1);
  }
  if (fps >= 28) {
    return ImVec4(0.98f, 0.76f, 0.30f, 1);
  }
  return ImVec4(0.96f, 0.38f, 0.36f, 1);
}

static void row(const char *label, const char *fmt, ...) IM_FMTARGS(2);
static void row(const char *label, const char *fmt, ...) {
  va_list ap;

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextDisabled("%s", label);
  ImGui::TableNextColumn();
  va_start(ap, fmt);
  ImGui::TextV(fmt, ap);
  va_end(ap);
}

/* The rows' width last frame, which the graph spans. */
static float rows_width = GRAPH_SIZE.x;

static void draw_graph(void) {
  static const double ticks[] = {30, 60};
  static const char *const tick_labels[] = {"30", "60"};
  /* Each frame's rate, oldest first. */
  static float fps[DIAG_HISTORY];
  int n = stats.history_len;
  int first = n == DIAG_HISTORY ? stats.history_pos : 0;
  float top = GRAPH_FPS;
  ImPlotSpec spec;

  for (int i = 0; i < n; i++) {
    float ms = stats.history[(first + i) % DIAG_HISTORY];

    fps[i] = ms > 0 ? 1000 / ms : 0;
    if (fps[i] > top) {
      top = fps[i];
    }
  }
  if (!ImPlot::BeginPlot("##frames", ImVec2(rows_width, GRAPH_SIZE.y),
                         ImPlotFlags_CanvasOnly | ImPlotFlags_NoInputs |
                             ImPlotFlags_NoFrame)) {
    return;
  }
  ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_NoDecorations);
  ImPlot::SetupAxis(ImAxis_Y1, NULL,
                    ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_Opposite);
  ImPlot::SetupAxisLimits(ImAxis_X1, 0, DIAG_HISTORY - 1, ImPlotCond_Always);
  ImPlot::SetupAxisLimits(ImAxis_Y1, 0, top * 1.05, ImPlotCond_Always);
  ImPlot::SetupAxisTicks(ImAxis_Y1, ticks, 2, tick_labels);
  spec.LineColor = rate_color(stats.fps);
  spec.FillColor = spec.LineColor;
  spec.FillAlpha = 0.25f;
  spec.LineWeight = 1.5f;
  /* Newest frames at the right edge. */
  ImPlot::PlotShaded("##fps", fps, n, 0, 1, DIAG_HISTORY - n, spec);
  ImPlot::PlotLine("##fps", fps, n, 1, DIAG_HISTORY - n, spec);
  ImPlot::EndPlot();
}

/* The top-left corner of the game's picture inside the window, past PsyZ's
   bars and the game's own black top band. */
static ImVec2 game_origin(void) {
  ImVec2 win = ImGui::GetIO().DisplaySize;
  /* PsyZ fits the game to the window at the view's aspect, centred, with
     black bars on the other axis (FitGameToWindow / GetCanvasPresentRect). */
  float aspect = (float)native_view_aspect();
  float h;

  if (win.x <= 0 || win.y <= 0) {
    return ImVec2(0, 0);
  }
  if (win.x / win.y > aspect) {
    return ImVec2((win.x - win.y * aspect) / 2, win.y * GAME_TOP_BAND);
  }
  h = win.x / aspect;
  return ImVec2(0, (win.y - h) / 2 + h * GAME_TOP_BAND);
}

static void draw_panel(void) {
  int gamestate = read_game_int(GAMESTATE_ADDR);
  ImVec2 origin = game_origin();

  ImGui::SetNextWindowPos(ImVec2(origin.x + MARGIN, origin.y + MARGIN));
  ImGui::Begin(
      "##diag", NULL,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
          ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
  draw_graph();
  /* NoHostExtendX sizes the table to its columns, not the window, so the
     graph can follow it without feeding back into the window's width. */
  if (ImGui::BeginTable("##rows", 2,
                        ImGuiTableFlags_SizingFixedFit |
                            ImGuiTableFlags_NoHostExtendX)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("fps");
    ImGui::TableNextColumn();
    ImGui::TextColored(rate_color(stats.fps), "%.1f", stats.fps);
    row("frame", "%.2f ms (max %.2f)", stats.frame_ms, stats.frame_max_ms);
    row("game", "%.2f ms", stats.game_ms);
    row("vsync", "%d", native_frames_run());
    row("state", "%d (%s)", gamestate, diag_stats_gamestate_name(gamestate));
    row("level id", "%d", read_game_int(ACTIVE_LEVEL_ADDR));
    ImGui::EndTable();
    rows_width = ImGui::GetItemRectSize().x;
  }
  ImGui::End();
}

/* Once per presented frame, before the overlay pass. */
static void on_frame(void) {
  SDL_GPUCommandBuffer *cmd;

  diag_stats_present(&stats, SDL_GetTicksNS());
  drawn = 0;
  if (!shown) {
    return;
  }
  if (title_mode) {
    if (stats.presents == 0) {
      update_title();
    }
    return;
  }
  if (!ready) {
    return;
  }
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  draw_panel();
  ImGui::Render();
  /* The vertex and texture uploads need a copy pass, which can't run inside
     PsyZ's overlay render pass: they go in a command buffer of their own,
     submitted ahead of the frame's. */
  cmd = SDL_AcquireGPUCommandBuffer(device);
  if (cmd != NULL) {
    ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
    SDL_SubmitGPUCommandBuffer(cmd);
    drawn = 1;
  }
}

static void on_render(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass) {
  if (!shown || !drawn) {
    return;
  }
  ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, pass);
}

void diag_init(int show) {
  shown = show;
  Psyz_OverlayInit_SDL3GPU(on_init);
  Psyz_OverlayEvent_SDL3(on_event);
  Psyz_OverlayFrameCB(on_frame);
  Psyz_OverlayRender_SDL3GPU(on_render);
  Psyz_OverlayDestroyCB(on_destroy);
}

void diag_vsync(uint64_t start, uint64_t end) {
  diag_stats_vsync(&stats, start, end);
}
