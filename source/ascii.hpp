#pragma once

#include <3ds/types.h>
#include <string>
#include <vector>
#include <array>
#include <cstring>

#define ASCII_PALETTE_MAXCOLORS (255)

namespace ASCII {
  typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
  } Color;

  class Palette {
  public:
    std::string name;
    std::array<Color, ASCII_PALETTE_MAXCOLORS> colors;
    u8 size = 0;
    
    bool push(Color color);
  };

  typedef struct {
    std::string name;
    std::string tilesetName;
    u16 width;
    u16 height;
    u16 texWidth;
    u16 texHeight;
  } Charset;

  typedef struct {
    u16 value;
    u8 fgColor;
    u8 bgColor;
    u8 xFlip : 1;
    u8 yFlip : 1;
    u8 dFlip : 1;
    u8 invColors : 1;
  } Cell;

  typedef struct {
    std::string name;
    float depthOffset;
    float alpha;
    u8 visible : 1;
    std::vector<Cell> data;
  } Layer;

  class Data {
  public:
    std::string name;
    u8 width;
    u8 height;
    Charset charset;
    u8 charWidth;
    u8 charHeight;
    Palette palette;
    std::vector<Layer> layers;

    int serialize(u8*& data);
    bool parse(u8* data, int size);
  };
};
