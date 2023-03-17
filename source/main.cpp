#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <tex3ds.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include "fileUtils.hpp"
#include "ascii.hpp"
#include "asciiRenderer.hpp"
#include "asciiUI.hpp"
#include "asciiHint.hpp"
#include "asciiLines.hpp"
#include "asciiCommand.hpp"
#include "fileExplorer.hpp"

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000

static u32 *SOC_buffer = NULL;

static C3D_RenderTarget* leftTarget;
// static C3D_RenderTarget* rightTarget;
static C3D_RenderTarget* botTarget;
static C3D_RenderTarget* curTarget;

static C3D_Mtx leftProj;
//static C3D_Mtx rightProj;
static C3D_Mtx botProj;

C3D_Mtx* screenProj;

static const char CHARSET_PATH[] = "/3dscii/charsets/";
static const char PALETTE_PATH[] = "/3dscii/palettes/";
static const char ART_PATH[] = "/3dscii/art/";

static std::string artworkPath;

static ASCII::Data asciiData;
static ASCII::Renderer renderer;
static ASCII::UI ui;
static ASCII::Hint hint;
static ASCII::History history;

static FileExplorer fileExplorer;
static bool explorerOpen = false;

static FileUtils::Config cfg;
static bool configChanged = false;

static ASCII::CAction* currentAction = NULL;
static int lastCanvasX = -1;
static int lastCanvasY = -1;

static ASCII::Renderer::UITextureVertex* uiPaletteStart;
static ASCII::Renderer::UITextureVertex* paletteSelectorsStart;

static ASCII::UI::Button* xFlipBtn;
static ASCII::UI::Button* yFlipBtn;
static ASCII::UI::Button* dFlipBtn;
static ASCII::UI::Button* invColorsBtn;

static u8 fgColor = 0;
static u8 bgColor = 0;
static int palettePage = 0;
static int wheelPos = 0;
static float paletteScroll = 0.f;
static int activeLayer = 0;
static bool eyedropperSelected = false;
static ASCII::UI::Button* eyedropperBtn;
static bool affectCharacters = true;
static bool affectFGColor = true;
static bool affectBGColor = true;
static bool invertColors = false;

__attribute__((format(printf, 1, 2))) static void failExit(const char *fmt, ...);

static void socShutdown() {
  printf("waiting for socExit...\n");
  socExit();
}

//TODO: charset selection
static void loadCharset(bool resetFrame) {
  // auto files = FileUtils::listFilesWithExt(CHARSET_PATH, "char");
  // for (std::string file : *files.get()) {
  std::string file = CHARSET_PATH;
  file += asciiData.charset.name + ".char";

  ASCII::Charset charset = FileUtils::loadCharsetFile(file);
  charset.name = asciiData.charset.name;
  if (charset.tilesetName.empty())
    return;
  
  // hack: textures get randomly corrupted when uploaded during frame
  // wating just one extra vsync cycle is okay, right?
  if (resetFrame)
    C3D_FrameEnd(0);
  if (renderer.charsetTexInitialized) {
    C3D_TexDelete(&renderer.charsetTex);
    renderer.charsetTexInitialized = false;
  }
  std::string tilesetFile = file.substr(0, file.find_last_of('/') + 1) + charset.tilesetName;
  bool loaded = FileUtils::loadTexFile(tilesetFile, &renderer.charsetTex, &charset.texWidth, &charset.texHeight);
  if (resetFrame)
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
  if (!loaded)
    return;
  renderer.charsetTexInitialized = true;

  float s = 2.f;

  asciiData.charWidth = charset.texWidth / charset.width;
  asciiData.charHeight = charset.texHeight / charset.height;

  renderer.charSize[0] = (float)charset.texWidth / (float)renderer.charsetTex.width / (float)charset.width;
  renderer.charSize[1] = (float)charset.texHeight / (float)renderer.charsetTex.height / (float)charset.height;

  asciiData.charset = charset;

  renderer.charPicker.scale[0] = renderer.charPicker.scale[1] = s;

  renderer.charPicker.position[0] = -charset.texWidth / 2.f * s;
  renderer.charPicker.position[1] = -charset.texHeight / 2.f * s;

}

static void insertEmptyLayer() {
  auto layer = &asciiData.layers.emplace_back();
  layer->alpha = 1.f;
  layer->visible = true;

  std::vector<ASCII::Cell>* data = &layer->data;
  *data = std::vector<ASCII::Cell>(asciiData.width * asciiData.height);
  for (unsigned int i = 0; i < data->size(); i++) {
    (*data)[i].value = 0;
    (*data)[i].bgColor = 0;
    (*data)[i].fgColor = 0;
    (*data)[i].xFlip = false;
    (*data)[i].yFlip = false;
    (*data)[i].dFlip = false;
  }
}

static void insertJunkLayer() {
  auto layer = &asciiData.layers.emplace_back();
  layer->alpha = 1.f;
  layer->visible = true;
  
  std::vector<ASCII::Cell>* data = &layer->data;
  *data = std::vector<ASCII::Cell>(asciiData.width * asciiData.height);
  for (unsigned int i = 0; i < data->size(); i++) {
    (*data)[i].value = 1 + std::rand() % 6;
    (*data)[i].bgColor = 0;
    (*data)[i].fgColor = 8 + std::rand() % 8;
    (*data)[i].xFlip = false;
    (*data)[i].yFlip = false;
    (*data)[i].dFlip = false;
  }
}

