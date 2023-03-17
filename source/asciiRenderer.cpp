//TODO: use IBO's to download more ram
//TODO: canvas layers, solid and charset shaders have hacked-in zOffset,
// which is redundant since drawing over stuff works just fine
#include "asciiRenderer.hpp"

typedef struct C2Di_Glyph_s
{
	u32 lineNo;
	C3D_Tex* sheet;
	float xPos;
	float width;
	struct
	{
		float left, top, right, bottom;
	} texcoord;
	u32 wordNo;
} C2Di_Glyph;

struct C2D_TextBuf_s
{
	u32 reserved[2];
	size_t glyphCount;
	size_t glyphBufSize;
	C2Di_Glyph glyphs[0];
};

namespace ASCII {

void Renderer::CharShader::init(Renderer* renderer) {
  DVLB_s* shaderDVLB = DVLB_ParseFile((u32*)charset_shbin, charset_shbin_size);
  shaderProgramInit(&shader);
  shaderProgramSetVsh(&shader, &shaderDVLB->DVLE[0]);

  AttrInfo_Init(&attrInfo);
  AttrInfo_AddLoader(&attrInfo, 0, GPU_FLOAT, 2); // v0=position
  AttrInfo_AddLoader(&attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
  AttrInfo_AddLoader(&attrInfo, 2, GPU_FLOAT, 2); // v2=charpos
  AttrInfo_AddLoader(&attrInfo, 3, GPU_FLOAT, 4); // v3=color

  proj = shaderInstanceGetUniformLocation(shader.vertexShader, "proj");
  tranScale = shaderInstanceGetUniformLocation(shader.vertexShader, "tranScale");
  charSize = shaderInstanceGetUniformLocation(shader.vertexShader, "charSize");
  xFlip = shaderInstanceGetUniformLocation(shader.vertexShader, "xFlip");
  yFlip = shaderInstanceGetUniformLocation(shader.vertexShader, "yFlip");
  dFlip = shaderInstanceGetUniformLocation(shader.vertexShader, "dFlip");
}

void Renderer::SolidShader::init(Renderer* renderer) {
  DVLB_s* shaderDVLB = DVLB_ParseFile((u32*)solid_shbin, solid_shbin_size);
  shaderProgramInit(&shader);
  shaderProgramSetVsh(&shader, &shaderDVLB->DVLE[0]);

  AttrInfo_Init(&attrInfo);
  AttrInfo_AddLoader(&attrInfo, 0, GPU_FLOAT, 2); // v0=position
  AttrInfo_AddLoader(&attrInfo, 1, GPU_FLOAT, 4); // v1=color

  proj = shaderInstanceGetUniformLocation(shader.vertexShader, "proj");
  tranScale = shaderInstanceGetUniformLocation(shader.vertexShader, "tranScale");
  zOffset = shaderInstanceGetUniformLocation(shader.vertexShader, "zOffset");
}

void Renderer::UIShader::init(Renderer* renderer) {
  DVLB_s* shaderDVLB = DVLB_ParseFile((u32*)ui_shbin, ui_shbin_size);
  shaderProgramInit(&shader);
  shaderProgramSetVsh(&shader, &shaderDVLB->DVLE[0]);

  AttrInfo_Init(&attrInfo);
  AttrInfo_AddLoader(&attrInfo, 0, GPU_FLOAT, 3); // v0=position
  AttrInfo_AddLoader(&attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
  AttrInfo_AddLoader(&attrInfo, 2, GPU_FLOAT, 2); // v2=edge

  proj = shaderInstanceGetUniformLocation(shader.vertexShader, "proj");
  distToEdge = shaderInstanceGetUniformLocation(shader.vertexShader, "distToEdge");
}

void Renderer::UISolidShader::init(Renderer* renderer) {
  DVLB_s* shaderDVLB = DVLB_ParseFile((u32*)uisolid_shbin, uisolid_shbin_size);
  shaderProgramInit(&shader);
  shaderProgramSetVsh(&shader, &shaderDVLB->DVLE[0]);

  AttrInfo_Init(&attrInfo);
  AttrInfo_AddLoader(&attrInfo, 0, GPU_FLOAT, 3); // v0=position
  AttrInfo_AddLoader(&attrInfo, 1, GPU_FLOAT, 4); // v1=color
  AttrInfo_AddLoader(&attrInfo, 2, GPU_FLOAT, 2); // v2=edge

  proj = shaderInstanceGetUniformLocation(shader.vertexShader, "proj");
  distToEdge = shaderInstanceGetUniformLocation(shader.vertexShader, "distToEdge");
}

void Renderer::OffsetShader::init(Renderer* renderer) {
  DVLB_s* shaderDVLB = DVLB_ParseFile((u32*)offset_shbin, offset_shbin_size);
  shaderProgramInit(&shader);
  shaderProgramSetVsh(&shader, &shaderDVLB->DVLE[0]);

  AttrInfo_Init(&attrInfo);
  AttrInfo_AddLoader(&attrInfo, 0, GPU_FLOAT, 3); // v0=position
  AttrInfo_AddLoader(&attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
  AttrInfo_AddLoader(&attrInfo, 2, GPU_FLOAT, 2); // v2=offsetMul
  AttrInfo_AddLoader(&attrInfo, 3, GPU_FLOAT, 4); // v3=color

  proj = shaderInstanceGetUniformLocation(shader.vertexShader, "proj");
  offset = shaderInstanceGetUniformLocation(shader.vertexShader, "offset");
}

void Renderer::RCharPicker::createBuf(Renderer* renderer) {
  ASCII::Data* asciiData = renderer->asciiData;
  
  vbo = (CharVertex*)linearAlloc(asciiData->charset.width * asciiData->charset.height * sizeof(CharVertex) * 6);

  CharVertex* vert = vbo;
  for (int x = 0; x < asciiData->charset.width; x++) {
    for (int y = 0; y < asciiData->charset.height; y++) {
      float tx = (float)(x * asciiData->charWidth);
      float ty = (float)(y * asciiData->charHeight);

      float w = (float)asciiData->charWidth;
      float h = (float)asciiData->charHeight;

      vert[0] = {{tx,     ty},     {0, 0}, {(float)x, (float)y}, {1, 1, 0, 1}};
      vert[1] = {{tx,     ty + h}, {0, 1}, {(float)x, (float)y}, {1, 1, 0, 1}};
      vert[2] = {{tx + w, ty + h}, {1, 1}, {(float)x, (float)y}, {1, 1, 0, 1}};

      vert[3] = {{tx + w, ty + h}, {1, 1}, {(float)x, (float)y}, {1, 1, 0, 1}};
      vert[4] = {{tx + w, ty},     {1, 0}, {(float)x, (float)y}, {1, 1, 0, 1}};
      vert[5] = {{tx,     ty},     {0, 0}, {(float)x, (float)y}, {1, 1, 0, 1}};

      vert += 6;
    }
  }

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(CharVertex), 4, 0x3210);
}

void Renderer::RCharPicker::render(Renderer* renderer) {
  auto charShader = &renderer->charShader;
  ASCII::Data* asciiData = renderer->asciiData;

  C3D_BindProgram(&charShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&charShader->attrInfo);

  C3D_TexBind(0, &renderer->charsetTex);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, charShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, charShader->tranScale, std::floor(position[0]), std::floor(position[1]), scale[0], scale[1]);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, charShader->charSize, renderer->charSize[0], renderer->charSize[1], 0, 0);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, charShader->xFlip, xFlip);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, charShader->yFlip, yFlip);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, charShader->dFlip, dFlip);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
  C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
  C3D_TexEnvOpAlpha(env, GPU_TEVOP_A_SRC_R);
  C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, asciiData->charset.width * asciiData->charset.height * 6);
}

