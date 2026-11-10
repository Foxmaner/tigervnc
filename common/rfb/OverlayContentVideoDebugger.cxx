// -=- OverlayContentVideoDebugger.cxx
//
// Renders the areas detected as playing video into a pixel buffer for use as
// debug overlay content.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>
#include <cstdio>
#include <vector>

#include <core/LogWriter.h>
#include <core/string.h>

#include <rfb/OverlayContentVideoDebugger.h>
#include <rfb/VideoDetector.h>

#include <pixman.h>

using namespace rfb;

static core::LogWriter vlog("OverlayContentVideoDebugger");

// Width in pixels of the border drawn around each video zone
static const int borderWidth = 3;

// Milliseconds between each measurement against the ground truth
static const int measurementInterval = 100;

static unsigned measurementCount = 0;

OverlayContentVideoDebugger::OverlayContentVideoDebugger(
    const std::string& groundTruth, const core::Region& detectedZones,
    const std::vector<core::Rect>& cellRects, int width, int height)
    : _buffer(nullptr), _groundTruth(parseZones(groundTruth)),
      _detectedZones(detectedZones), _screen(0, 0, width, height),
      _measurementTimer(this) {
  _width = _height = 0;
  _buffer = generateVideoDebugBuffer(_groundTruth, detectedZones, cellRects,
                                     width, height, &_width, &_height);
  _measurementTimer.start(measurementInterval);
}

OverlayContentVideoDebugger::~OverlayContentVideoDebugger() { delete[] _buffer; }

static unsigned long long regionArea(const core::Region& region) {
  std::vector<core::Rect> rects;
  unsigned long long area = 0;

  region.get_rects(&rects);
  for (const core::Rect& r : rects)
    area += r.area();

  return area;
}

void OverlayContentVideoDebugger::handleTimeout(core::Timer* /*t*/) {
  core::Region detected = _detectedZones.intersect(_screen);
  core::Region truth = _groundTruth.intersect(_screen);

  unsigned long long tp = regionArea(detected.intersect(truth));
  unsigned long long fp = regionArea(detected) - tp;
  unsigned long long fn = regionArea(truth) - tp;
  unsigned long long tn = _screen.area() - tp - fp - fn;

  vlog.info("Measurement: sample=%u tp=%llu tn=%llu fp=%llu fn=%llu",
            ++measurementCount, tp, tn, fp, fn);

  _measurementTimer.repeat();
}

//Parse the zones from overlayInput string
core::Region OverlayContentVideoDebugger::parseZones(const std::string& zones) {
  core::Region region;

  for (const std::string& zone : core::split(zones.c_str(), ',')) {
    int x1, y1, x2, y2, len;

    if (zone.empty())
      continue;

    // %n makes sure there is nothing left after the rectangle
    if ((sscanf(zone.c_str(), "%dx%d:%dx%d%n", &x1, &y1, &x2, &y2, &len) !=
         4) ||
        (len != static_cast<int>(zone.size())) || (x2 <= x1) || (y2 <= y1)) {
      vlog.error("Invalid video zone specified: %s", zone.c_str());
      continue;
    }

    region.assign_union(core::Region(core::Rect(x1, y1, x2, y2)));
  }

  return region;
}

// Draws 1px lines along the boundaries of the VideoDetector cells,
// including the split ones. Only the top and left side of each cell is
// drawn.
static void drawGrid(pixman_image_t* destImage,
                     const std::vector<core::Rect>& cellRects,
                     const pixman_color_t& color) {
  std::vector<pixman_rectangle16_t> lines;

  for (const core::Rect& r : cellRects) {
    // Nothing to separate along the top and left edge of the screen
    if (r.tl.y > 0)
      lines.push_back({static_cast<int16_t>(r.tl.x),
                       static_cast<int16_t>(r.tl.y),
                       static_cast<uint16_t>(r.width()), 1});
    if (r.tl.x > 0)
      lines.push_back({static_cast<int16_t>(r.tl.x),
                       static_cast<int16_t>(r.tl.y), 1,
                       static_cast<uint16_t>(r.height())});
  }

  if (lines.empty())
    return;

  pixman_image_fill_rectangles(PIXMAN_OP_SRC, destImage, &color,
                               lines.size(), lines.data());
}

