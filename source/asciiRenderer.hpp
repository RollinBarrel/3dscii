#pragma once

#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <tex3ds.h>
#include "ascii.hpp"

#include "charset_shbin.h"
#include "solid_shbin.h"
#include "ui_shbin.h"
#include "uisolid_shbin.h"
#include "offset_shbin.h"

#include "tiles.h"
#include "tiles_t3x.h"

namespace ASCII {

class Renderer {
public:
  struct IRenderable {
    virtual void createBuf(Renderer* renderer) = 0;
    virtual void render(Renderer* renderer) = 0;
  };

  struct IPosScale {
    float position[2];
    float scale[2];
  };

  struct IShader {
    virtual void init(Renderer* renderer) = 0;
  };

  typedef struct {
    float position[2];
    float texcoord[2];
    float charpos[2];
    float color[4];
  } CharVertex;

  typedef struct {
    float position[2];
    float color[4];
  } ColoredVertex;

  typedef struct {
    float position[3];
    float texcoord[2];
    float edge[2];
  } UITextureVertex;

  typedef struct {
    float position[3];
    float color[4];
    float edge[2];
  } UIColoredVertex;

  typedef struct {
    float position[3];
    float texcoord[2];
    float offsetMul[2];
    float color[4];
  } OffsetTextureVertex;

  C3D_Mtx* screenProj;
  float screenBounds[2];

  C3D_Tex charsetTex;
  float charSize[2];
  bool charsetTexInitialized = false;
  C3D_Tex tilesTex;
  Tex3DS_Texture tilesT3x;

  ASCII::Data* asciiData;

  struct CharShader : public IShader {
    shaderProgram_s shader;
    C3D_AttrInfo attrInfo;
    
    u8 proj;
    u8 tranScale;
    u8 charSize;
    u8 xFlip;
    u8 yFlip;
    u8 dFlip;

    void init(Renderer* renderer);
  } charShader;

  struct SolidShader : public IShader {
    shaderProgram_s shader;
    C3D_AttrInfo attrInfo;
    
    u8 proj;
    u8 tranScale;
    u8 zOffset;

    void init(Renderer* renderer);
  } solidShader;

  struct UIShader : public IShader {
    shaderProgram_s shader;
    C3D_AttrInfo attrInfo;

    u8 proj;
    u8 distToEdge;

    void init(Renderer* renderer);
  } uiShader;

  struct UISolidShader : public IShader {
    shaderProgram_s shader;
    C3D_AttrInfo attrInfo;

    u8 proj;
    u8 distToEdge;

    void init(Renderer* renderer);
  } uiSolidShader;

  struct OffsetShader : public IShader {
    shaderProgram_s shader;
    C3D_AttrInfo attrInfo;

    u8 proj;
    u8 offset;

    void init(Renderer* renderer);
  } offsetShader;

  struct RCharPicker : public IRenderable, IPosScale {
    CharVertex* vbo;
    C3D_BufInfo bufInfo;
    bool initialized = false;

    int selected[2] = {0, 0};
    bool xFlip = false;
    bool yFlip = false;
    bool dFlip = false;

    void createBuf(Renderer* renderer);
    void render(Renderer* renderer);
  
    struct RBackground : public IRenderable {
      RCharPicker* owner;

      ColoredVertex* vbo;
      C3D_BufInfo bufInfo;
      
      void writeColor(float r, float g, float b, float a);
      void resize(float w, float h);
      void resize(Renderer* renderer);
      void createBuf(Renderer* renderer);
      void render(Renderer* renderer);
    } background;

    struct RSelection : public IRenderable {
      RCharPicker* owner;
      int selected[2];

      ColoredVertex* vbo;
      C3D_BufInfo bufInfo;

      void writeBuf(Renderer* renderer);
      void createBuf(Renderer* renderer);
      void render(Renderer* renderer);
    } selection;
  } charPicker;

  struct RCanvasLayer : public IRenderable {
    ASCII::Layer* layer;

    CharVertex* vbo;
    C3D_BufInfo bufInfo;

    bool active = false;
    float zOffset = 0.f;