void Renderer::RCharPicker::RBackground::writeColor(float r, float g, float b, float a) {
  for (int i = 0; i < 6; i++) {
    vbo[i].color[0] = r;
    vbo[i].color[1] = g;
    vbo[i].color[2] = b;
    vbo[i].color[3] = a;
  }
}

void Renderer::RCharPicker::RBackground::resize(float w, float h) {
  vbo[2].position[0] = w;
  vbo[3].position[0] = w;
  vbo[4].position[0] = w;

  vbo[1].position[1] = h;
  vbo[2].position[1] = h;
  vbo[3].position[1] = h;
}

void Renderer::RCharPicker::RBackground::resize(Renderer* renderer) {
  float w = (float)renderer->asciiData->charset.texWidth;
  float h = (float)renderer->asciiData->charset.texHeight;
  resize(w, h);
}

void Renderer::RCharPicker::RBackground::createBuf(Renderer* renderer) {
  vbo = (ColoredVertex*)linearAlloc(sizeof(ColoredVertex) * 6);

  float w = (float)renderer->asciiData->charset.texWidth;
  float h = (float)renderer->asciiData->charset.texHeight;

  vbo[0] = {{0, 0}, {1, 0, 1, 1}};
  vbo[1] = {{0, h}, {1, 0, 1, 1}};
  vbo[2] = {{w, h}, {1, 0, 1, 1}};

  vbo[3] = {{w, h}, {1, 0, 1, 1}};
  vbo[4] = {{w, 0}, {1, 0, 1, 1}};
  vbo[5] = {{0, 0}, {1, 0, 1, 1}};

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(ColoredVertex), 2, 0x10);
}

void Renderer::RCharPicker::RBackground::render(Renderer* renderer) {
  auto solidShader = &renderer->solidShader;

  C3D_BindProgram(&solidShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&solidShader->attrInfo);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, solidShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, solidShader->tranScale, std::floor(owner->position[0]), std::floor(owner->position[1]), owner->scale[0], owner->scale[1]);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, 6);
}

