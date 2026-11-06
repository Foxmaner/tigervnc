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

#include <rfb/VideoDetector.h>
#include <rfb/PixelBuffer.h>

using namespace rfb;

static core::LogWriter vlog("VideoDetector");

VideoDetector::VideoDetector()
{
  // FIXME: Placeholder zones until the detection algorithm is in place
  videoRegion.assign_union(core::Rect(100, 100, 740, 460));
  videoRegion.assign_union(core::Rect(900, 200, 1220, 380));
  videoRegion.assign_union(core::Rect(300, 600, 780, 870));
}

VideoDetector::~VideoDetector()
{
}

void VideoDetector::init(const PixelBuffer* pb)
{
  // TODO: Reset state for the new framebuffer
  videoRegion.clear();
  cells.clear();
  int nrOfCols = (pb->width() + GRID_WIDTH - 1) / GRID_WIDTH;
  int nrOfRows = (pb->height() + GRID_HEIGHT - 1) / GRID_HEIGHT;

  cells.assign(nrOfCols*nrOfRows, Cell());
  vlog.error("VIDEO nrOfCells: %zu",cells.size());
}

void VideoDetector::addChanged(const core::Region& changed)
{
  // TODO: Update videoRegion based on the changed areas
  //videoRegion.assign_union(changed);
  (void)changed;
}

const core::Region& VideoDetector::getVideoRegion() const
{
  return videoRegion;
}
