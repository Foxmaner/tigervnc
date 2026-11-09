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
  int nrOfCols = (pb->width() + GRID_WIDTH - 1) / GRID_WIDTH;
  int nrOfRows = (pb->height() + GRID_HEIGHT - 1) / GRID_HEIGHT;

  cells.assign(nrOfCols * nrOfRows, Cell());
  vlog.debug("VIDEO nrOfCells: %zu", cells.size());
}

// Returns true if the region has changed
bool VideoDetector::addChanged(const core::Region &changed) {

  core::Region oldVideoRegion = videoRegion;
  videoRegion = changed;

  if (oldVideoRegion != videoRegion) {
    return true;
  } else {
    return false;
  }
}

const core::Region &VideoDetector::getVideoRegion() const {
  return videoRegion;
}
