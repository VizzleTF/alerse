// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_ARCOMIUM_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_COLOR_ARCOMIUM_COLOR_MIXER_H_

#include "ui/color/color_provider_key.h"

namespace ui {
class ColorProvider;
}

// Arcomium's sidebar, Space switcher and mini player: Arc's greys on a dark
// frame, the same greys inverted on a light one (a light Space color), and
// the system's in high contrast.
void AddArcomiumColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderKey& key);

#endif  // CHROME_BROWSER_UI_COLOR_ARCOMIUM_COLOR_MIXER_H_
