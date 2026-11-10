// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ALERSE_COMMAND_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ALERSE_COMMAND_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteInput;
class BrowserWindowInterface;

// Arc's command bar actions: typing an action's name ("copy", "theme",
// "history") lists it as a row of its own, with its icon and keyboard
// shortcut, and picking it runs it in the window instead of navigating. The
// rows are Chromium's standalone action matches (kPedal with a takeover
// action), so the popup, the keyboard and the location bar's icon handle
// them as they do the Lens action.
class AlerseCommandProvider : public AutocompleteProvider {
 public:
  explicit AlerseCommandProvider(BrowserWindowInterface* browser);
  AlerseCommandProvider(const AlerseCommandProvider&) = delete;
  AlerseCommandProvider& operator=(const AlerseCommandProvider&) = delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~AlerseCommandProvider() override;

  base::WeakPtr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ALERSE_COMMAND_PROVIDER_H_
