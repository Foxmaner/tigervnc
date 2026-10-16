// -=- OverlayContentVideoDebugger.h
//
// An overlay content implementation that renders the detected video zones,
// and a user-specified ground truth into the pixel buffer.

#ifndef __RFB_OVERLAY_CONTENT_VIDEO_DEBUGGER_H__
#define __RFB_OVERLAY_CONTENT_VIDEO_DEBUGGER_H__

#include <string>

#include <core/Region.h>
#include <core/Timer.h>

#include <rfb/OverlayContent.h>

namespace rfb {

class OverlayContentVideoDebugger : public OverlayContent,
                                    public core::Timer::Callback {
public:
  OverlayContentVideoDebugger(const std::string& groundTruth,
                              const core::Region& detectedZones, int width,
                              int height);
  virtual ~OverlayContentVideoDebugger();

  virtual uint8_t* getContentPixelBuffer() override { return _buffer; }

private:
  // Parses a comma separated list of "X1xY1:X2xY2" rectangles, where
  // (X1,Y1) is the top left corner and (X2,Y2) the bottom right corner,
  // e.g. "10x10:20x20,30x30:40x40". Invalid entries are disregarded.
  static core::Region parseZones(const std::string& zones);

  // Draws the detected and ground truth video zones into a freshly allocated,
  // framebuffer sized ARGB32 buffer.
  static uint8_t* generateVideoDebugBuffer(const core::Region& groundTruth,
                                           const core::Region& detectedZones,
                                           int width, int height,
                                           int* outWidth, int* outHeight);

  // Logs the pixel counts of true/false positives/negatives of the
  // detected zones against the ground truth, ten times per second
  void handleTimeout(core::Timer* t) override;

  uint8_t* _buffer;
  core::Region _groundTruth;
  const core::Region& _detectedZones;
  core::Rect _screen;
  core::Timer _measurementTimer;
};

} // namespace rfb

#endif // __RFB_OVERLAY_CONTENT_VIDEO_DEBUGGER_H__
