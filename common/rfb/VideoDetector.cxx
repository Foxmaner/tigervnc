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

#include "core/Rect.h"
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

  vlog.info("Settings: grid_size_max=%d grid_size_min=%d bucket_ms=%u "
            "history_buckets=%u history_ms=%u "
            "min_threshold_bucket_ratio=%.2f min_keep_bucket_ratio=%.2f "
            "min_video_region_ratio=%.2f",
            GRID_SIZE_MAX, GRID_SIZE_MIN, BUCKET_MS, HISTORY_BUCKETS,
            HISTORY_MS, MIN_THRESHOLD_BUCKET_RATIO, MIN_KEEP_BUCKET_RATIO,
            MIN_VIDEO_REGION_RATIO);
}

// Returns true if the region has changed
bool VideoDetector::addChanged(const core::Region &changed) {
  std::vector<core::Rect> rects;
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
    if (isEdgeCell(cell)) {
      refineCell(cell);
    } else {
      cell.children.reset(); // Merge back to parent if not edge cell
    }
  }

  // Stores all the rects including the split ones, for the video debugger
  // overlay.
  cellRects.clear();
  for (const SuperCell &cell : superCells) {
    collectCellRects(cell);
  }

  oldVideoRegion = videoRegion;
  videoRegion = findLargestVideoArea();

  // Return if the video region has changed
  return videoRegion != oldVideoRegion;
}

// Recursively collects all rects, both supercell and split cell.
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
  int connectiveActiveNeighbors = 0;
  for (int i = 0; i < 8; i++) {
    int neighborIndex = cell.neighborSuperCells[i];
    if (neighborIndex != -1 && superCells[neighborIndex].active) {
      connectiveActiveNeighbors++;
    }
  }
  if (connectiveActiveNeighbors == 3 || connectiveActiveNeighbors == 5) {
    return true;
  }
  return false;
}

// Returns true if two rects overlap, share a border, or corner
static bool touches(const core::Rect &a, const core::Rect &b) {
  return a.tl.x <= b.br.x && b.tl.x <= a.br.x && a.tl.y <= b.br.y &&
         b.tl.y <= a.br.y;
}

// Adds rect i and every rect connected to it to the group
static void fillGroup(const std::vector<core::Rect> &rects,
                      std::vector<bool> &visited, size_t i, core::Rect &bounds,
                      int &area) {
  // Basecase, if the rect is already visited
  if (visited[i])
    return;

  // Visit current rect if not already
  visited[i] = true;
  area += rects[i].area();
  bounds = bounds.union_boundary(rects[i]);

  // Flood fill all neighbors, recursivly
  for (size_t j = 0; j < rects.size(); j++) {
    if (touches(rects[i], rects[j]))
      fillGroup(rects, visited, j, bounds, area);
  }
}

core::Rect VideoDetector::findLargestVideoArea() const {
  // Get all active cells over the whole framebuffer
  core::Region region;
  for (const SuperCell &cell : superCells) {
    if (cell.active)
      collectActiveCells(cell, region);
  }

  // Create a vector of rectangles in the active area
  std::vector<core::Rect> rects;
  region.get_rects(&rects);

  std::vector<bool> visited(rects.size(), false);
  core::Rect largest;
  int largestArea = 0;

  // Flood fill from every rect that is not yet part of a group
  for (size_t i = 0; i < rects.size(); i++) {
    if (visited[i])
      continue;

    core::Rect bounds = rects[i];
    int area = 0;
    fillGroup(rects, visited, i, bounds, area);

    if (area > largestArea) {
      largestArea = area;
      largest = bounds;
    }
  }

  if(largestArea < MIN_VIDEO_REGION_RATIO * fbRect.area()){
    return core::Rect();
  }

  return largest.intersect(fbRect);
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

  if (cell.children != nullptr) {
    for (int i = 0; i < 4; i++) {
      ageCell(cell.children[i], now);
    }
  }
}

void VideoDetector::markChangedCell(Cell &cell, const core::Rect &r) {
  // Mark cell as changed by setting the current bucket to 1
  if (cell.rect.intersect(r).is_empty()) {
    return;
  }
  cell.historicalBuckets = cell.historicalBuckets | 1;
  if (cell.children != nullptr) {
    for (int i = 0; i < 4; i++) {
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
  if (cell.children != nullptr) {
    for (int i = 0; i < 4; i++) {
      checkActiveCell(cell.children[i]);
    }
  }
}

void VideoDetector::splitCell(Cell &cell) {
  if (cell.children != nullptr) {
    return; // Already split
  }
  cell.children.reset(new Cell[4]);
  int midX = (cell.rect.tl.x + cell.rect.br.x) / 2;
  int midY = (cell.rect.tl.y + cell.rect.br.y) / 2;
  cell.children[0].rect =
      core::Rect(cell.rect.tl.x, cell.rect.tl.y, midX, midY);
  cell.children[1].rect =
      core::Rect(midX, cell.rect.tl.y, cell.rect.br.x, midY);
  cell.children[2].rect =
      core::Rect(cell.rect.tl.x, midY, midX, cell.rect.br.y);
  cell.children[3].rect =
      core::Rect(midX, midY, cell.rect.br.x, cell.rect.br.y);

  for (int i = 0; i < 4; i++) {
    cell.children[i].historicalBuckets = cell.historicalBuckets;
    cell.children[i].lastBucket = cell.lastBucket;
    // cell.children[i].active = cell.active;
  }
}

void VideoDetector::collectActiveCells(const Cell &cell,
                                       core::Region &region) const {
  if (cell.active && cell.children == nullptr) {
    region.assign_union(cell.rect);
  }
  if (cell.children != nullptr) {
    for (int i = 0; i < 4; i++) {
      collectActiveCells(cell.children[i], region);
    }
  }
}

void VideoDetector::refineCell(Cell &cell) {

  if (cell.children != nullptr) {
    for (int i = 0; i < 4; i++)
      refineCell(cell.children[i]);

    bool anyActive = false;
    bool anyChildSplit = false;
    for (int i = 0; i < 4; i++) {
      if (cell.children[i].active)
        anyActive = true;
      if (cell.children[i].children != nullptr)
        anyChildSplit = true;
    }
    if (!anyActive && !anyChildSplit)
      cell.children.reset(); // Nothing active, merge back to parent
    return;
  }

  if (cell.active && cell.rect.width() > GRID_SIZE_MIN)
    splitCell(cell);
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