void Renderer::RCharPicker::RSelection::writeBuf(Renderer* renderer) {
  ASCII::Data* asciiData = renderer->asciiData;

  float cw = asciiData->charWidth;
  float ch = asciiData->charHeight;
  
  float tx = selected[0] * cw;
  float ty = selected[1] * ch;

  // upper line
  vbo[0] = {{tx - 1,      ty - 1}, {1, 0, 0, 1}};
  vbo[1] = {{tx - 1,      ty},     {1, 0, 0, 1}};
  vbo[2] = {{tx + cw + 1, ty},     {1, 0, 0, 1}};

  vbo[3] = {{tx + cw + 1, ty},     {1, 0, 0, 1}};
  vbo[4] = {{tx + cw + 1, ty - 1}, {1, 0, 0, 1}};
  vbo[5] = {{tx - 1,      ty - 1}, {1, 0, 0, 1}};
  // bottom line
  vbo[6] =  {{tx - 1,      ty + ch},     {1, 0, 0, 1}};
  vbo[7] =  {{tx - 1,      ty + ch + 1}, {1, 0, 0, 1}};
  vbo[8] =  {{tx + cw + 1, ty + ch + 1}, {1, 0, 0, 1}};

  vbo[9] =  {{tx + cw + 1, ty + ch + 1}, {1, 0, 0, 1}};
  vbo[10] = {{tx + cw + 1, ty + ch},     {1, 0, 0, 1}};
  vbo[11] = {{tx - 1,      ty + ch},     {1, 0, 0, 1}};
  // left line
  vbo[12] = {{tx - 1, ty - 1},      {1, 0, 0, 1}};
  vbo[13] = {{tx - 1, ty + ch + 1}, {1, 0, 0, 1}};
  vbo[14] = {{tx,     ty + ch + 1}, {1, 0, 0, 1}};

  vbo[15] = {{tx,     ty + ch + 1}, {1, 0, 0, 1}};
  vbo[16] = {{tx,     ty - 1},      {1, 0, 0, 1}};
  vbo[17] = {{tx - 1, ty - 1},      {1, 0, 0, 1}};
  // right line
  vbo[18] = {{tx + cw,     ty - 1},      {1, 0, 0, 1}};
  vbo[19] = {{tx + cw,     ty + ch + 1}, {1, 0, 0, 1}};
  vbo[20] = {{tx + cw + 1, ty + ch + 1}, {1, 0, 0, 1}};

  vbo[21] = {{tx + cw + 1, ty + ch + 1}, {1, 0, 0, 1}};
  vbo[22] = {{tx + cw + 1, ty - 1},      {1, 0, 0, 1}};
  vbo[23] = {{tx + cw,     ty - 1},      {1, 0, 0, 1}};
}

void Renderer::RCharPicker::RSelection::createBuf(Renderer* renderer) {
  vbo = (ColoredVertex*)linearAlloc(sizeof(ColoredVertex) * 6 * 4);

  writeBuf(renderer);

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(ColoredVertex), 2, 0x10);
}

void Renderer::RCharPicker::RSelection::render(Renderer* renderer) {
  auto solidShader = &renderer->solidShader;

  C3D_BindProgram(&solidShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&solidShader->attrInfo);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, solidShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, solidShader->tranScale, std::floor(owner->position[0]), std::floor(owner->position[1]), owner->scale[0], owner->scale[1]);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, 6 * 4);
}

void Renderer::RCanvasLayer::writeCell(int index, ASCII::Data* asciiData) {
  int x = index % asciiData->width;
  int y = index / asciiData->width;
  ASCII::Cell* cell = &layer->data[index];

  // when the charset and/or palette switch and have less 
  // tiles/colors, we can get out of range tiles/colors.
  // rather than replacing them and losing data that potentially
  // can be utilised with subsequent charset/palette switches,
  // we'll just render them in a consistent way
  u16 tIdx = cell->value < (asciiData->charset.width * asciiData->charset.height) ? cell->value : 0;
  u8 bgIdx = cell->bgColor < asciiData->palette.size ? cell->bgColor : 0;
  u8 fgIdx = cell->fgColor < asciiData->palette.size ? cell->fgColor : 0;

  ASCII::Color bgCol = asciiData->palette.colors[bgIdx];
  ASCII::Color fgCol = asciiData->palette.colors[fgIdx];

  float bgR = 1.f / 255 * bgCol.r;
  float bgG = 1.f / 255 * bgCol.g;
  float bgB = 1.f / 255 * bgCol.b;
  float bgA = 1.f / 255 * bgCol.a;

  float fgR = 1.f / 255 * fgCol.r;
  float fgG = 1.f / 255 * fgCol.g;
  float fgB = 1.f / 255 * fgCol.b;
  float fgA = 1.f / 255 * fgCol.a;

  ColoredVertex* bgVert = &background.vbo[index * 6];
  CharVertex* fgVert = &vbo[index * 6];

  for (int i = 0; i < 6; i++) {
    bgVert[i].color[0] = bgR;
    bgVert[i].color[1] = bgG;
    bgVert[i].color[2] = bgB;
    bgVert[i].color[3] = bgA;
  }

  int cy = tIdx / asciiData->charset.width;
  int cx = tIdx % asciiData->charset.width;

  float w = (float)asciiData->charWidth;
  float h = (float)asciiData->charHeight;

  float tx = x * w;
  float ty = y * h;

  float ul = 0.f;
  float ur = 1.f;
  float vl = 0.f;
  float vr = 1.f;
  
  if ((cell->xFlip && !cell->dFlip) || (cell->yFlip && cell->dFlip)) {
    ul = 1.f;
    ur = 0.f;
  }
  if ((cell->yFlip && !cell->dFlip) || (cell->xFlip && cell->dFlip)) {
    vl = 1.f;
    vr = 0.f;
  }
  
  fgVert[0] = {{tx,     ty},     {ul, vl}, {(float)cx, (float)cy}, {fgR, fgG, fgB, fgA}};
  fgVert[1] = {{tx,     ty + h}, {ul, vr}, {(float)cx, (float)cy}, {fgR, fgG, fgB, fgA}};
  fgVert[2] = {{tx + w, ty + h}, {ur, vr}, {(float)cx, (float)cy}, {fgR, fgG, fgB, fgA}};

  fgVert[3] = {{tx + w, ty + h}, {ur, vr}, {(float)cx, (float)cy}, {fgR, fgG, fgB, fgA}};
  fgVert[4] = {{tx + w, ty},     {ur, vl}, {(float)cx, (float)cy}, {fgR, fgG, fgB, fgA}};
  fgVert[5] = {{tx,     ty},     {ul, vl}, {(float)cx, (float)cy}, {fgR, fgG, fgB, fgA}};

  if (cell->dFlip) {
    for (int i = 0; i < 6; i++) {
      float t = fgVert[i].texcoord[0];
      fgVert[i].texcoord[0] = fgVert[i].texcoord[1];
      fgVert[i].texcoord[1] = t;
    }
  }
}

