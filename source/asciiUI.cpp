#include "asciiUI.hpp"

namespace ASCII {

void UI::Button::btnSwap(UI::Button* btn, Renderer::UITextureVertex* vert, const Tex3DS_SubTexture* tex) {
  float ul = tex->left;
  float vl = tex->top;
  float ur = tex->right;
  float vr = tex->bottom;

  vert[0].texcoord[0] = ul;
  vert[0].texcoord[1] = vl;
  vert[1].texcoord[0] = ul;
  vert[1].texcoord[1] = vr;
  vert[2].texcoord[0] = ur;
  vert[2].texcoord[1] = vr;

  vert[3].texcoord[0] = ur;
  vert[3].texcoord[1] = vr;
  vert[4].texcoord[0] = ur;
  vert[4].texcoord[1] = vl;
  vert[5].texcoord[0] = ul;
  vert[5].texcoord[1] = vl;
}

void UI::Button::init(float dx, float dy, Renderer::UITextureVertex* vert, Tex3DS_Texture texture, int subIdx, int subIdxPressed, bool* toggled, bool inverted) {
  auto tex = Tex3DS_GetSubTexture(texture, subIdx);
  auto pressedTex = Tex3DS_GetSubTexture(texture, subIdxPressed);

  aabb = {
    l: dx,
    u: dy,
    r: dx + tex->width,
    d: dy + tex->height
  };

  onSwap = [this, vert, tex, pressedTex, toggled, inverted](bool pressed) {
    if (toggled != NULL && *toggled != inverted) {
      pressed = !pressed;
    }
    auto t = pressed ? pressedTex : tex;
    UI::Button::btnSwap(this, vert, t);
  };

  onSync = [](ASCII::UI::Button*){};
  onDrag = [](float, float){};
  onDown = [](){};
  onUp = [](){};
  onPress = [](){};
}

void UI::init() {
  emptyTouchable.onRelease = [this]() {
    this->current = NULL;
  };
  leftPanel.aabb = {
    l: -160,
    u: -120,
    r: -160 + 24,
    d: 120
  };
  rightPanel.aabb = {
    l: 160 - 24,
    u: -120,
    r: 160,
    d: 120
  };
}

void UI::handlePickerScreen() {
  if ((touch.px == 0 && touch.py == 0) || lastTouchTarget != TARGET_PICKER) {
    if (current != NULL) {
      float pos[] = {
        (float)(prevTouch.px - 160),
        (float)(prevTouch.py - 120)
      };
      current->sync();
      current->release(pos);
      
      current = lastTouchTarget != TARGET_PICKER ? &emptyTouchable : NULL;
    }

    lastTouchTarget = TARGET_PICKER;

    return;
  }

  float pos[] = {
    (float)(touch.px - 160),
    (float)(touch.py - 120)
  };

  if (current != NULL) {
    current->sync();
    current->drag(pos);
    return;
  }

  // gonna assume that the left panel is full height and stays left
  // and that the right panel is full height and stays right
  if (pos[0] < leftPanel.aabb.r) {
    for (auto& button : leftPanel.buttons) {
      button.sync();
      if (button.check(pos)) {
        current = &button;
        button.press(pos);
        return;
      }
    }
    return;
  } else if (pos[0] > rightPanel.aabb.l) {
    for (auto& button : rightPanel.buttons) {
      button.sync();
      if (button.check(pos)) {
        current = &button;
        button.press(pos);
        return;
      }
    }

    colorPicker.sync();
    if (colorPicker.check(pos)) {
      current = &colorPicker;
      colorPicker.press(pos);
    }
    return;
  }

  charPicker.sync();
  if (charPicker.check(pos)) {
    current = &charPicker;
    charPicker.press(pos);
  }
}

void UI::handleCanvasScreen() {
  if ((touch.px == 0 && touch.py == 0) || lastTouchTarget != TARGET_CANVAS) {
    if (current != NULL) {
      float pos[] = {
        (float)(prevTouch.px - 160),
        (float)(prevTouch.py - 120)
      };
      current->sync();
      current->release(pos);
      current = lastTouchTarget != TARGET_CANVAS ? &emptyTouchable : NULL;
    }

    lastTouchTarget = TARGET_CANVAS;

    return;
  }

  float pos[] = {
    (float)(touch.px - 160),
    (float)(touch.py - 120)
  };

  if (current != NULL) {
    current->sync();
    current->drag(pos);
    return;
  }

  canvas.sync();
  if (canvas.check(pos)) {
    current = &canvas;
    canvas.press(pos);
  }
}

void UI::handleMenu() {
  if ((touch.px == 0 && touch.py == 0) || lastTouchTarget != TARGET_MENU) {
    if (current != NULL) {
      float pos[] = {
        (float)(prevTouch.px - 160),
        (float)(prevTouch.py - 120)
      };
      current->sync();
      current->release(pos);
      current = lastTouchTarget != TARGET_MENU ? &emptyTouchable : NULL;
    }

    lastTouchTarget = TARGET_MENU;

    return;
  }

  float pos[] = {
    (float)(touch.px - 160),
    (float)(touch.py - 120)
  };

  if (current != NULL) {
    current->sync();
    current->drag(pos);
    return;
  }

  for (auto& button : menuBtns) {
    button.sync();
    if (button.check(pos)) {
      current = &button;
      button.press(pos);
      return;
    }
  }
}

void UI::handleFiles(std::vector<Button> buttons) {
  if ((touch.px == 0 && touch.py == 0) || lastTouchTarget != TARGET_FILES) {
    if (current != NULL) {
      float pos[] = {
        (float)(prevTouch.px - 160),
        (float)(prevTouch.py - 120)
      };
      current->sync();
      current->release(pos);
      current = lastTouchTarget != TARGET_FILES ? &emptyTouchable : NULL;
    }

    lastTouchTarget = TARGET_FILES;

    return;
  }

  float pos[] = {
    (float)(touch.px - 160),
    (float)(touch.py - 120)
  };

  if (current != NULL) {
    current->sync();
    current->drag(pos);
    return;
  }

  for (auto& button : buttons) {
    button.sync();
    if (button.check(pos)) {
      current = &button;
      button.press(pos);
      return;
    }
  }
}

}