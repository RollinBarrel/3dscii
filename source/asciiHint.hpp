#pragma once

#include <functional>

#include "asciiRenderer.hpp"

namespace ASCII {

class Hint {
public:
  ASCII::Renderer::RHint* renderer;
  
  bool show = false;
  float y = 100.f;
  float maxWidth = 200.f;
  float scrollSpeed = 1.f;
  float slideDuration = 0.2f;
  float slide = 1.f;
  double time = 0.;

  std::function<void(float width)> writeFrame;
  
  Hint() {};
  inline Hint(ASCII::Renderer::RHint* renderer) {this->renderer = renderer;};
  void setText(const char* text);
  void update(double dt);
};

}