void Renderer::RCanvasLayer::createBuf(Renderer* renderer) {
  ASCII::Data* asciiData = renderer->asciiData;
  
  vbo = (CharVertex*)linearAlloc(asciiData->width * asciiData->height * sizeof(CharVertex) * 6);

  float w = (float)asciiData->charWidth;
  float h = (float)asciiData->charHeight;

  CharVertex* vert = vbo;
  
  for (int y = 0; y < asciiData->height; y++) {
    float ty = y * h;
    for (int x = 0; x < asciiData->width; x++) {
      float tx = x * w;

      vert[0] = {{tx,     ty},     {}, {}, {}};
      vert[1] = {{tx,     ty + h}, {}, {}, {}};
      vert[2] = {{tx + w, ty + h}, {}, {}, {}};

      vert[3] = {{tx + w, ty + h}, {}, {}, {}};
      vert[4] = {{tx + w, ty},     {}, {}, {}};
      vert[5] = {{tx,     ty},     {}, {}, {}};

      vert += 6;
    }
  }

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(CharVertex), 4, 0x3210);
}

void Renderer::RCanvasLayer::render(Renderer* renderer) {
  auto charShader = &renderer->charShader;
  auto canvas = &renderer->canvas;
  ASCII::Data* asciiData = renderer->asciiData;

  C3D_BindProgram(&charShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&charShader->attrInfo);

  C3D_TexBind(0, &renderer->charsetTex);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, charShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, charShader->tranScale, std::floor(canvas->position[0]) + layer->depthOffset, std::floor(canvas->position[1]), canvas->scale[0], canvas->scale[1]);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, charShader->charSize, renderer->charSize[0], renderer->charSize[1], 0, 0);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, charShader->xFlip, false);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, charShader->yFlip, false);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, charShader->dFlip, false);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
  // C3D_AlphaTest(true, GPU_GREATER, 0);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
  C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
  C3D_TexEnvOpAlpha(env, GPU_TEVOP_A_SRC_R);
  C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, asciiData->width * asciiData->height * 6);
}

void Renderer::RCanvasLayer::RBackground::createBuf(Renderer* renderer) {
  ASCII::Data* asciiData = renderer->asciiData;

  vbo = (ColoredVertex*)linearAlloc(asciiData->width * asciiData->height * sizeof(ColoredVertex) * 6);

  float w = (float)asciiData->charWidth;
  float h = (float)asciiData->charHeight;

  ColoredVertex* vert = vbo;

  for (int y = 0; y < asciiData->height; y++) {
    float ty = y * h;
    for (int x = 0; x < asciiData->width; x++) {
      float tx = x * w;

      vert[0] = {{tx,     ty},     {}};
      vert[1] = {{tx,     ty + h}, {}};
      vert[2] = {{tx + w, ty + h}, {}};

      vert[3] = {{tx + w, ty + h}, {}};
      vert[4] = {{tx + w, ty},     {}};
      vert[5] = {{tx,     ty},     {}};

      vert += 6;
    }
  }

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(ColoredVertex), 2, 0x10);
}

void Renderer::RCanvasLayer::RBackground::render(Renderer* renderer) {
  auto solidShader = &renderer->solidShader;
  auto canvas = &renderer->canvas;
  ASCII::Data* asciiData = renderer->asciiData;

  C3D_BindProgram(&solidShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&solidShader->attrInfo);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, solidShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, solidShader->tranScale, std::floor(canvas->position[0]) + layer->depthOffset, std::floor(canvas->position[1]), canvas->scale[0], canvas->scale[1]);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, solidShader->zOffset, zOffset, 0, 0, 0);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
  // C3D_AlphaTest(true, GPU_GREATER, 0);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, asciiData->width * asciiData->height * 6);
}

void Renderer::Canvas::readLayers(Renderer* renderer) {
  for (auto& layer : canvasLayers) {
    if (layer.active) {
      linearFree(layer.vbo);
      linearFree(layer.background.vbo);
      layer.active = false;
    }
  }

  size_t layerCount = renderer->asciiData->layers.size();
  if (layerCount > 8) {
    layerCount = 8;
    //TODO: show a hint that more than 8 layers are not supported
  }

  if (canvasLayers.size() < layerCount)
    canvasLayers.resize(layerCount);

  for (size_t i = 0; i < layerCount; ++i) {
    RCanvasLayer* layer = &canvasLayers[i];

    layer->active = true;

    layer->layer = &renderer->asciiData->layers[i];
    layer->background.layer = layer->layer;
    
    layer->createBuf(renderer);
    layer->background.createBuf(renderer);

    layer->zOffset = i;
    layer->background.zOffset = i;

    for (int y = 0; y < renderer->asciiData->height; y++) {
      for (int x = 0; x < renderer->asciiData->width; x++) {
        layer->writeCell(x + y * renderer->asciiData->width, renderer->asciiData);
      }
    }
  }
}

