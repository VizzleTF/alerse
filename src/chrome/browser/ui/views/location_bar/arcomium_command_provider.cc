// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/arcomium_command_provider.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/arcomium_space_command_hook.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace {

enum class Kind {
  // A browser command, run with chrome::ExecuteCommand when it is enabled.
  kBrowser,
  // A Space bar or sidebar command (arcomium_space_command_hook.h).
  kSidebar,
};

struct Command {
  int command_id;
  Kind kind;
  int name_id;
  // The name while the command's toggle is on (Unpin Tab), 0 for the same.
  int on_name_id;
  const gfx::VectorIcon* icon;
  const gfx::VectorIcon* on_icon;
  // Other words that find it, as Arc finds Settings by "preferences".
  const char16_t* aliases;
};

// Arc's command bar actions (Arc for macOS' inventory), in Arc's order for
// equally good matches.
const Command kCommands[] = {
    {IDC_COPY_URL, Kind::kBrowser, IDS_ARCOMIUM_MENU_COPY_URL, 0,
     &vector_icons::kContentCopyIcon, nullptr, u"link address"},
    {IDC_ARCOMIUM_COPY_URL_MARKDOWN, Kind::kBrowser,
     IDS_ARCOMIUM_COPY_URL_MARKDOWN, 0,
     &vector_icons::kContentCopyIcon, nullptr, u"link"},
    {IDC_NEW_WINDOW, Kind::kBrowser, IDS_ARCOMIUM_MENU_NEW_WINDOW, 0,
     &kNewWindowIcon, nullptr, u""},
    {IDC_NEW_INCOGNITO_WINDOW, Kind::kBrowser, IDS_ARCOMIUM_MENU_NEW_INCOGNITO,
     0, &kIncognitoIcon, nullptr, u"private"},
    {IDC_ARCOMIUM_PIN_TAB, Kind::kSidebar, IDS_ARCOMIUM_MENU_PIN_TAB,
     IDS_ARCOMIUM_UNPIN_TAB, &kKeepIcon, &kKeepOffIcon, u"pin"},
    {IDC_NEW_SPLIT_TAB, Kind::kBrowser, IDS_ARCOMIUM_ADD_SPLIT_VIEW, 0,
     &kSplitSceneIcon, nullptr, u"new split"},
    {IDC_ARCOMIUM_NEW_SPACE, Kind::kSidebar, IDS_ARCOMIUM_NEW_SPACE, 0,
     &vector_icons::kAdd2Icon, nullptr, u""},
    {IDC_ARCOMIUM_RENAME_SPACE, Kind::kSidebar, IDS_ARCOMIUM_RENAME_SPACE, 0,
     &vector_icons::kEditIcon, nullptr, u""},
    {IDC_ARCOMIUM_EDIT_SPACE_THEME, Kind::kSidebar,
     IDS_ARCOMIUM_MENU_EDIT_THEME, 0, &kBrushFilledIcon, nullptr,
     u"color colour"},
    {IDC_ARCOMIUM_NEW_FOLDER, Kind::kSidebar, IDS_ARCOMIUM_NEW_FOLDER, 0,
     &vector_icons::kFolderOpenIcon, nullptr, u""},
    // Arc for macOS collapses the pinned tabs under the Space's name.
    {IDC_ARCOMIUM_TOGGLE_PINNED, Kind::kSidebar, IDS_ARCOMIUM_COLLAPSE_PINNED,
     IDS_ARCOMIUM_EXPAND_PINNED, &kChevronRightIcon, &kChevronRightIcon,
     u"folders"},
    // Listed only while it can run: a shown tab with a row, text selected.
    {IDC_ARCOMIUM_REVEAL_TAB, Kind::kSidebar, IDS_ARCOMIUM_MENU_REVEAL_TAB, 0,
     &vector_icons::kSearchIcon, nullptr, u"show find"},
    {IDC_ARCOMIUM_COPY_URL_QUOTE, Kind::kSidebar, IDS_ARCOMIUM_COPY_URL_QUOTE,
     0, &vector_icons::kContentCopyIcon, nullptr, u"link selection"},
    {IDC_OPTIONS, Kind::kBrowser, IDS_ARCOMIUM_MENU_SETTINGS, 0,
     &vector_icons::kSettingsIcon, nullptr, u"preferences options"},
    {IDC_SHOW_HISTORY, Kind::kBrowser, IDS_ARCOMIUM_VIEW_HISTORY, 0,
     &vector_icons::kHistoryIcon, nullptr, u"library"},
};