// Draws every rectangle of the zones with a solid border. With pixman.
static void drawZones(pixman_image_t* destImage, const core::Region& zones,
                      const pixman_color_t* fillColor,
                      const pixman_color_t& borderColor) {
  std::vector<core::Rect> rects;
  zones.get_rects(&rects);

  //Draw each rect
  for (const core::Rect& r : rects) {
    if (fillColor) {
      pixman_rectangle16_t fill = {
          static_cast<int16_t>(r.tl.x), static_cast<int16_t>(r.tl.y),
          static_cast<uint16_t>(r.width()), static_cast<uint16_t>(r.height())};
      pixman_image_fill_rectangles(PIXMAN_OP_SRC, destImage, fillColor, 1,
                                   &fill);
    }

    // Keep the border inside the zone, even for very small zones
    int bw = std::min({borderWidth, r.width() / 2, r.height() / 2});
    if (bw <= 0)
      continue;

    pixman_rectangle16_t border[4] = {
        // Top
        {static_cast<int16_t>(r.tl.x), static_cast<int16_t>(r.tl.y),
         static_cast<uint16_t>(r.width()), static_cast<uint16_t>(bw)},
        // Bottom
        {static_cast<int16_t>(r.tl.x), static_cast<int16_t>(r.br.y - bw),
         static_cast<uint16_t>(r.width()), static_cast<uint16_t>(bw)},
        // Left
        {static_cast<int16_t>(r.tl.x), static_cast<int16_t>(r.tl.y),
         static_cast<uint16_t>(bw), static_cast<uint16_t>(r.height())},
        // Right
        {static_cast<int16_t>(r.br.x - bw), static_cast<int16_t>(r.tl.y),
         static_cast<uint16_t>(bw), static_cast<uint16_t>(r.height())},
    };
    pixman_image_fill_rectangles(PIXMAN_OP_SRC, destImage, &borderColor, 4,
                                 border);
  }
}

// Draws the detected zones as a translucent red fill with a solid red border,
// and the ground truth zones as a green border on top, into a new buffer
// covering the entire framebuffer.
uint8_t* OverlayContentVideoDebugger::generateVideoDebugBuffer(
    const core::Region& groundTruth, const core::Region& detectedZones,
    const std::vector<core::Rect>& cellRects, int width, int height,
    int* outWidth, int* outHeight) {
  *outWidth = *outHeight = 0;

  if ((width <= 0) || (height <= 0))
    return nullptr;

  int stride = width * 4;
  uint8_t* buf = new uint8_t[stride * height]();

  pixman_image_t* destImage = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height,
                               reinterpret_cast<uint32_t*>(buf), stride);
  if (!destImage) {
    vlog.error("Failed to create Pixman image surface wrapper for video zones.");
    delete[] buf;
    return nullptr;
  }

  // Pixman colours are premultiplied, so the colour components of the
  // translucent fill must be scaled by its alpha (25% red)
  pixman_color_t detectedFillColor = {0x4000, 0x0000, 0x0000, 0x4000};
  pixman_color_t detectedBorderColor = {0xffff, 0x0000, 0x0000, 0xffff};
  pixman_color_t groundTruthBorderColor = {0x0000, 0xffff, 0x0000, 0xffff};
  pixman_color_t gridColor = {0xffff, 0xffff, 0xffff, 0xffff};

  core::Rect fbRect(0, 0, width, height);
  core::Region detected = detectedZones.intersect(fbRect);
  core::Region truth = groundTruth.intersect(fbRect);

  vlog.debug("Rendering %d detected and %d ground truth video zone rect(s)",
             detected.numRects(), truth.numRects());

  // The grid is drawn first so the zones are drawn on top of it
  drawGrid(destImage, cellRects, gridColor);

  // Ground truth is drawn last so it stays visible where the zones overlap
  drawZones(destImage, detected, &detectedFillColor, detectedBorderColor);
  drawZones(destImage, truth, nullptr, groundTruthBorderColor);

  pixman_image_unref(destImage);

  *outWidth = width;
  *outHeight = height;
  return buf;
}