void Renderer::RUI::writeUI(UITextureVertex* vert, const Tex3DS_SubTexture* tex, float x, float y, float z, float xs, float ys, float xe, float ye) {
  float xr = x + tex->width * xs;
  float yr = y + tex->height * ys;

  vert[0] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}};
  vert[1] = {{x,  yr, z}, {tex->left,  tex->bottom}, {xe, ye}};
  vert[2] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}};

  vert[3] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}};
  vert[4] = {{xr, y,  z}, {tex->right, tex->top},    {xe, ye}};
  vert[5] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}};
}

void Renderer::RUI::createBuf(Renderer* renderer) {
  vbo = (UITextureVertex*)linearAlloc(sizeof(UITextureVertex) * maxElements * 6);

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(UITextureVertex), 3, 0x210);
}

void Renderer::RUI::render(Renderer* renderer) {
  auto uiShader = &renderer->uiShader;
  C3D_BindProgram(&uiShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&uiShader->attrInfo);

  C3D_TexBind(0, &renderer->tilesTex);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uiShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, uiShader->distToEdge, renderer->screenBounds[0], renderer->screenBounds[1], 0, 0);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
  C3D_AlphaTest(true, GPU_GREATER, 0);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, maxElements * 6);
}

void Renderer::RColorPicker::writeColor(UIColoredVertex* vert, float r, float g, float b, float a, float x, float y, float z, float w, float h, float xe, float ye) {
  float xr = x + w;
  float yr = y + h;

  vert[0] = {{x,  y,  z}, {r, g, b, a}, {xe, ye}};
  vert[1] = {{x,  yr, z}, {r, g, b, a}, {xe, ye}};
  vert[2] = {{xr, yr, z}, {r, g, b, a}, {xe, ye}};

  vert[3] = {{xr, yr, z}, {r, g, b, a}, {xe, ye}};
  vert[4] = {{xr, y,  z}, {r, g, b, a}, {xe, ye}};
  vert[5] = {{x,  y,  z}, {r, g, b, a}, {xe, ye}};
}

void Renderer::RColorPicker::writeColorsFromPalette(UIColoredVertex* vert, ASCII::Color* colors, u8 num, u8 palSize) {
  for (int i = 0; i < num; i++) {
    float col[4];
    if (i < palSize) {
      col[0] = 1.f / 255 * colors[i].r;
      col[1] = 1.f / 255 * colors[i].g;
      col[2] = 1.f / 255 * colors[i].b;
      col[3] = 1.f / 255 * colors[i].a;
    } else {
      for (int j = 0; j < 4; j++) 
        col[j] = 0;
    }

    for (int v = 0; v < 6; v++) {
      for (int c = 0; c < 4; c++) {
        vert[v].color[c] = col[c];
      }
    }
    vert += 6;
  }
}

void Renderer::RColorPicker::createBuf(Renderer* renderer) {
  vbo = (UIColoredVertex*)linearAlloc(sizeof(UIColoredVertex) * 18 * 6);

  auto vert = vbo;
  for (int i = 1; i <= 18; i++) {
    writeColor(vert, 0, 0, 0, 0, -24, -120 + 12 * i, 1, 24, 12, 1, 0);
    vert += 6;
  }

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(UIColoredVertex), 3, 0x210);
}

void Renderer::RColorPicker::render(Renderer* renderer) {
  auto uiSolidShader = &renderer->uiSolidShader;

  C3D_BindProgram(&uiSolidShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&uiSolidShader->attrInfo);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uiSolidShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, uiSolidShader->distToEdge, renderer->screenBounds[0], renderer->screenBounds[1], 0, 0);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, 18 * 6);
}

void Renderer::RHint::writeText(const char* content) {
  C2D_TextBufClear(textBuf);
  C2D_TextParse(&text, textBuf, content);
  C2D_TextOptimize(&text);

  CFNT_s* font = fontGetSystemFont();

  u8 h = fontGetGlyphInfo(font)->cellHeight;
  float s = 18.f / h;
  
  float y = 0.f;
  float z = 400.f;
  
  float xom = 1.f;
  float yom = 1.f;

  width = 0.f;

  OffsetTextureVertex* vert = vbo;
  C2Di_Glyph* begin = &textBuf->glyphs[text.begin];
  C2Di_Glyph* end = &textBuf->glyphs[text.end];
  for (C2Di_Glyph* cur = begin; cur != end; ++cur) {
    float x = cur->xPos * s;
    float xr = x + cur->width * s;
    float yr = h * s;

    if (width < xr)
      width = xr;

    vert[0] = {{x,  y,  z}, {cur->texcoord.left,  cur->texcoord.top},    {xom, yom}, {1, 1, 1, 1}};
    vert[1] = {{x,  yr, z}, {cur->texcoord.left,  cur->texcoord.bottom}, {xom, yom}, {1, 1, 1, 1}};
    vert[2] = {{xr, yr, z}, {cur->texcoord.right, cur->texcoord.bottom}, {xom, yom}, {1, 1, 1, 1}};

    vert[3] = {{xr, yr, z}, {cur->texcoord.right, cur->texcoord.bottom}, {xom, yom}, {1, 1, 1, 1}};
    vert[4] = {{xr, y,  z}, {cur->texcoord.right, cur->texcoord.top},    {xom, yom}, {1, 1, 1, 1}};
    vert[5] = {{x,  y,  z}, {cur->texcoord.left,  cur->texcoord.top},    {xom, yom}, {1, 1, 1, 1}};

    vert += 6;
  }
}

