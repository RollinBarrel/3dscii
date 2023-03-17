#include "asciiHint.hpp"

namespace ASCII {

void Hint::setText(const char* text) {
  renderer->writeText(text);
  writeFrame(renderer->width);
  renderer->offset[0] = 182.f - renderer->width;
  show = true;
  slide = 0.f;
  time = 0.;
}

//TODO: scrolling, adjust position with length
void Hint::update(double dt) {
  if (!show) {
    time -= dt;
  }
  
  if (show || time > 0.) {
    slide = std::min(slide + (float)dt / slideDuration, 1.f);
  } else {
    slide = std::max(slide - (float)dt / slideDuration, 0.f);
  }


  float t = 1 - (std::max(slide - (2.f / 3.f), 0.f) * 3.f);
  t = 1 - (t * t * t);
  renderer->offset[1] = 124.f + (y - 124.f) * t;

  if (renderer->width > maxWidth) {
    
  }
}

}