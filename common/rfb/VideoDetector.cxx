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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>

#include <core/LogWriter.h>

#include <rfb/PixelBuffer.h>
#include <rfb/VideoDetector.h>

using namespace rfb;

static core::LogWriter vlog("VideoDetector");

VideoDetector::VideoDetector() {}

VideoDetector::~VideoDetector() {}

void VideoDetector::init(const PixelBuffer *pb) {
  // Reset the state after a new framebuffer
  videoRegion.clear();
  cells.clear();
  fbRect = pb->getRect();
  nrOfSuperCols = (pb->width() + GRID_SIZE_MAX - 1) / GRID_SIZE_MAX;
  nrOfSuperRows = (pb->height() + GRID_SIZE_MAX - 1) / GRID_SIZE_MAX;

  cells.resize(nrOfSuperCols * nrOfSuperRows);

  // Assign each cell to a rectangle in the framebuffer
  for (int row = 0; row < nrOfSuperRows; row++) {
    for (int col = 0; col < nrOfSuperCols; col++) {
      int x1 = col * GRID_SIZE_MAX;
      int y1 = row * GRID_SIZE_MAX;
      int x2 = x1 + GRID_SIZE_MAX;
      int y2 = y1 + GRID_SIZE_MAX;

      cells[row * nrOfSuperCols + col].rect = core::Rect(x1, y1, x2, y2);
    }
  }
  vlog.debug("nrOfCells: %zu", cells.size());
}

// Returns true if the region has changed
bool VideoDetector::addChanged(const core::Region &changed) {
  std::vector<core::Rect> rects;
  core::Region region;
  core::Region oldVideoRegion;

  // If framebuffer is empty, return false.
  if (cells.empty())
    return false;

  // Reset the changed state of all cells, as no history is yet implemented.
  for (Cell &cell : cells)
    cell.changed = false;

  changed.get_rects(&rects);
  for (const core::Rect &r : rects) {
    if (r.is_empty())
      continue;

    // Find the grid cells that intersect with the changed rectangle, and mark
    // them as changed
    int intersectColStart = std::max(r.tl.x / GRID_SIZE_MAX, 0);
    int intersectRowStart = std::max(r.tl.y / GRID_SIZE_MAX, 0);
    int intersectColEnd =
        std::min((r.br.x - 1) / GRID_SIZE_MAX, nrOfSuperCols - 1);
    int intersectRowEnd =
        std::min((r.br.y - 1) / GRID_SIZE_MAX, nrOfSuperRows - 1);

    for (int row = intersectRowStart; row <= intersectRowEnd; row++) {
      for (int col = intersectColStart; col <= intersectColEnd; col++)
        cells[row * nrOfSuperCols + col].changed = true;
    }
  }

  for (const Cell &cell : cells) {
    if (cell.changed)
      region.assign_union(cell.rect);
  }

  oldVideoRegion = videoRegion;
  videoRegion = region.intersect(fbRect);

  // Return if the video region has changed
  return videoRegion != oldVideoRegion;
}

const core::Region &VideoDetector::getVideoRegion() const {
  return videoRegion;
}