void Renderer::RHint::createBuf(Renderer* renderer) {
  vbo = (OffsetTextureVertex*)linearAlloc(sizeof(OffsetTextureVertex) * maxChars * 6);

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(OffsetTextureVertex), 4, 0x3210);

  textBuf = C2D_TextBufNew(maxChars);
}

//TODO: we can safely skip most initialization stuff here
// as long as we always render hint text after hint frame
void Renderer::RHint::render(Renderer* renderer) {
  if (text.words == 0)
    return;

  auto offsetShader = &renderer->offsetShader;
  C3D_BindProgram(&offsetShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&offsetShader->attrInfo);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, offsetShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, offsetShader->offset, offset[0], offset[1], 0, 0);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
  C3D_AlphaTest(true, GPU_GREATER, 0);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvColor(env, 0xFFFFFFFF);
  C3D_TexEnvSrc(env, C3D_RGB, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
  C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C2Di_Glyph* begin = &text.buf->glyphs[text.begin];
  C2Di_Glyph* end = &text.buf->glyphs[text.end];
  int i = 0;
  int from = 0;
  int count = 0;
  C3D_Tex* lastSheet = begin->sheet;
  for (C2Di_Glyph* cur = begin; cur != end; ++cur) {
    if (cur->sheet == lastSheet) {
      ++count;
    } else {
      C3D_TexBind(0, lastSheet);
      C3D_DrawArrays(GPU_TRIANGLES, from * 6, count * 6);

      lastSheet = cur->sheet;
      count = 1;
      from = i;
    }
    ++i;
  }

  C3D_TexBind(0, lastSheet);
  C3D_DrawArrays(GPU_TRIANGLES, from * 6, count * 6);
}

void Renderer::RHint::RFrame::write(Renderer* renderer, float width, const Tex3DS_SubTexture* left, const Tex3DS_SubTexture* center, const Tex3DS_SubTexture* right) {
  auto tex = left;
  float x = 0.f;
  float y = -4.f;
  float z = 401.f;
  float xr = x + tex->width;
  float yr = y + tex->height;
  float xe = 1.f;
  float ye = 1.f;

  auto vert = vbo;
  vert[0] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}, {1, 1, 1, 1}};
  vert[1] = {{x,  yr, z}, {tex->left,  tex->bottom}, {xe, ye}, {1, 1, 1, 1}};
  vert[2] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}, {1, 1, 1, 1}};

  vert[3] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}, {1, 1, 1, 1}};
  vert[4] = {{xr, y,  z}, {tex->right, tex->top},    {xe, ye}, {1, 1, 1, 1}};
  vert[5] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}, {1, 1, 1, 1}};
  
  vert += 6;

  tex = center;
  x = xr;
  z = 399.f;
  xr = x + width;
  yr = y + tex->height;

  vert[0] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}, {1, 1, 1, 1}};
  vert[1] = {{x,  yr, z}, {tex->left,  tex->bottom}, {xe, ye}, {1, 1, 1, 1}};
  vert[2] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}, {1, 1, 1, 1}};

  vert[3] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}, {1, 1, 1, 1}};
  vert[4] = {{xr, y,  z}, {tex->right, tex->top},    {xe, ye}, {1, 1, 1, 1}};
  vert[5] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}, {1, 1, 1, 1}};

  vert += 6;

  tex = right;
  x = xr;
  z = 401.f;
  xr = x + tex->width;
  yr = y + tex->height;

  vert[0] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}, {1, 1, 1, 1}};
  vert[1] = {{x,  yr, z}, {tex->left,  tex->bottom}, {xe, ye}, {1, 1, 1, 1}};
  vert[2] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}, {1, 1, 1, 1}};

  vert[3] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}, {1, 1, 1, 1}};
  vert[4] = {{xr, y,  z}, {tex->right, tex->top},    {xe, ye}, {1, 1, 1, 1}};
  vert[5] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}, {1, 1, 1, 1}};
}

void Renderer::RHint::RFrame::createBuf(Renderer* renderer) {
  vbo = (OffsetTextureVertex*)linearAlloc(sizeof(OffsetTextureVertex) * 3 * 6);

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(OffsetTextureVertex), 4, 0x3210);
}

void Renderer::RHint::RFrame::render(Renderer* renderer) {
  auto offsetShader = &renderer->offsetShader;
  C3D_BindProgram(&offsetShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&offsetShader->attrInfo);

  C3D_TexBind(0, &renderer->tilesTex);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, offsetShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, offsetShader->offset, owner->offset[0] - 8.f, owner->offset[1], 0, 0);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
  C3D_AlphaTest(true, GPU_GREATER, 0);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, 3 * 6);
}