static void setupRenderTargets() {
  leftTarget = C3D_RenderTargetCreate(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP, GPU_RB_RGBA8, GPU_RB_DEPTH16);
  C3D_RenderTargetSetOutput(leftTarget, GFX_TOP, GFX_LEFT, 
    GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
	  GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
	  GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
  );

  botTarget = C3D_RenderTargetCreate(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, GPU_RB_RGBA8, GPU_RB_DEPTH16);
  C3D_RenderTargetSetOutput(botTarget, GFX_BOTTOM, GFX_LEFT, 
    GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
	  GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
	  GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
  );
}

static void gridSync(ASCII::Renderer::IPosScale* posScale, ASCII::UI::Grid* grid) {
  grid->position[0] = posScale->position[0];
  grid->position[1] = posScale->position[1];

  grid->scale[0] = posScale->scale[0];
  grid->scale[1] = posScale->scale[1];
}

static void charPickerOnTouch(float dx, float dy) {
  renderer.charPicker.selected[0] = (int)dx;
  renderer.charPicker.selected[1] = (int)dy;
}

static void updateFgSelection() {
  int num = fgColor - palettePage * 18;
  if (num > 18 || num < 0) {
    num = -1;
  } else {
    num++;
  }
  auto selectionArrow = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_selectionArrow1_idx);
  renderer.ui.writeUI(paletteSelectorsStart, selectionArrow, -26, -120 + num * 12 + 2, 2, 1, 1, 1, 0);
}

static void updateBgSelection() {
  int num = bgColor - palettePage * 18;
  if (num > 18 || num < 0) {
    num = -1;
  } else {
    num++;
  }
  auto selectionArrow = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_selectionArrow1_idx);
  renderer.ui.writeUI(&paletteSelectorsStart[6], selectionArrow, -14, -120 + num * 12 + 2, 2, 1, 1, 1, 0);
}

static void updateFgColor() {
  u8 color = invertColors ? bgColor : fgColor;
  ASCII::Color c = asciiData.palette.colors[color];
  float col[4] = {
    1.f / 255 * c.r,
    1.f / 255 * c.g,
    1.f / 255 * c.b,
    1.f / 255 * c.a
  };

  auto vert = renderer.charPicker.vbo;
  for (int i = 0; i < asciiData.charset.width * asciiData.charset.height * 6; i++) {
    for (int j = 0; j < 4; j++) {
      vert->color[j] = col[j];
    }
    vert++;
  }

  updateFgSelection();
}

static void updateBgColor() {
  u8 color = invertColors ? fgColor : bgColor;
  ASCII::Color c = asciiData.palette.colors[color];
  float col[4] = {
    1.f / 255 * c.r,
    1.f / 255 * c.g,
    1.f / 255 * c.b,
    1.f / 255 * c.a
  };

  renderer.charPicker.background.writeColor(col[0], col[1], col[2], col[3]);

  updateBgSelection();
}

static void drawPalette() {
  u8 colorsLeft = 18;
  int pageHighestColor = (palettePage + 1) * 18;
  if (pageHighestColor > asciiData.palette.size)
    colorsLeft -= (pageHighestColor - asciiData.palette.size);

  auto vert = uiPaletteStart;

  const Tex3DS_SubTexture* paletteCell = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_paletteCell1_idx);
  const Tex3DS_SubTexture* paletteFrame = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_paletteFrame1_idx);

  for (int i = 1; i <= 18; i++) {
    if (i - 1 < colorsLeft) {
      // cell
      renderer.ui.writeUI(vert, paletteCell, -24, -120 + 12 * i, 0, 1, 1, 1, 0);
      vert += 6;

      // frame
      renderer.ui.writeUI(vert, paletteFrame, -24, -120 + 12 * i, 2, 1, 1, 1, 0);
      vert += 6;
    } else {
      for (int v = 0; v < 12; v++) {
        *vert = {};
        vert++;
      }
    }
  }

  renderer.colorPicker.writeColorsFromPalette(renderer.colorPicker.vbo, &asciiData.palette.colors[palettePage * 18], 18, colorsLeft);

  updateFgSelection();
  updateBgSelection();
}

static void loadPalette() {
  std::string paletteFilePath = PALETTE_PATH;
  paletteFilePath += asciiData.palette.name + ".png";
  auto palette = FileUtils::loadPaletteFile(paletteFilePath);
  palette.name = asciiData.palette.name;
  asciiData.palette = palette;

  palettePage = 0;
  bgColor = 0;
  fgColor = 2;
}

static void updatePalette() {
  updateBgColor();
  updateFgColor();

  drawPalette();
}

static void colorPickerOnTouch(float dx, float dy) {
  u8 id = palettePage * 18 + (int)dy;
  if (id >= asciiData.palette.size)
    return;
  
  if ((int)dx == 0) {
    fgColor = id;
    updateFgColor();
  } else {
    bgColor = id;
    updateBgColor();
  }
}