// At most this many action rows.
constexpr size_t kMaxRows = 3;
// The best one leads, above the search for what was typed (1300) but under a
// site the address completes to (1400 and up), as in Arc, where "theme" runs
// Edit Theme but "pin" opens pinterest.com.
constexpr int kLeadRelevance = 1350;
// Others follow the search and the sites.
constexpr int kOtherRelevance = 1240;
// Fewer letters than this never lead, so typing a word does not run an
// action by surprise.
constexpr size_t kMinLeadLength = 3;

// How well `query` names an action called `name`, also found by `aliases`:
// 3 for the whole name, 2 for its start, 1 when every word typed starts a
// word of the name or of the aliases, 0 for none.
int MatchQuality(const std::u16string& query,
                 const std::u16string& name,
                 const std::u16string& aliases) {
  std::u16string plain = base::i18n::ToLower(name);
  // "Edit Theme…" is found by "edit theme".
  base::TrimString(plain, u"…. ", &plain);
  if (plain == query) {
    return 3;
  }
  if (base::StartsWith(plain, query)) {
    return 2;
  }
  const std::vector<std::u16string> words = base::SplitString(
      base::StrCat({plain, u" ", aliases}), u" ", base::TRIM_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  const std::vector<std::u16string> typed = base::SplitString(
      query, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (typed.empty()) {
    return 0;
  }
  for (const std::u16string& word : typed) {
    if (std::ranges::none_of(words, [&](const std::u16string& candidate) {
          return base::StartsWith(candidate, word);
        })) {
      return 0;
    }
  }
  return 1;
}

std::u16string ShortcutFor(BrowserWindowInterface* browser, int command_id) {
  ui::Accelerator accelerator;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view ||
      !browser_view->GetAccelerator(command_id, &accelerator)) {
    return std::u16string();
  }
  return accelerator.GetShortcutText();
}

void RunCommandNow(base::WeakPtr<BrowserWindowInterface> weak_browser,
                   int command_id,
                   Kind kind) {
  BrowserWindowInterface* browser = weak_browser.get();
  if (!browser) {
    return;
  }
  switch (kind) {
    case Kind::kBrowser:
      chrome::ExecuteCommand(browser, command_id);
      break;
    case Kind::kSidebar:
      arcomium::RunSpaceCommand(browser, command_id);
      break;
  }
}

void RunCommand(base::WeakPtr<BrowserWindowInterface> weak_browser,
                int command_id,
                Kind kind) {
  BrowserWindowInterface* browser = weak_browser.get();
  if (!browser) {
    return;
  }
  // The command bar closes first, as Arc's: the page takes the keyboard, so
  // what the command opens (a Space's name to rename) can take it next.
  if (content::WebContents* contents =
          browser->GetTabStripModel()->GetActiveWebContents()) {
    contents->Focus();
  }
  // After the sidebar has seen the focus leave and closed the command bar
  // (it does so in a task of its own): a dialog the command opens takes the
  // window's focus, and the command bar would stay open under it.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RunCommandNow, std::move(weak_browser),
                                command_id, kind));
}

class ArcomiumCommandAction : public OmniboxAction {
 public:
  ArcomiumCommandAction(base::WeakPtr<BrowserWindowInterface> browser,
                        int command_id,
                        Kind kind,
                        const std::u16string& name,
                        const gfx::VectorIcon& icon)
      : OmniboxAction(LabelStrings(name, name, std::u16string(), name),
                      GURL()),
        browser_(std::move(browser)),
        command_id_(command_id),
        kind_(kind),
        icon_(icon) {}

  // OmniboxAction:
  void Execute(ExecutionContext& context) const override {
    // After the omnibox is done opening the row.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RunCommand, browser_, command_id_, kind_));
  }
  const gfx::VectorIcon& GetVectorIcon() const override { return *icon_; }

 private:
  ~ArcomiumCommandAction() override = default;

  base::WeakPtr<BrowserWindowInterface> browser_;
  const int command_id_;
  const Kind kind_;
  const raw_ref<const gfx::VectorIcon> icon_;
};

}  // namespace

