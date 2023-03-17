#include "asciiLines.hpp"

namespace ASCII {

const char* Lines::english[LINE_LINECOUNT] = {
  [LINE_UNIMPLEMENTED] = "Not implemented yet :(",
  [LINE_OK] = "Ok",
  [LINE_BACK] = "Back",
  [LINE_ON] = "On",
  [LINE_OFF] ="Off",
  [LINE_XFLIP] = "Flip Tiles Horizontally",
  [LINE_YFLIP] = "Flip Tiles Vertically",
  [LINE_DFLIP] = "Flip Tiles Diagonally (Swap U with V)",
  [LINE_INVERT_COLORS] = "Swap Tiles's Foreground and Background Colors",
  [LINE_EYEDROPPER] = "Eyedropper Tool",
  [LINE_AFFECT_CHARS] = "Affect Characters",
  [LINE_AFFECT_FG] = "Affect Foreground Colors",
  [LINE_AFFECT_BG] = "Affect Background Colors",
  [LINE_QUICKSAVE] = "Save Artwork",
  [LINE_PALETTE_SCROLL] = "Swipe Left/Right to Scroll Palette Pages",
  [LINE_SWAP_COLORS] = "Swap Foreground and Background Colors",
  [LINE_FILE_SAVE] = "Save Artwork As",
  [LINE_FILE_OPEN] = "Load / Create Artwork",
  [LINE_LOAD_CHARSET] = "Change Charset",
  [LINE_LOAD_PALETTE] = "Change Color Palette",
  [LINE_SETTINGS] = "Artwork Settings (not implemented)",
  [LINE_HINTS] = "Touch Icons Longer to see Hints (like this one!)",
  [LINE_TOGGLE_BACKUPS] = "Keep Backups (Currently %s)",
  [LINE_TOGGLE_HOLD] = "Hold L/R to Switch Screens (Currently %s)",
  [LINE_TOGGLE_WIRELESS] = "Send Artwork to your Server when Saved (Currently %s)",
  [LINE_TOGGLE_HINTS] = "Toggle Hints (you're reading one!) (Currently %s)",
  [LINE_FILES_UP_LEVEL] = "Parent Directory",
  [LINE_FILES_NEW] = "Create New File",
  [LINE_FILES_NEXT_PAGE] = "Next Page",
  [LINE_FILES_PREV_PAGE] = "Previous Page",
  [LINE_STATUS_SAVED] = "Saved %s",
  [LINE_STATUS_LOADED] = "Loaded %s",
  [LINE_STATUS_CHARSET] = "Charset changed to %s",
  [LINE_STATUS_PALETTE] = "Palette changed to %s",
};

}