static void canvasOnTouch(bool released, float dx, float dy) {
  //TODO: affect all tiles between last touch's position and current one
  // if not in eyedropper mode
  if (released) {
    if (currentAction != NULL) {
      history.push(currentAction);
      currentAction = NULL;
    }

    if (eyedropperSelected) {
      eyedropperBtn->onPress();
      eyedropperBtn->onSwap(false);
    }
    
    lastCanvasX = -1;
    lastCanvasY = -1;
    return;
  }

  int x = (int)dx;
  int y = (int)dy;

  if (lastCanvasX == x && lastCanvasY == y) 
    return;

  lastCanvasX = x;
  lastCanvasY = y;

  int index = x + y * asciiData.width;
  auto cell = &asciiData.layers[activeLayer].data[index];

  if (eyedropperSelected) {
    u16 tIdx = cell->value < (asciiData.charset.width * asciiData.charset.height) ? cell->value : 0;
    u8 bgIdx = cell->bgColor < asciiData.palette.size ? cell->bgColor : 0;
    u8 fgIdx = cell->fgColor < asciiData.palette.size ? cell->fgColor : 0;

    renderer.charPicker.selected[0] = tIdx % asciiData.charset.width;
    renderer.charPicker.selected[1] = tIdx / asciiData.charset.width;

    if (!cell->invColors) {
      fgColor = fgIdx;
      bgColor = bgIdx;
    } else {
      fgColor = bgIdx;
      bgColor = fgIdx;
    }
    
    renderer.charPicker.xFlip = cell->xFlip;
    renderer.charPicker.yFlip = cell->yFlip;
    renderer.charPicker.dFlip = cell->dFlip;
    invertColors = cell->invColors;

    // sync the icon states
    xFlipBtn->onSwap(false);
    yFlipBtn->onSwap(false);
    dFlipBtn->onSwap(false);
    invColorsBtn->onSwap(false);
    
    updateFgColor();
    updateBgColor();
  }

  auto before = *cell;
  auto after = *cell;

  u16 val = renderer.charPicker.selected[0] + renderer.charPicker.selected[1] * asciiData.charset.width;
  if (affectCharacters) {
    after.value = val;
    after.xFlip = renderer.charPicker.xFlip;
    after.yFlip = renderer.charPicker.yFlip;
    after.dFlip = renderer.charPicker.dFlip;
    after.invColors = invertColors;
  }
  if (affectFGColor)
    after.fgColor = after.invColors ? bgColor : fgColor;
  if (affectBGColor)
    after.bgColor = after.invColors ? fgColor : bgColor;

  if (currentAction == NULL) {
    currentAction = new ASCII::CAction();
  }

  currentAction->push(new ASCII::CPaint(activeLayer, index, before, after))->execute(&asciiData, &renderer);
}

static void scrollWheelOnSwap(ASCII::UI::Button* btn, ASCII::Renderer::UITextureVertex* vert) {
  auto scrollTex = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_scroll1_idx + wheelPos);
  ASCII::UI::Button::btnSwap(btn, vert, scrollTex);
}

static void load(std::string path, bool resetFrame) {
  history.clear();

  FileUtils::loadArtworkFile(path, &asciiData);
  loadCharset(resetFrame);
  loadPalette();

  renderer.initCharPicker();

  renderer.canvas.readLayers(&renderer);
}

static void scrollPalettePages(float dx, float dy) {
  float prevPos[] = {
    (float)(ui.prevTouch.px - 160),
    (float)(ui.prevTouch.py - 120)
  };

  paletteScroll += (dx - prevPos[0]) / 8.f;

  int prevPage = palettePage;

  while (paletteScroll > 1) {
    paletteScroll -= 1;
    palettePage += 1;
    if (palettePage >= (asciiData.palette.size + 18 - 1) / 18) 
      palettePage = 0;

    wheelPos--;
    if (wheelPos < 0)
      wheelPos = 4;
  }
  while (paletteScroll < -1) {
    paletteScroll += 1;
    palettePage -= 1;
    if (palettePage < 0) 
      palettePage = (asciiData.palette.size + 18 - 1) / 18 - 1;

    wheelPos++;
    if (wheelPos > 4)
      wheelPos = 0;
  }

  if (prevPage != palettePage) 
    drawPalette();
}

static void swapColors() {
  u8 t = fgColor;
  fgColor = bgColor;
  bgColor = t;

  updateFgColor();
  updateBgColor();
}

static void undo() {
  if (!currentAction) {
    auto cmd = history.undo();
    if (cmd)
      cmd->restore(&asciiData, &renderer);
  }
}

static void redo() {
  if (!currentAction) {
    auto cmd = history.redo();
    if (cmd)
      cmd->execute(&asciiData, &renderer);
  }
}

static void initButtonHint(ASCII::UI::Button* btn, ASCII::Lines::LINE line) {
  btn->onDown = [line]() {
    hint.setText(ASCII::Lines::english[line]);
  };
  btn->onUp = []() {
    hint.show = false;
  };
}

static void saveConfigChanges() {
  if (configChanged) {
    FileUtils::saveConfigFile("/3dscii/config.ini", &cfg);
    configChanged = false;
  }
}

