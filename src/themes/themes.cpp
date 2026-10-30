#include <Preferences.h>

#include "santa.h"
#include "spider.h"
#include "theme.h"

namespace theme {
namespace {

// Every character, in picker order. A new theme needs its own file and a line here.
const Theme* const kThemes[] = {&santa::theme, &spider::theme};
constexpr size_t kCount = sizeof(kThemes) / sizeof(kThemes[0]);

// Saved by name, so reordering the list doesn't change anyone's choice.
constexpr char kPrefsNamespace[] = "now-playing";
constexpr char kPrefsKey[] = "theme";

size_t active_index = 0;

}  // namespace

void begin() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace);
  String saved = prefs.isKey(kPrefsKey) ? prefs.getString(kPrefsKey) : "";
  prefs.end();
  for (size_t i = 0; i < kCount; i++) {
    if (saved == kThemes[i]->name) active_index = i;
  }
}

size_t count() { return kCount; }

const Theme& at(size_t index) { return *kThemes[index < kCount ? index : 0]; }

const Theme& active() { return *kThemes[active_index]; }

size_t activeIndex() { return active_index; }

void select(size_t index) {
  if (index >= kCount) return;
  active_index = index;
  Preferences prefs;
  prefs.begin(kPrefsNamespace);
  prefs.putString(kPrefsKey, kThemes[index]->name);
  prefs.end();
}

}  // namespace theme
