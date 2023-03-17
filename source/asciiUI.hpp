#pragma once

#include <math.h>
#include <3ds.h>
#include <functional>
#include <list>
#include <tex3ds.h>
#include "asciiRenderer.hpp"

namespace ASCII {

class UI {
public:
  typedef struct {
    float l;
    float u;
    float r;
    float d;
  } AABB;

  class ITouchable {
  public:
    virtual void sync() = 0;
    virtual bool check(float pos[2]) = 0;
    virtual void press(float pos[2]) = 0;
    virtual void drag(float pos[2]) = 0;
    virtual void release(float pos[2]) = 0;
  };

  class EmptyTouchable : public ITouchable {
  public:
    std::function<void()> onRelease;

    void sync() {};
    bool check(float pos[2]) {return true;};
    void press(float pos[2]) {};
    void drag(float pos[2]) {};
    void release(float pos[2]) {onRelease();};
  };

  class Grid : public ITouchable {
  public:
    bool active = true;
    float position[2];
    float scale[2];
    float cellDimension[2];
    float cellCount[2];

    std::function<void(float dx, float dy)> onPress;
    std::function<void(float dx, float dy)> onDrag;
    std::function<void(float dx, float dy)> onRelease;
    std::function<void(Grid* grid)> onSync;

    float getDX(float x) {
      float gridPos = (x - std::floor(position[0])) / scale[0] / cellDimension[0];
      if (gridPos < 0)
        gridPos = 0;
      if (gridPos >= cellCount[0])
        gridPos = cellCount[0] - 1;

      return gridPos;
    }
    float getDY(float y) {
      float gridPos = (y - std::floor(position[1])) / scale[1] / cellDimension[1];
      if (gridPos < 0)
        gridPos = 0;
      if (gridPos >= cellCount[1])
        gridPos = cellCount[1] - 1;
      
      return gridPos;
    }

    void sync() {onSync(this);}
    //TODO: my brain doesn't work rn, there's a simpler way
    bool check(float pos[2]) {
      float gridPos[] = {
        (pos[0] - std::floor(position[0])) / scale[0] / cellDimension[0],
        (pos[1] - std::floor(position[1])) / scale[1] / cellDimension[1]
      };

      if (
        gridPos[0] >= 0 && gridPos[0] < cellCount[0] &&
        gridPos[1] >= 0 && gridPos[1] < cellCount[1]
      ) {
        return true;
      }
      return false;
    }
    void press(float pos[2]) {onPress(getDX(pos[0]), getDY(pos[1]));}
    void drag(float pos[2]) {onDrag(getDX(pos[0]), getDY(pos[1]));}
    void release(float pos[2]) {onRelease(getDX(pos[0]), getDY(pos[1]));}
  };

  class Button : public ITouchable {
  public:
    bool active = true;
    AABB aabb;

    std::function<void(float dx, float dy)> onDrag;
    std::function<void()> onDown;
    std::function<void()> onUp;
    std::function<void()> onPress;
    std::function<void(bool pressed)> onSwap;
    std::function<void(Button* btn)> onSync;

    static void btnSwap(UI::Button* btn, Renderer::UITextureVertex* vert, const Tex3DS_SubTexture* tex);

    void init(float dx, float dy, Renderer::UITextureVertex* vert, Tex3DS_Texture texture, int subIdx, int subIdxPressed, bool* toggled = NULL, bool inverted = false);
    void sync() {onSync(this);}
    bool check(float pos[2]) {
      return pos[0] >= aabb.l && pos[0] < aabb.r && pos[1] >= aabb.u && pos[1] < aabb.d;
    }
    void press(float pos[2]) {
      onSwap(true);
      onDown();
    }
    void drag(float pos[2]) {
      if (check(pos)) {
        onSwap(true);
      } else {
        onSwap(false);
      }
      onDrag(pos[0], pos[1]);
    }
    void release(float pos[2]) {
      if (check(pos)) {
        onPress();
      }
      onSwap(false);
      onUp();
    }
  };

  typedef struct {
    AABB aabb;
    std::list<Button> buttons;
  } SidePanel;

  typedef enum {
    TARGET_NONE,
    TARGET_PICKER,
    TARGET_CANVAS,
    TARGET_MENU,
    TARGET_FILES
  } TouchTarget;

  EmptyTouchable emptyTouchable;
  touchPosition prevTouch;
  touchPosition touch;
  TouchTarget lastTouchTarget = TARGET_NONE;
  ITouchable* current;

  Grid charPicker;
  Grid colorPicker;
  Grid canvas;

  SidePanel leftPanel;
  SidePanel rightPanel;
  std::list<Button> menuBtns;

  void init();
  void handlePickerScreen();
  void handleCanvasScreen();
  void handleMenu();
  void handleFiles(std::vector<Button> buttons);
};

}
