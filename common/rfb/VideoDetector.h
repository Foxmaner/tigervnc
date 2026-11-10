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
#include <cstdint>
#include <sys/time.h>
#include <vector>

namespace rfb {

class PixelBuffer;

class VideoDetector {
public:
  // Setting variables to tune
  //_________________________
  //  Min/Max size of the grid cells in the quadtree.
  static const int GRID_SIZE_MAX = 16;
  static const int GRID_SIZE_MIN = 8;

  static const unsigned BUCKET_MS =
      100; // Milliseconds per bucket in the history
  static const unsigned HISTORY_BUCKETS =
      50; // Number of buckets in the history (Maximun 64!!!)
  static const unsigned HISTORY_MS =
      BUCKET_MS * HISTORY_BUCKETS; // Total history time
  static constexpr double MIN_THRESHOLD_BUCKET_RATIO =
      0.5; // Minimum number of changed buckets to be considered video
  static constexpr double MIN_VIDEO_REGION_RATIO =
      0.25; // Minimum ratio of video region to framebuffer area to be
            // considered video
  //_________________________

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
    uint64_t historicalBuckets = 0; // Bitfield of the last HISTORY_BUCKETS
                                    // buckets, 1 = changed, 0 = unchanged
    unsigned lastBucket = 0; // The last bucket index when the cell was updated
    bool active = false;     // True if cell is concidered to be video
  };

  // A cell in the top level grid, with the indexes of its eight
  // neighboring supercells. If neighbor is outside the frame, the index is -1.
  struct SuperCell : Cell {
    int neighborSuperCells[8];
  };

  std::vector<SuperCell> superCells;

  int nrOfSuperCols = 0;
  int nrOfSuperRows = 0;

  core::Rect fbRect;

  timeval startTime; // Time when VideoDetector was initialized

  bool isEdgeCell(const SuperCell &cell)
      const; // Returns true if the cell is on the edge of the video zone.

  // Returns the largest bounding box of connected active cells.
  core::Rect findLargestVideoArea() const;

  unsigned currentBucket() const; // Returns the current bucket index based on
                                  // the elapsed time since startTime
  void ageCell(
      Cell &cell,
      unsigned now); // Ages the cells historicalBuckets to the current bucket.
  unsigned countChangedBuckets(
      const Cell &cell) const; // Counts the number of changed buckets in the
                               // cell's historicalBuckets
};

} // namespace rfb

#endif