static void initUI() {
  ui.init();

  // grids

  ui.charPicker.onSync = [](ASCII::UI::Grid* grid) {
    ui.charPicker.cellDimension[0] = asciiData.charWidth;
    ui.charPicker.cellDimension[1] = asciiData.charHeight;

    ui.charPicker.cellCount[0] = asciiData.charset.width;
    ui.charPicker.cellCount[1] = asciiData.charset.height;

    gridSync(&renderer.charPicker, grid);
  };
  ui.charPicker.onPress = charPickerOnTouch;
  ui.charPicker.onDrag = charPickerOnTouch;
  ui.charPicker.onRelease = charPickerOnTouch;

  ui.colorPicker.cellDimension[0] = 12;
  ui.colorPicker.cellDimension[1] = 12;

  ui.colorPicker.cellCount[0] = 2;
  ui.colorPicker.cellCount[1] = 18;

  ui.colorPicker.position[0] = 160 - 24;
  ui.colorPicker.position[1] = -120 + 12;

  ui.colorPicker.scale[0] = 1;
  ui.colorPicker.scale[1] = 1;

  ui.colorPicker.onSync = [](ASCII::UI::Grid*){};
  ui.colorPicker.onPress = colorPickerOnTouch;
  ui.colorPicker.onDrag = colorPickerOnTouch;
  ui.colorPicker.onRelease = colorPickerOnTouch;

  ui.canvas.onSync = [](ASCII::UI::Grid* grid) {
    ui.canvas.cellDimension[0] = asciiData.charWidth;
    ui.canvas.cellDimension[1] = asciiData.charHeight;

    ui.canvas.cellCount[0] = asciiData.width;
    ui.canvas.cellCount[1] = asciiData.height;

    gridSync(&renderer.canvas, grid);
  };
  ui.canvas.onPress = [](float dx, float dy){canvasOnTouch(false, dx, dy);};
  ui.canvas.onDrag = [](float dx, float dy){canvasOnTouch(false, dx, dy);};
  ui.canvas.onRelease = [](float dx, float dy){canvasOnTouch(true, dx, dy);};

  auto vert = renderer.ui.vbo;

  // sidebar backgrounds

  auto frame1 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_uiFrame1_idx);
  auto frame2 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_uiFrame2_idx);
  auto frame3 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_uiFrame3_idx);

  // left
  renderer.ui.writeUI(&vert[0], frame1, 0, -120, 0, 1, 1, -1, 0);
  renderer.ui.writeUI(&vert[6], frame2, 0, -120 + frame1->height, 0, 1, (240 - (frame1->height + frame3->height)) / (float)frame2->height, -1, 0);
  renderer.ui.writeUI(&vert[12], frame3, 0, 120 - frame3->height, 0, 1, 1, -1, 0);
  vert += 18;

  renderer.ui.writeUI(&vert[0], frame1, 0, -120, 0, -1, 1, 1, 0);
  renderer.ui.writeUI(&vert[6], frame2, 0, -120 + frame1->height, 0, -1, (240 - (frame1->height + frame3->height)) / (float)frame2->height, 1, 0);
  renderer.ui.writeUI(&vert[12], frame3, 0, 120 - frame3->height, 0, -1, 1, 1, 0);
  vert += 18;
  
  // buttons

  auto separator = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_separator1_idx);

  ASCII::UI::Button* btn;
  const Tex3DS_SubTexture* tex;

  // left sidebar

  auto buttons = &ui.leftPanel.buttons;
  float dy = -120 + 4;

  auto initBtn = [&](int subIdx, int subIdxPressed, bool* toggle, ASCII::Lines::LINE hint, bool inverted = false) {
    btn = &buttons->emplace_back();
    tex = Tex3DS_GetSubTexture(renderer.tilesT3x, subIdx);
    btn->init(-160 + 3 + (16 - tex->width) / 2.f, dy, vert, renderer.tilesT3x, subIdx, subIdxPressed, toggle, inverted);
    renderer.ui.writeUI(vert, tex, 3 + (16 - tex->width) / 2.f, dy, 0, 1, 1, -1, 0);
    btn->onPress = [toggle]() {*toggle = !*toggle;};
    initButtonHint(btn, hint);
    vert += 6;
    dy += tex->height + 4;
  };
    
  // xFlip
  initBtn(
    tiles_xFlip2_idx, tiles_xFlip1_idx,
    &renderer.charPicker.xFlip,
    ASCII::Lines::LINE_XFLIP
  );
  xFlipBtn = btn;

  // yFlip
  initBtn(
    tiles_yFlip2_idx, tiles_yFlip1_idx,
    &renderer.charPicker.yFlip,
    ASCII::Lines::LINE_YFLIP
  );
  yFlipBtn = btn;

  // dFlip
  initBtn(
    tiles_dFlip2_idx, tiles_dFlip1_idx,
    &renderer.charPicker.dFlip,
    ASCII::Lines::LINE_DFLIP
  );
  dFlipBtn = btn;

  // invert colors
  initBtn(
    tiles_invColors2_idx, tiles_invColors1_idx,
    &invertColors,
    ASCII::Lines::LINE_INVERT_COLORS
  );
  btn->onPress = [btn]() {
    invertColors = !invertColors;

    updateFgColor();
    updateBgColor();
  };
  invColorsBtn = btn;

  // 1st separator
  renderer.ui.writeUI(vert, separator, 0, dy, 0, 1, 1, -1, 0);
  vert += 6;
  dy += separator->height + 4;
  
  // eyedropper
  initBtn(
    tiles_eyedropper2_idx, tiles_eyedropper1_idx,
    &eyedropperSelected,
    ASCII::Lines::LINE_EYEDROPPER
  );
  eyedropperBtn = btn;

  // 2nd separator
  renderer.ui.writeUI(vert, separator, 0, dy, 0, 1, 1, -1, 0);
  vert += 6;
  dy += separator->height + 4;

  // affect characters
  initBtn(
    tiles_character2_idx, tiles_character1_idx,
    &affectCharacters,
    ASCII::Lines::LINE_AFFECT_CHARS,
    true
  );

  // affect foreground color
  initBtn(
    tiles_fgColor2_idx, tiles_fgColor1_idx,
    &affectFGColor,
    ASCII::Lines::LINE_AFFECT_FG,
    true
  );

  // affect background color
  initBtn(
    tiles_bgColor2_idx, tiles_bgColor1_idx,
    &affectBGColor,
    ASCII::Lines::LINE_AFFECT_BG,
    true
  );

  // 3nd separator
  renderer.ui.writeUI(vert, separator, 0, dy, 0, 1, 1, -1, 0);
  vert += 6;
  dy += separator->height + 4;

  // save
  initBtn(
    tiles_save2_idx, tiles_save1_idx,
    NULL,
    ASCII::Lines::LINE_QUICKSAVE
  );
  btn->onPress = []() {
    FileUtils::saveArtworkFile(artworkPath, &asciiData, cfg.keepBackups);

    std::string name = artworkPath.substr(artworkPath.find_last_of('/') + 1);
    char text[hint.renderer->maxChars];
    snprintf(text, sizeof(text), ASCII::Lines::english[ASCII::Lines::LINE_STATUS_SAVED], name.c_str());
    hint.setText(text);
    hint.slide += 0.4;
    hint.time = 3.;
  };

  // right sidebar

  buttons = &ui.rightPanel.buttons;
  dy = -120 + 1;

  // palette scroll wheel
  btn = &buttons->emplace_back();
  tex = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_scroll1_idx);
  btn->aabb = {
    l: 160.f - 23.f,
    u: dy,
    r: 160.f - 23.f + tex->width,
    d: dy + tex->height
  };
  renderer.ui.writeUI(vert, tex, -23, dy, 0, 1, 1, 1, 0);
  btn->onPress = [](){};
  btn->onSync = [](ASCII::UI::Button*){};
  btn->onSwap = [btn, vert](bool pressed) {scrollWheelOnSwap(btn, vert);};
  btn->onDrag = scrollPalettePages;
  initButtonHint(btn, ASCII::Lines::LINE_PALETTE_SCROLL);
  vert += 6;

  dy = 120 - 12;

  // swap colors
  btn = &buttons->emplace_back();
  tex = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_swap2_idx);
  btn->init(160 - 23, dy, vert, renderer.tilesT3x, tiles_swap2_idx, tiles_swap1_idx);
  renderer.ui.writeUI(vert, tex, -23, dy, 0, 1, 1, 1, 0);
  btn->onSync = [](ASCII::UI::Button*){};
  btn->onDrag = [](float, float){};
  btn->onPress = swapColors;
  initButtonHint(btn, ASCII::Lines::LINE_SWAP_COLORS);
  vert += 6;

  // palette
  
  // cells and frames
  uiPaletteStart = vert;
  vert += (6 + 6) * 18;
  // selection arrows
  paletteSelectorsStart = vert;
  vert += 6 * 2;

  updatePalette();
}

