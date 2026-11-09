/* Copyright 2026 Eskil Brännerud for Cendio AB
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifndef __RFB_VIDEODETECTOR_H__
#define __RFB_VIDEODETECTOR_H__

#include "core/Rect.h"
#include <core/Region.h>
#include <vector>

namespace rfb {

class PixelBuffer;

class VideoDetector {
public:
  // Size in pixels of each grid cell
  static const int GRID_SIZE_MAX = 128;

  VideoDetector();
  ~VideoDetector();

  // Called when the framebuffer is set or changes size
  void init(const PixelBuffer *pb);

  // Called with the areas of the framebuffer that have actually
  // changed, after the comparison has removed unchanged pixels
  bool addChanged(const core::Region &changed);

  const core::Region &getVideoRegion() const;

private:
  core::Region videoRegion;

  struct Cell {
    core::Rect rect;
    bool changed = false;
  };

  std::vector<Cell> cells;

  int nrOfSuperCols = 0;
  int nrOfSuperRows = 0;

  core::Rect fbRect;
};

} // namespace rfb

#endif