ArcomiumCommandProvider::ArcomiumCommandProvider(
    BrowserWindowInterface* browser)
    : AutocompleteProvider(AutocompleteProvider::Type::kArcomiumCommand),
      browser_(browser->GetWeakPtr()) {}

ArcomiumCommandProvider::~ArcomiumCommandProvider() = default;

void ArcomiumCommandProvider::Start(const AutocompleteInput& input,
                                    bool minimal_changes) {
  matches_.clear();
  BrowserWindowInterface* browser = browser_.get();
  std::u16string query = base::i18n::ToLower(input.text());
  base::TrimWhitespace(query, base::TRIM_ALL, &query);
  // Not for an address being typed, nor in a site's search.
  if (!browser || query.size() < 2 || input.IsZeroSuggest() ||
      input.in_keyword_mode() ||
      input.type() == metrics::OmniboxInputType::URL) {
    return;
  }

  struct Found {
    int quality;
    std::u16string name;
    const Command* command;
    bool on;
  };
  std::vector<Found> found;
  for (const Command& command : kCommands) {
    bool on = false;
    switch (command.kind) {
      case Kind::kBrowser:
        if (!chrome::IsCommandEnabled(browser, command.command_id)) {
          continue;
        }
        // Nor has it, or a blank page, an address worth copying.
        if ((command.command_id == IDC_COPY_URL ||
             command.command_id == IDC_ARCOMIUM_COPY_URL_MARKDOWN) &&
            !chrome::GetArcomiumCopyableContents(browser)) {
          continue;
        }
        break;
      case Kind::kSidebar: {
        const arcomium::SpaceCommandState state =
            arcomium::GetSpaceCommandState(browser, command.command_id);
        if (state == arcomium::SpaceCommandState::kUnavailable) {
          continue;
        }
        on = state == arcomium::SpaceCommandState::kOn;
        break;
      }
    }
    const std::u16string name = l10n_util::GetStringUTF16(
        on && command.on_name_id ? command.on_name_id : command.name_id);
    const int quality = MatchQuality(query, name, command.aliases);
    if (quality > 0) {
      found.push_back({quality, name, &command, on});
    }
  }
  // The best match first, then the shorter name.
  std::ranges::stable_sort(found, [](const Found& a, const Found& b) {
    if (a.quality != b.quality) {
      return a.quality > b.quality;
    }
    return a.name.size() < b.name.size();
  });
  if (found.size() > kMaxRows) {
    found.resize(kMaxRows);
  }

  for (size_t i = 0; i < found.size(); ++i) {
    const Found& row = found[i];
    const bool leads = i == 0 && query.size() >= kMinLeadLength;
    AutocompleteMatch match(
        this, leads ? kLeadRelevance : kOtherRelevance - static_cast<int>(i),
        /*deletable=*/false, omnibox::AutocompleteMatchType::kPedal);
    match.allowed_to_be_default_match = leads;
    match.transition = ui::PAGE_TRANSITION_GENERATED;
    match.suggest_type = omnibox::SuggestType::TYPE_NATIVE_CHROME;
    // Each row its own address, so they are not merged as duplicates; it is
    // never opened, the action runs instead.
    match.destination_url = GURL(base::StrCat(
        {"chrome://arcomium-command/",
         base::NumberToString(row.command->command_id)}));
    // The name is the description and the shortcut the contents: the
    // controller swaps the two of a match that has a description, so the name
    // shows first and the popup draws the shortcut as Arc's key chip
    // (searchbox_match.ts).
    match.description = row.name;
    match.description_class = {
        ACMatchClassification(0, ACMatchClassification::NONE)};
    match.contents = ShortcutFor(browser, row.command->command_id);
    if (!match.contents.empty()) {
      match.contents_class = {
          ACMatchClassification(0, ACMatchClassification::NONE)};
    }
    // Arrowing onto the row shows its name, as Arc's does.
    match.fill_into_edit = row.name;
    match.takeover_action = base::MakeRefCounted<ArcomiumCommandAction>(
        browser_, row.command->command_id, row.command->kind, row.name,
        row.on && row.command->on_icon ? *row.command->on_icon
                                       : *row.command->icon);
    matches_.push_back(std::move(match));
  }
}
