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
#include <core/time.h>

#include <rfb/PixelBuffer.h>
#include <rfb/VideoDetector.h>

using namespace rfb;

static const int neighborOffsets[8][2] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1},
};

static core::LogWriter vlog("VideoDetector");

VideoDetector::VideoDetector() {}

VideoDetector::~VideoDetector() {}

void VideoDetector::init(const PixelBuffer *pb) {
  // Reset the state after a new framebuffer
  videoRegion.clear();
  superCells.clear();
  fbRect = pb->getRect();
  gettimeofday(&startTime, nullptr);
  nrOfSuperCols = (pb->width() + GRID_SIZE_MAX - 1) / GRID_SIZE_MAX;
  nrOfSuperRows = (pb->height() + GRID_SIZE_MAX - 1) / GRID_SIZE_MAX;

  superCells.resize(nrOfSuperCols * nrOfSuperRows);

  // Assign each cell to a rectangle in the framebuffer
  for (int row = 0; row < nrOfSuperRows; row++) {
    for (int col = 0; col < nrOfSuperCols; col++) {
      int x1 = col * GRID_SIZE_MAX;
      int y1 = row * GRID_SIZE_MAX;
      int x2 = x1 + GRID_SIZE_MAX;
      int y2 = y1 + GRID_SIZE_MAX;

      superCells[row * nrOfSuperCols + col].rect = core::Rect(x1, y1, x2, y2);

      // Asign neighbor supercells to each supercell
      for (int i = 0; i < 8; i++) {
        int neighborCol = col + neighborOffsets[i][0];
        int neighborRow = row + neighborOffsets[i][1];
        if (neighborCol < 0 || neighborCol >= nrOfSuperCols ||
            neighborRow < 0 || neighborRow >= nrOfSuperRows) {
          superCells[row * nrOfSuperCols + col].neighborSuperCells[i] =
              -1; // No neighbor
        } else {
          superCells[row * nrOfSuperCols + col].neighborSuperCells[i] =
              neighborRow * nrOfSuperCols + neighborCol;
        }
      }
    }
  }
  vlog.debug("nrOfCells: %zu", superCells.size());
}

// Returns true if the region has changed
bool VideoDetector::addChanged(const core::Region &changed) {
  std::vector<core::Rect> rects;
  core::Region region;
  core::Region oldVideoRegion;

  // If framebuffer is empty, return false.
  if (superCells.empty())
    return false;

  unsigned now = currentBucket();
  // Age each cell each pass
  for (SuperCell &cell : superCells) {
    ageCell(cell, now);
  }

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

    // Mark cells intersecting with damage as video by setting the current
    // bucket to 1
    for (int row = intersectRowStart; row <= intersectRowEnd; row++) {
      for (int col = intersectColStart; col <= intersectColEnd; col++) {
        SuperCell &cell = superCells[row * nrOfSuperCols + col];
        markChangedCell(cell, r);
      }
    }
  }

  // Determine which cells have had enough damage to be concidered video
  for (SuperCell &cell : superCells) {
    checkActiveCell(cell);
    /*
    if (cell.active) {
      region.assign_union(cell.rect);
    }
      */
  }

  //For debugging, only include edge cells in the video region
  for (const SuperCell &cell : superCells) {
    if (cell.active && isEdgeCell(cell)) {
      region.assign_union(cell.rect);
    }
  }

  // Stores all the rects including the split ones, for the video debugger overlay.
  cellRects.clear();
  for (const SuperCell &cell : superCells) {
    collectCellRects(cell);
  }

  oldVideoRegion = videoRegion;
  videoRegion = region.intersect(fbRect);

  // Return if the video region has changed
  return videoRegion != oldVideoRegion;
}

//Recursively collects all rects, both supercell and split cell.
void VideoDetector::collectCellRects(const Cell &cell) {
  if (cell.children != nullptr) {
    for (int i = 0; i < 4; i++)
      collectCellRects(cell.children[i]);
    return;
  }

  // Cells on the right and bottom can stick out of the framebuffer
  core::Rect r = cell.rect.intersect(fbRect);
  if (!r.is_empty())
    cellRects.push_back(r);
}

bool VideoDetector::isEdgeCell(const SuperCell &cell) const {
  for (int i = 0; i < 8; i++) {
    int neighborIndex = cell.neighborSuperCells[i];
    if (neighborIndex == -1 || !superCells[neighborIndex].active) {
      return true;
    }
  }
  return false;
}

//TODO: Implement the algoritm
core::Rect VideoDetector::findLargestVideoArea() const {
  core::Rect largest;
  int activeArea = 0; // Total number of active cells
  int largestArea = 0; // Largest rectangle of connected active cells
  //Avoid unused variable warning
  (void)largestArea;
  (void)activeArea;

  //set activeArea to number of active cells
  for (const SuperCell &cell : superCells) {
    if (cell.active) {
      activeArea++;
    }else{
      continue;
    }
  }
  return largest;
}

unsigned VideoDetector::currentBucket() const {
  return core::msSince(&startTime) / BUCKET_MS;
}

void VideoDetector::ageCell(Cell &cell, unsigned now) {
  // Age cell by shifting historicalbuckets to the left by the number of passed
  // buckets since last call.
  unsigned passed = now - cell.lastBucket;

  if (passed >= HISTORY_BUCKETS)
    cell.historicalBuckets = 0;
  else
    cell.historicalBuckets = cell.historicalBuckets << passed;

  cell.lastBucket = now;

  if(cell.children != nullptr){
    for(int i = 0; i < 4; i++){
      ageCell(cell.children[i], now);
    }
  }
}

void VideoDetector::markChangedCell(Cell &cell, const core::Rect &r) {
  // Mark cell as changed by setting the current bucket to 1
  if(cell.rect.intersect(r).is_empty()){
    return;
  }
  cell.historicalBuckets = cell.historicalBuckets | 1;
  if(cell.children != nullptr){
    for(int i = 0; i < 4; i++){
      markChangedCell(cell.children[i], r);
    }
  }
}

void VideoDetector::checkActiveCell(Cell &cell) {
  unsigned changedBuckets = countChangedBuckets(cell);
  if (changedBuckets >= MIN_THRESHOLD_BUCKET_RATIO * HISTORY_BUCKETS) {
    cell.active = true;
  } else {
    cell.active = false;
  }
  if(cell.children != nullptr){
    for(int i = 0; i < 4; i++){
      checkActiveCell(cell.children[i]);
    }
  }
}

unsigned VideoDetector::countChangedBuckets(const Cell &cell) const {
  // Counts the number of 1:s in the historicalBuckets bitfield
  unsigned count = 0;

  for (unsigned i = 0; i < HISTORY_BUCKETS; i++) {
    if ((cell.historicalBuckets >> i) & 1)
      count++;
  }

  return count;
}

const core::Region &VideoDetector::getVideoRegion() const {
  return videoRegion;
}

const std::vector<core::Rect> &VideoDetector::getCellRects() const {
  return cellRects;
}