void Renderer::RMenu::writeUI(UITextureVertex* vert, const Tex3DS_SubTexture* tex, float x, float y, float z, float xs, float ys, float xe, float ye) {
  float xr = x + tex->width * xs;
  float yr = y + tex->height * ys;

  vert[0] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}};
  vert[1] = {{x,  yr, z}, {tex->left,  tex->bottom}, {xe, ye}};
  vert[2] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}};

  vert[3] = {{xr, yr, z}, {tex->right, tex->bottom}, {xe, ye}};
  vert[4] = {{xr, y,  z}, {tex->right, tex->top},    {xe, ye}};
  vert[5] = {{x,  y,  z}, {tex->left,  tex->top},    {xe, ye}};
}

void Renderer::RMenu::createBuf(Renderer* renderer) {
  vbo = (UITextureVertex*)linearAlloc(sizeof(UITextureVertex) * maxElements * 6);

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(UITextureVertex), 3, 0x210);
}

void Renderer::RMenu::render(Renderer* renderer) {
  auto uiShader = &renderer->uiShader;
  C3D_BindProgram(&uiShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&uiShader->attrInfo);

  C3D_TexBind(0, &renderer->tilesTex);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uiShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, uiShader->distToEdge, renderer->screenBounds[0], renderer->screenBounds[1], 0, 0);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
  C3D_AlphaTest(true, GPU_GREATER, 0);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, maxElements * 6);
}

void Renderer::RMenu::RBackground::createBuf(Renderer* renderer) {
  vbo = (ColoredVertex*)linearAlloc(sizeof(ColoredVertex) * 6);

  vbo[0] = {{-160, -120}, {0, 0, 0, 0.85}};
  vbo[1] = {{-160,  120}, {0, 0, 0, 0.85}};
  vbo[2] = {{ 160,  120}, {0, 0, 0, 0.85}};

  vbo[3] = {{ 160,  120}, {0, 0, 0, 0.85}};
  vbo[4] = {{ 160, -120}, {0, 0, 0, 0.85}};
  vbo[5] = {{-160, -120}, {0, 0, 0, 0.85}};

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(ColoredVertex), 2, 0x10);
}

void Renderer::RMenu::RBackground::render(Renderer* renderer) {
  auto solidShader = &renderer->solidShader;

  C3D_BindProgram(&solidShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&solidShader->attrInfo);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, solidShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, solidShader->tranScale, 0, 0, 1, 1);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C3D_DrawArrays(GPU_TRIANGLES, 0, 6);
}

void Renderer::RFiles::writeColor(int index, u32 color) {
  textColor[index][0] = 1.f / 255.f * ((color >> 24) & 0xFF);
  textColor[index][1] = 1.f / 255.f * ((color >> 16) & 0xFF);
  textColor[index][2] = 1.f / 255.f * ((color >>  8) & 0xFF);
  textColor[index][3] = 1.f / 255.f * ((color >>  0) & 0xFF);

  if (prepared) {
    OffsetTextureVertex* vert = vbo;
    C2Di_Glyph* glyphs = textBuf->glyphs;
    for (size_t i = 0; i < textBuf->glyphCount; ++i) {
      C2Di_Glyph* cur = &glyphs[i];
      if ((int)cur->lineNo != index) {
        vert += 6;
        continue;
      }
      
      float* c = textColor[cur->lineNo];
      for (int v = 0; v < 6; ++v) {
        for (int col = 0; col < 4; ++col) {
          vert[v].color[col] = c[col];
        }
      }

      vert += 6;
    }
  }
}

//TODO: Up a Dir, New File, Next Page and Prev Page should only be
// updated on boot or when switching languages
void Renderer::RFiles::writeText(int index, const char* content, u32 color) {
  C2D_TextParse(&text[index], textBuf, content);
  prepared = false;

  writeColor(index, color);

  C2Di_Glyph* begin = &textBuf->glyphs[text[index].begin];
  C2Di_Glyph* end = &textBuf->glyphs[text[index].end];
  for (C2Di_Glyph* cur = begin; cur < end; ++cur) {
    cur->lineNo = index;
  }
}

void Renderer::RFiles::createBuf(Renderer* renderer) {
  vbo = (OffsetTextureVertex*)linearAlloc(sizeof(OffsetTextureVertex) * maxChars * 6);

  BufInfo_Init(&bufInfo);
  BufInfo_Add(&bufInfo, vbo, sizeof(OffsetTextureVertex), 4, 0x3210);

  textBuf = C2D_TextBufNew(maxChars);
}

static int compareGlyphs(const void* a, const void* b) {
  C2Di_Glyph* ga = (C2Di_Glyph*)a;
  C2Di_Glyph* gb = (C2Di_Glyph*)b;
  int ret = (int)ga->sheet - (int)gb->sheet;
  if (ret == 0)
    ret = (int)a - (int)b;
  return ret;
}