static void initMenu() {
  auto frame1 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_hintFrame1_idx);
  auto frame2 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_hintFrame2_idx);
  auto frame3 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_hintFrame3_idx);

  auto vert = renderer.menu.vbo;
  ASCII::UI::Button* btn;
  const Tex3DS_SubTexture* tex;
  auto buttons = &ui.menuBtns;

  // main section

  int btnCount = 5;
  float btnSectorW = 32 * btnCount;

  float dx = -(btnSectorW / 2.f);
  float dy = -(frame1->height / 2.f);
  float dz = 10.f;
  renderer.menu.writeUI(&vert[0], frame1, dx - frame1->width, dy, dz, 1, 1, 0, 0);
  renderer.menu.writeUI(&vert[6], frame2, dx, dy, dz, btnSectorW / frame2->width, 1, 0, 0);
  renderer.menu.writeUI(&vert[12], frame3, dx + btnSectorW, dy, dz, 1, 1, 0, 0);
  vert += 18;

  auto initBtn = [&](int subIdx, int subIdxPressed, bool* toggle, ASCII::Lines::LINE line, bool inverted = false, bool hintState = false) {
    btn = &buttons->emplace_back();
    tex = Tex3DS_GetSubTexture(renderer.tilesT3x, subIdx);
    btn->init(dx, dy + 6, vert, renderer.tilesT3x, subIdx, subIdxPressed, toggle, inverted);
    btn->aabb.r = btn->aabb.l + 32;
    btn->aabb.d = btn->aabb.u + 32;
    renderer.ui.writeUI(vert, tex, dx + 16 - tex->width, dy + 6 + 16 - tex->height, dz, 2, 2, 0, 0);

    if (hintState) {
      btn->onDown = [line, toggle, inverted]() {
        ASCII::Lines::LINE state = *toggle == inverted ? ASCII::Lines::LINE_ON : ASCII::Lines::LINE_OFF;
        char text[hint.renderer->maxChars];
        snprintf(text, sizeof(text), ASCII::Lines::english[line], ASCII::Lines::english[state]);
        hint.setText(text);
      };
      btn->onUp = []() {
        hint.show = false;
      };
    } else {
      initButtonHint(btn, line);
    }
    vert += 6;
    dx += 32;
    // calling this so that the correct icon is displayed
    btn->onSwap(false);
  };

  initBtn(
    tiles_save2_idx, tiles_save1_idx,
    NULL,
    ASCII::Lines::LINE_FILE_SAVE
  );
  btn->onPress = []() {
    explorerOpen = true;
    fileExplorer.mode = FileExplorer::EXPLORER_CREATE;
    fileExplorer.filterEnabled = false;
    fileExplorer.rootPath = ART_PATH;
    fileExplorer.limitedToRoot = true;
    fileExplorer.callback = [](std::string path) {
      artworkPath = path;
      cfg.lastArtwork = path;
      configChanged = true;
      FileUtils::saveArtworkFile(path, &asciiData, cfg.keepBackups);
      explorerOpen = false;
      fileExplorer.close();
    };
    fileExplorer.load(ART_PATH);
  };

  initBtn(
    tiles_load2_idx, tiles_load1_idx,
    NULL,
    ASCII::Lines::LINE_FILE_OPEN
  );
  btn->onPress = [](){
    explorerOpen = true;
    fileExplorer.mode = FileExplorer::EXPLORER_LOAD;
    fileExplorer.filterEnabled = false;
    fileExplorer.rootPath = ART_PATH;
    fileExplorer.limitedToRoot = true;
    fileExplorer.callback = [](std::string path) {
      artworkPath = path;
      cfg.lastArtwork = path;
      configChanged = true;
      load(path, true);
      updatePalette();
      explorerOpen = false;
      fileExplorer.close();
    };
    fileExplorer.load(ART_PATH);
  };

  initBtn(
    tiles_charset2_idx, tiles_charset1_idx,
    NULL,
    ASCII::Lines::LINE_LOAD_CHARSET
  );
  btn->onPress = [](){
    explorerOpen = true;
    fileExplorer.mode = FileExplorer::EXPLORER_LOAD;
    fileExplorer.filter = "char";
    fileExplorer.filterEnabled = true;
    fileExplorer.rootPath = CHARSET_PATH;
    fileExplorer.limitedToRoot = true;
    fileExplorer.callback = [](std::string path) {
      size_t pathLen = std::string(CHARSET_PATH).length();
      asciiData.charset.name = path.substr(pathLen, path.length() - pathLen - 5);
      loadCharset(true);
      renderer.initCharPicker();
      renderer.canvas.readLayers(&renderer);
      updatePalette();
      explorerOpen = false;
      fileExplorer.close();
    };
    fileExplorer.load(CHARSET_PATH);
  };

  initBtn(
    tiles_palette2_idx, tiles_palette1_idx,
    NULL,
    ASCII::Lines::LINE_LOAD_PALETTE
  );
  btn->onPress = [](){
    explorerOpen = true;
    fileExplorer.mode = FileExplorer::EXPLORER_LOAD;
    fileExplorer.filter = "png";
    fileExplorer.filterEnabled = true;
    fileExplorer.rootPath = PALETTE_PATH;
    fileExplorer.limitedToRoot = true;
    fileExplorer.callback = [](std::string path) {
      size_t pathLen = std::string(CHARSET_PATH).length();
      asciiData.palette.name = path.substr(pathLen, path.length() - pathLen - 4);
      loadPalette();
      renderer.canvas.readLayers(&renderer);
      updatePalette();
      explorerOpen = false;
      fileExplorer.close();
    };
    fileExplorer.load(PALETTE_PATH);
  };

  initBtn(
    tiles_settings2_idx, tiles_settings1_idx,
    NULL,
    ASCII::Lines::LINE_UNIMPLEMENTED
  );
  btn->onPress = [](){
    //TODO
  };

  // config section

  btnCount = 4;
  btnSectorW = 32 * btnCount;

  dx = -(btnSectorW / 2.f);
  dy += frame1->height - 1;

  renderer.menu.writeUI(&vert[0], frame1, dx - frame1->width, dy, dz, 1, 1, 0, 0);
  renderer.menu.writeUI(&vert[6], frame2, dx, dy, dz, btnSectorW / frame2->width, 1, 0, 0);
  renderer.menu.writeUI(&vert[12], frame3, dx + btnSectorW, dy, dz, 1, 1, 0, 0);
  vert += 18;

  initBtn(
    tiles_backup2_idx, tiles_backup1_idx,
    &cfg.keepBackups,
    ASCII::Lines::LINE_TOGGLE_BACKUPS,
    true, true
  );
  btn->onPress = []() {
    cfg.keepBackups = !cfg.keepBackups;
    configChanged = true;
  };

  initBtn(
    tiles_hold2_idx, tiles_hold1_idx,
    &cfg.holdBumpers,
    ASCII::Lines::LINE_TOGGLE_HOLD,
    true, true
  );
  btn->onPress = []() {
    cfg.holdBumpers = !cfg.holdBumpers;
    configChanged = true;
  };

  initBtn(
    tiles_wireless2_idx, tiles_wireless1_idx,
    &cfg.remoteSendEnabled,
    ASCII::Lines::LINE_UNIMPLEMENTED,
    true
  );
  btn->onPress = []() {
    //TODO
  };
  
  initBtn(
    tiles_question2_idx, tiles_question1_idx,
    NULL,
    ASCII::Lines::LINE_HINTS
  );
  btn->onDown = []() {
    hint.setText(ASCII::Lines::english[ASCII::Lines::LINE_HINTS]);
    hint.slide += 0.4;
    hint.time = 3.;
  };
  btn->onUp = []() {
    hint.show = false;
  };
  btn->onPress = []() {};

  // zoom slider

  // auto slider1 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_slider1_idx);
  // auto slider2 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_slider2_idx);
  // auto slider3 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_slider3_idx);
  // auto zoom3 = Tex3DS_GetSubTexture(renderer.tilesT3x, tiles_zoom3_idx);

  // int nibs = 5;
  // btnSectorW = (slider1->width + slider3->width + slider2->width * nibs) * 2.f;

  // dx = 160 - (btnSectorW + frame3->width);
  // dy = -120;

  // renderer.menu.writeUI(&vert[0], frame1, dx - frame1->width, dy, dz, 1, 1, 0, 0);
  // renderer.menu.writeUI(&vert[6], frame2, dx, dy, dz, btnSectorW / frame2->width, 1, 0, 0);
  // renderer.menu.writeUI(&vert[12], frame3, dx + btnSectorW, dy, dz, 1, 1, 0, 0);
  // vert += 18;

  // renderer.menu.writeUI(vert, slider1, dx, dy, dz, 2, 2, 0, 0);
  // vert += 6;
  // dx += slider1->width * 2.f;
  // for (int i = 0; i < nibs; ++i) {
  //   renderer.menu.writeUI(vert, slider2, dx, dy, dz, 2, 2, 0, 0);
  //   dx += slider2->width * 2.f;
  //   vert += 6;
  // }
  // renderer.menu.writeUI(vert, slider3, dx, dy, dz, 2, 2, 0, 0);
  // vert += 6;

  // dx = 160 - (btnSectorW + frame3->width);

  // btn = &buttons->emplace_back();
  // btn->init(dx, dy, vert, renderer.tilesT3x, tiles_sliderCursor2_idx, tiles_sliderCursor1_idx);
  // btn->aabb.r = btn->aabb.l + slider2->width * nibs * 2.f;
  // btn->onDrag = []() {

  // };
  // renderer.menu.writeUI(vert, slider3, dx, dy, dz, 2, 2, 0, 0);
  // vert += 6
}

