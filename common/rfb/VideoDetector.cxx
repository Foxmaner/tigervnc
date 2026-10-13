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

#include <rfb/VideoDetector.h>

using namespace rfb;

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

const core::Region& VideoDetector::getVideoRegion() const
{
  return videoRegion;
}
