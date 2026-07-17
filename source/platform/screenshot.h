#pragma once

namespace platform {

// Saves the top screen to sdmc:/3ds/myfarm/screenshots/ as a 24-bit BMP.
// With the 3D slider up, both eye framebuffers are captured into one
// side-by-side stereo pair (left | right, 800x240) - view it cross-eyed,
// in a stereo photo viewer, or on the console itself. Slider down saves a
// plain 400x240 shot. Returns false on I/O failure; *wasStereo reports
// which kind was written.
bool saveScreenshot(bool* wasStereo);

} // namespace platform