    void writeCell(int index, ASCII::Data* asciiData);
    void createBuf(Renderer* renderer);
    void render(Renderer* renderer);
    void freeBuf();
  
    struct RBackground : public IRenderable {
      ASCII::Layer* layer;

      ColoredVertex* vbo;
      C3D_BufInfo bufInfo;

      float zOffset = 0.f;

      void createBuf(Renderer* renderer);
      void render(Renderer* renderer);
      void freeBuf();
    } background;
  };

  struct Canvas : public IPosScale {
    std::vector<RCanvasLayer> canvasLayers;

    void readLayers(Renderer* renderer);
  } canvas;

  struct RUI : public IRenderable {
    UITextureVertex* vbo;
    C3D_BufInfo bufInfo;

    int maxElements = 0;

    void writeUI(UITextureVertex* vert, const Tex3DS_SubTexture* tex, float x, float y, float z, float xs, float ys, float xe, float ye);
    void createBuf(Renderer* renderer);
    void render(Renderer* renderer);
  } ui;

  struct RColorPicker : public IRenderable {
    UIColoredVertex* vbo;
    C3D_BufInfo bufInfo;

    void writeColor(UIColoredVertex* vert, float r, float g, float b, float a, float x, float y, float z, float w, float h, float xe, float ye);
    void writeColorsFromPalette(UIColoredVertex* vert, ASCII::Color* colors, u8 num, u8 palSize);
    void createBuf(Renderer* renderer);
    void render(Renderer* renderer);
  } colorPicker;

  struct RHint : public IRenderable {
    OffsetTextureVertex* vbo;
    C3D_BufInfo bufInfo;

    int maxChars = 128;
    C2D_TextBuf textBuf;
    C2D_Text text;
    float width = 0.f;

    float offset[2] = {0.f, 0.f};

    void writeText(const char* content);
    void createBuf(Renderer* renderer);
    void render(Renderer* renderer);

    struct RFrame : public IRenderable {
      RHint* owner;

      OffsetTextureVertex* vbo;
      C3D_BufInfo bufInfo;

      void write(Renderer* renderer, float width, const Tex3DS_SubTexture* left, const Tex3DS_SubTexture* center, const Tex3DS_SubTexture* right);
      void createBuf(Renderer* renderer);
      void render(Renderer* renderer);
    } frame;
  } hint;

  struct RMenu : public IRenderable {
    UITextureVertex* vbo;
    C3D_BufInfo bufInfo;

    int maxElements = 0;

    void writeUI(UITextureVertex* vert, const Tex3DS_SubTexture* tex, float x, float y, float z, float xs, float ys, float xe, float ye);
    void createBuf(Renderer* renderer);
    void render(Renderer* renderer);

    struct RBackground : public IRenderable {
      ColoredVertex* vbo;
      C3D_BufInfo bufInfo;
      
      void createBuf(Renderer* renderer);
      void render(Renderer* renderer);
    } background;
  } menu;

  struct RFiles : public IRenderable {
    static const int maxElements = 12;//64 + 5;

    OffsetTextureVertex* vbo;
    C3D_BufInfo bufInfo;

    int maxChars = maxElements * 64;
    C2D_TextBuf textBuf;
    C2D_Text text[maxElements];
    float textColor[maxElements][4];
    bool prepared = false;

    float offset[2] = {0.f, 0.f};

    void writeColor(int index, u32 color);
    void writeText(int index, const char* content, u32 color);
    void clear() {C2D_TextBufClear(textBuf);};
    void prepare();
    void createBuf(Renderer* renderer);
    void render(Renderer* renderer);

    // struct RIcons : public IRenderable {
    //   OffsetTextureVertex* vbo;
    //   C3D_BufInfo bufInfo;

    //   void writeIcon(OffsetTextureVertex* vert, const Tex3DS_SubTexture* tex, float x, float y, float z);
    //   void createBuf(Renderer* renderer);
    //   void render(Renderer* renderer);
    // } icons;
  } files;

  void initCharPicker();
  void init();
  void renderCharPicker();
  void renderCanvas();
};

}
