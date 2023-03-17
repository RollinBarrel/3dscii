#include "ascii.hpp"

bool ASCII::Palette::push(ASCII::Color color) {
  if (size >= ASCII_PALETTE_MAXCOLORS)
    return false;
  bool found = false;
  for (int i = 0; i < size; i++) {
    if (
      color.r == colors[i].r && color.g == colors[i].g &&
      color.b == colors[i].b && color.a == colors[i].a
    ) {
      found = true;
      break;
    }
  }

  if (!found)  {
    colors[size++] = color;
    return true;
  }
  return false;
}

int ASCII::Data::serialize(u8*& data) {
  u8 nameSize = name.size();
  u8 charsetNameSize = charset.name.size();
  u8 paletteNameSize = palette.name.size();

  int layerNamesSize = 0;
  for (unsigned int l = 0; l < layers.size(); l++) {
    layerNamesSize += layers[l].name.size();
  }

  data = (u8*)malloc(
    6 + // "3DSCII"
    2 + // rev
    1 + nameSize + // name
    2 + // dimensions
    1 + charsetNameSize + // charset name
    1 + paletteNameSize + // palette name
    ( // layers
      1 + // layer name size
      4 + // depth offset
      4 + // layer alpha
      1 + // layer flags
      (
        ( // cells
        2 + // value
        1 + // foreground
        1 + // background
        1 // cell flags
        ) * (width * height)
      )
    ) * layers.size()
    + layerNamesSize // sum of all palettes names' sizes
  );
  int b = 0;

  // header

  // "3DSCII" 6 bytes
  data[b++] = '3'; data[b++] = 'D'; data[b++] = 'S';
  data[b++] = 'C'; data[b++] = 'I'; data[b++] = 'I';

  // .3dscii file format revision 2 bytes
  *(u16*)&data[b] = 4;
  b += sizeof(u16);

  // name 1 + #nameSize bytes
  data[b++] = nameSize;
  std::memcpy(&data[b], name.c_str(), nameSize);
  b += nameSize;

  // artwork dimensions 2 bytes
  data[b++] = width;
  data[b++] = height;

  // charsetName 1 + #charsetNameSize bytes
  data[b++] = charsetNameSize;
  std::memcpy(&data[b], charset.name.c_str(), charsetNameSize);
  b += charsetNameSize;

  // paletteName 1 + #paletteNameSize bytes
  data[b++] = paletteNameSize;
  std::memcpy(&data[b], palette.name.c_str(), paletteNameSize);
  b += paletteNameSize;

  // layerCount 1 byte
  data[b++] = layers.size();

  // layers

  for (auto &layer : layers) {
    // layerName 1 + #layerNameSize bytes
    int layerNameSize = layer.name.size();
    data[b++] = layerNameSize;
    std::memcpy(&data[b], layer.name.c_str(), layerNameSize);
    b += layerNameSize;

    // depthOffset 4 bytes
    *(float*)&data[b] = layer.depthOffset;
    b += sizeof(layer.depthOffset);

    // alpha 4 bytes
    *(float*)&data[b] = layer.alpha;
    b += sizeof(layer.alpha);

    // flags 1 byte
    data[b++] = layer.visible;

    // cell data
    for (int i = 0; i < width * height; i++) {
      // value 2 bytes
      Cell cell = layer.data[i];
      *((u16*)&data[b]) = cell.value;
      b += sizeof(cell.value);

      // fgColor 1 byte
      data[b++] = cell.fgColor;

      // bgColor 1 byte
      data[b++] = cell.bgColor;

      // flags 1 byte
      data[b++] = cell.xFlip | (cell.yFlip << 1) | (cell.dFlip << 2) | (cell.invColors << 3);
    }
  }

  return b;
}

//TODO: check for oob reads
bool ASCII::Data::parse(u8* data, int size) {
  int b = 0;

  if (
    data[b++] != '3' || data[b++] != 'D' || data[b++] != 'S' ||
    data[b++] != 'C' || data[b++] != 'I' || data[b++] != 'I' 
  ) {
    return false;
  }

  u16 rev = *(u16*)&data[b];
  b += 2;

  switch (rev) {
    // handle previous revisions
    case 4:
    break;
    default:
      return false;
    break;
  }

  u8 nameSize = data[b++];
  name = std::string((char*)&data[b], nameSize);
  b += nameSize;

  width = data[b++];
  height = data[b++];

  u8 charsetNameSize = data[b++];
  charset.name = std::string((char*)&data[b], charsetNameSize);
  b += charsetNameSize;

  u8 paletteNameSize = data[b++];
  palette.name = std::string((char*)&data[b], paletteNameSize);
  b += paletteNameSize;

  u8 layerCount = data[b++];
  layers = std::vector<Layer>();
  for (int l = 0; l < layerCount; l++) {
    auto layer = &layers.emplace_back();
    
    u8 layerNameSize = data[b++];
    layer->name = std::string((char*)&data[b], layerNameSize);
    b += layerNameSize;

    layer->depthOffset = *(float*)&data[b];
    b += 4;

    layer->alpha = *(float*)&data[b];
    b += 4;

    u8 flags = data[b++];
    layer->visible = (flags & (1 << 0)) > 0;
    
    layer->data = std::vector<Cell>(width * height);
    for (int c = 0; c < width * height; c++) {
      Cell* cell = &layer->data[c];

      cell->value = *((u16*)&data[b]);
      b += 2;

      cell->fgColor = data[b++];
      cell->bgColor = data[b++];

      u8 flags = data[b++];
      cell->xFlip = (flags & (1 << 0)) > 0;
      cell->yFlip = (flags & (1 << 1)) > 0;
      cell->dFlip = (flags & (1 << 2)) > 0;
      cell->invColors = (flags & (1 << 3)) > 0;
    }
  }

  return true;
}