static void setTargetLeft() {
  renderer.screenBounds[0] = 200.f;
  renderer.screenBounds[1] = 120.f;

  screenProj = &leftProj;
  renderer.screenProj = screenProj;

  curTarget = leftTarget;
}

static void setTargetBot() {
  renderer.screenBounds[0] = 160.f;
  renderer.screenBounds[1] = 120.f;

  screenProj = &botProj;
  renderer.screenProj = screenProj;

  curTarget = botTarget;
}

static int frame = 0;
int main(int argc, char **argv) {
  gfxInitDefault();
  atexit(gfxExit);

  if (
    !std::filesystem::exists("/3dscii/") || 
    !std::filesystem::exists("/3dscii/art/") || 
    !std::filesystem::exists("/3dscii/charsets/") ||
    !std::filesystem::exists("/3dscii/palettes/")
  ) {
    consoleInit(GFX_BOTTOM, NULL);
    printf("\x1b[15;8HPlease make sure you placed");
    printf("\x1b[16;5Hthe \"3dscii\" folder into the root");
    printf("\x1b[17;14Hof your SD card");
    printf("\x1b[30;11HPress Start to exit.");

    while (aptMainLoop()) {
      hidScanInput();
      if (hidKeysDown() & KEY_START)
        break;

      gfxFlushBuffers();
      gfxSwapBuffers();
      gspWaitForVBlank();
    }

    return -1;
  }

  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
  atexit(C3D_Fini);
  
  if (!FileUtils::loadConfigFile("/3dscii/config.ini", &cfg)) {
    FileUtils::saveConfigFile("/3dscii/config.ini", &cfg);
  }

  if (cfg.lastArtwork == "" || !std::filesystem::exists(cfg.lastArtwork)) {
    cfg.lastArtwork = std::string(ART_PATH) + "file.3dscii";
    configChanged = true;
  } 
  artworkPath = cfg.lastArtwork;
  
  // consoleInit(GFX_BOTTOM, NULL);

  setupRenderTargets();

  SOC_buffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
  if (SOC_buffer == NULL) {
    failExit("memalign: failed to allocate\n");
  }

  int res = socInit(SOC_buffer, SOC_BUFFERSIZE);
  if (res != 0) {
    failExit("socInit: 0x%08X\n", (unsigned int)res);
  }

  atexit(socShutdown);

  renderer.asciiData = &asciiData;
  renderer.ui.maxElements = 128;
  renderer.menu.maxElements = 128;
  renderer.init();

  fileExplorer = FileExplorer(&renderer.files);

  if (std::filesystem::exists(artworkPath)) {
    load(artworkPath, false);
  } else {
    //TODO: check if these charset and pallete even exist
    asciiData.charset.name = "agat";
    asciiData.palette.name = "pico8";
    asciiData.width = 64;
    asciiData.height = 64;

    insertJunkLayer();

    loadCharset(false);
    loadPalette();

    renderer.initCharPicker();
    renderer.canvas.readLayers(&renderer);
  }

  initUI();
  initMenu();
  
  renderer.canvas.scale[0] = renderer.canvas.scale[1] = 2.f;
  renderer.canvas.position[0] = -asciiData.width * asciiData.charWidth * renderer.canvas.scale[0] / 2.f;
  renderer.canvas.position[1] = -asciiData.height * asciiData.charHeight * renderer.canvas.scale[1] / 2.f;

  hint = ASCII::Hint(&renderer.hint);
  ASCII::Renderer* rendererRef = &renderer;
  hint.writeFrame = [rendererRef](float width) {
    auto left = Tex3DS_GetSubTexture(rendererRef->tilesT3x, tiles_hintFrame1_idx);
    auto center = Tex3DS_GetSubTexture(rendererRef->tilesT3x, tiles_hintFrame2_idx);
    auto right = Tex3DS_GetSubTexture(rendererRef->tilesT3x, tiles_hintFrame3_idx);
    
    renderer.hint.frame.write(rendererRef, width, left, center, right);
  };

  // centered so that we don't need to move stuff around when swapping top & bottom screens
  Mtx_OrthoTilt(&leftProj, -200.0f, 200.0f, 120.0f, -120.0f, 9999.0f, -9999.0f, true);
  Mtx_OrthoTilt(&botProj, -160.0f, 160.0f, 120.0f, -120.0f, 9999.0f, -9999.0f, true);

  TickCounter ticks;
  osTickCounterStart(&ticks);

  bool menuOpen = false;
  bool swapScreens = false;
  circlePosition circle;
  while (aptMainLoop()) {
    osTickCounterUpdate(&ticks);
    double deltaTime = osTickCounterRead(&ticks);
    double dt = deltaTime / 1000.;
    // hint.setText(std::to_string((int)(deltaTime * 1000)).c_str());
    // hint.slide = 1.f;

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C3D_RenderTargetClear(leftTarget, C3D_CLEAR_ALL, 0x000000FF, 0);
    C3D_RenderTargetClear(botTarget, C3D_CLEAR_ALL, 0x000000FF, 0);

    hidScanInput();

    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    hidCircleRead(&circle);

    ui.prevTouch = ui.touch;
    hidTouchRead(&ui.touch);

    #ifdef THREEDSCII_DEBUG
    if (kDown & KEY_START) 
      break;
    #endif
    if (
      kDown & KEY_SELECT 
      #ifndef THREEDSCII_DEBUG
      || kDown & KEY_START
      #endif
    ) {
      menuOpen = !menuOpen;
      if (!menuOpen) {
        if (explorerOpen) {
          explorerOpen = false;
          fileExplorer.close();
        }
        saveConfigChanges();
      }
    }

    if (cfg.holdBumpers) {
      swapScreens = kHeld & (KEY_L | KEY_R);
    } else if (kDown & (KEY_L | KEY_R)) {
      swapScreens = !swapScreens;
    }
    swapScreens = swapScreens || menuOpen;

    if (kDown & KEY_DLEFT)
      undo();
    if (kDown & KEY_DRIGHT)
      redo();

    if (!swapScreens) {
      hint.show = false;
      hint.slide = 0.f;
    }
    if (cfg.hintsEnabled /* && !hint.showAlways */) {
      hint.update(dt);
    }

    if (!swapScreens) {
      if (std::abs(circle.dx) > 20 || std::abs(circle.dy) > 20) {
        renderer.canvas.position[0] -= circle.dx * dt;
        renderer.canvas.position[1] += circle.dy * dt;
      }

      ui.handleCanvasScreen();
    } else {
      if (std::abs(circle.dx) > 20 || std::abs(circle.dy) > 20) {
        renderer.charPicker.position[0] -= circle.dx * dt;
        renderer.charPicker.position[1] += circle.dy * dt;
      }

      if (menuOpen) {
        if (!explorerOpen) {
          ui.handleMenu();
        } else {
          fileExplorer.update();
          ui.handleFiles(fileExplorer.btns);
        }
      } else {
        ui.handlePickerScreen();
      }
    }

    // render charset screen
    if (!swapScreens) {
      setTargetLeft();
    } else {
      setTargetBot();
    }

    C3D_FrameDrawOn(curTarget);

    renderer.renderCharPicker();
    renderer.ui.render(&renderer);
    renderer.colorPicker.render(&renderer);

    if (menuOpen) {
      renderer.menu.background.render(&renderer);
      if (!explorerOpen) {
        renderer.menu.render(&renderer);
      } else {
        renderer.files.render(&renderer);
      }
    }

    // render canvas screen
    if (!swapScreens) {
      setTargetBot();
    } else {
      setTargetLeft();
    }

    C3D_FrameDrawOn(curTarget);

    renderer.renderCanvas();
    renderer.hint.frame.render(&renderer);
    renderer.hint.render(&renderer);

    C3D_FrameEnd(0);
  }
}

static void failExit(const char *fmt, ...) {
  va_list ap;
  
  printf(CONSOLE_RED);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf(CONSOLE_RESET);
  printf("\nPress B to exit\n");

  while (aptMainLoop()) {
    gspWaitForVBlank();
    hidScanInput();

    u32 kDown = hidKeysDown();
    if (kDown & KEY_B) exit(0);
  }
}