void Renderer::RFiles::prepare() {
  prepared = true;
  qsort(&textBuf->glyphs, textBuf->glyphCount, sizeof(C2Di_Glyph), compareGlyphs);

  CFNT_s* font = fontGetSystemFont();

  float lineHeight = 20.f;

  u8 h = fontGetGlyphInfo(font)->cellHeight;
  float s = 18.f / h;
  
  float z = 10.f;
  
  float xom = 1.f;
  float yom = 1.f;

  OffsetTextureVertex* vert = vbo;
  C2Di_Glyph* glyphs = textBuf->glyphs;
  for (size_t i = 0; i < textBuf->glyphCount; ++i) {
    C2Di_Glyph* cur = &glyphs[i];
    float* c = textColor[cur->lineNo];

    float x = 32.f + cur->xPos * s;
    float y = lineHeight * cur->lineNo + (lineHeight - 18.f) / 2.f;
    float xr = x + cur->width * s;
    float yr = y + h * s;


    vert[0] = {{x,  y,  z}, {cur->texcoord.left,  cur->texcoord.top},    {xom, yom}, {c[0], c[1], c[2], c[3]}};
    vert[1] = {{x,  yr, z}, {cur->texcoord.left,  cur->texcoord.bottom}, {xom, yom}, {c[0], c[1], c[2], c[3]}};
    vert[2] = {{xr, yr, z}, {cur->texcoord.right, cur->texcoord.bottom}, {xom, yom}, {c[0], c[1], c[2], c[3]}};

    vert[3] = {{xr, yr, z}, {cur->texcoord.right, cur->texcoord.bottom}, {xom, yom}, {c[0], c[1], c[2], c[3]}};
    vert[4] = {{xr, y,  z}, {cur->texcoord.right, cur->texcoord.top},    {xom, yom}, {c[0], c[1], c[2], c[3]}};
    vert[5] = {{x,  y,  z}, {cur->texcoord.left,  cur->texcoord.top},    {xom, yom}, {c[0], c[1], c[2], c[3]}};

    vert += 6;
  }
}

void Renderer::RFiles::render(Renderer* renderer) {
  if (textBuf->glyphCount == 0)
    return;

  if (!prepared)
    prepare();

  auto offsetShader = &renderer->offsetShader;
  C3D_BindProgram(&offsetShader->shader);

  C3D_SetBufInfo(&bufInfo);
  C3D_SetAttrInfo(&offsetShader->attrInfo);

  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, offsetShader->proj, renderer->screenProj);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, offsetShader->offset, offset[0], offset[1], 0, 0);

  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
  C3D_AlphaTest(true, GPU_GREATER, 0);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

  C3D_TexEnv* env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvColor(env, 0xFFFFFFFF);
  C3D_TexEnvSrc(env, C3D_RGB, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
  C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);

  C3D_TexEnvInit(C3D_GetTexEnv(1));
  C3D_TexEnvInit(C3D_GetTexEnv(2));
  C3D_TexEnvInit(C3D_GetTexEnv(3));

  C2Di_Glyph* glyphs = textBuf->glyphs;
  int from = 0;
  int count = 0;
  C3D_Tex* lastSheet = glyphs[0].sheet;
  for (size_t i = 0; i < textBuf->glyphCount; ++i) {
    if (glyphs[i].sheet == lastSheet) {
      ++count;
      continue;
    }

    C3D_TexBind(0, lastSheet);
    C3D_DrawArrays(GPU_TRIANGLES, from * 6, count * 6);

    lastSheet = glyphs[i].sheet;
    count = 1;
    from = i;
  }

  C3D_TexBind(0, lastSheet);
  C3D_DrawArrays(GPU_TRIANGLES, from * 6, count * 6);
}

void Renderer::initCharPicker() {
  if (!charPicker.initialized) {
    charPicker.background.createBuf(this);
    charPicker.background.owner = &charPicker;

    charPicker.selection.createBuf(this);
    charPicker.selection.owner = &charPicker;
  } else {
    linearFree(charPicker.vbo);
    charPicker.background.resize(this);
  }

  charPicker.createBuf(this);
  charPicker.initialized = true;

  charPicker.selection.selected[0] = charPicker.selected[0];
  charPicker.selection.selected[1] = charPicker.selected[1];
}

void Renderer::init() {
  charShader.init(this);
  solidShader.init(this);
  uiShader.init(this);
  uiSolidShader.init(this);
  offsetShader.init(this);

  tilesT3x = Tex3DS_TextureImport(&tiles_t3x, tiles_t3x_size, &tilesTex, NULL, false);
  C3D_TexSetWrap(&tilesTex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
  C3D_TexSetFilter(&tilesTex, GPU_NEAREST, GPU_NEAREST);

  ui.createBuf(this);
  colorPicker.createBuf(this);

  hint.createBuf(this);
  hint.frame.owner = &hint;
  hint.frame.createBuf(this);
  
  menu.createBuf(this);
  menu.background.createBuf(this);

  files.createBuf(this);
}

void Renderer::renderCharPicker() {
  charPicker.background.render(this);

  charPicker.render(this);

  if (
    charPicker.selected[0] != charPicker.selection.selected[0] ||
    charPicker.selected[1] != charPicker.selection.selected[1]
  ) {
    charPicker.selection.selected[0] = charPicker.selected[0];
    charPicker.selection.selected[1] = charPicker.selected[1];
    charPicker.selection.writeBuf(this);
  }
  charPicker.selection.render(this);
}

void Renderer::renderCanvas() {
  for (size_t i = 0; i < canvas.canvasLayers.size(); ++i) {
    canvas.canvasLayers[i].background.render(this);
    canvas.canvasLayers[i].render(this);
  }
}

}