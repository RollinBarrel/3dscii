#include "fileExplorer.hpp"

bool FileExplorer::load(std::string path) {
  dir = opendir(path.c_str());
  if (dir == NULL)
    return false;

  entries.clear();
  btns.clear();
  renderer->clear();
  rendered = false;
  this->path = path;

  struct dirent* ent;
  while ((ent = readdir(dir)) != NULL) {
    std::string name(ent->d_name);
    // Citra reports a "not a file" error for every directory
    // is_directory() or stat() called on, and yet it returns the correct value
    bool isDir = std::filesystem::is_directory((path + name));

    if (!isDir && filterEnabled) {
      size_t dot = name.find_last_of('.');
      if (dot == name.npos || name.substr(dot + 1) != filter)
        continue;
    }

    Entry* entry = &entries.emplace_back();
    strcpy(entry->name, ent->d_name);
    entry->isDirectory = isDir;
  }

  closedir(dir);

  std::sort(entries.begin(), entries.end());
  page = 0;
  pageCount = (entries.size() + pageSize - 1) / pageSize;

  return true;
}

void FileExplorer::changePage(unsigned int page) {
  btns.clear();
  renderer->clear();
  rendered = false;

  this->page = page;
}

ASCII::UI::Button* FileExplorer::initBtn(int index) {
  auto btn = &btns.emplace_back();

  btn->aabb.l = -160.f;
  btn->aabb.r = 160.f;
  btn->aabb.u = -120.f + index * 20.f;
  btn->aabb.d = -120.f + (index + 1) * 20.f;

  btn->onSwap = [this, index](bool pressed) {
    this->renderer->writeColor(index, pressed ? 0xAAAAAAFF : 0xFFFFFFFF);
  };
  btn->onSync = [](ASCII::UI::Button*){};
  btn->onDrag = [](float, float){};
  btn->onDown = [](){};
  btn->onUp = [](){};

  return btn;
}

//TODO: scrollable list
// pages are a temporary measure
void FileExplorer::update(bool force) {
  if (rendered && !force)
    return;
  rendered = true;

  renderer->offset[0] = -160.f;
  renderer->offset[1] = -120.f;

  int index = 0;

  renderer->writeText(index++, (std::string("sd:") + path).c_str(), 0x33FF33FF);
  // renderer->icons->

  if (path != "/" && (!limitedToRoot || rootPath != path)) {
    renderer->writeText(index, "..", 0xFFFFFFFF);

    auto btn = initBtn(index);
    btn->onPress = [this]() {
      size_t lastSlash = path.find_last_of('/', path.length() - 2);
      this->load(path.substr(0, lastSlash + 1));
    };

    ++index;
  }
  if (mode == EXPLORER_CREATE) {
    renderer->writeText(index, newFileLine.c_str(), 0xFFFFFFFF);

    auto btn = initBtn(index);
    btn->onPress = [this]() {
      char input[128];
      SwkbdState swkbd;
      swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 1, 32);
      swkbdSetHintText(&swkbd, "File Name");
      swkbdSetInitialText(&swkbd, "file.3dscii");
      swkbdSetFeatures(&swkbd, SWKBD_FIXED_WIDTH | SWKBD_ALLOW_HOME | SWKBD_ALLOW_RESET | SWKBD_ALLOW_POWER | SWKBD_DEFAULT_QWERTY);
      swkbdSetNumpadKeys(&swkbd, L'.', L'.');
      SwkbdButton res = swkbdInputText(&swkbd, input, sizeof(input));
      if (res == SWKBD_BUTTON_CONFIRM) 
        callback(path + input);
    };

    ++index;
  }
  if (pageCount > 1) {
    if (page < pageCount - 1) {
      renderer->writeText(index, nextPageLine.c_str(), 0xFFFFFFFF);

      auto btn = initBtn(index);
      btn->onPress = [this]() {
        changePage(page + 1);
      };

      ++index;
    }
    if (page > 0) {
      renderer->writeText(index, prevPageLine.c_str(), 0xFFFFFFFF);

      auto btn = initBtn(index);
      btn->onPress = [this]() {
        changePage(page - 1);
      };
      
      ++index;
    }
  }

  size_t entryId = page * pageSize;
  while (entryId < entries.size() && index < renderer->maxElements) {
    Entry* entry = &entries[entryId];

    renderer->writeText(index, entry->name, 0xFFFFFFFF);

    auto btn = initBtn(index);
    if (entry->isDirectory) {
      btn->onPress = [this, entry]() {
        this->load(path + entry->name + "/");
      };
    } else {
      btn->onPress = [this, entry]() {
        callback(path + entry->name);
      };
    }

    ++index;
    ++entryId;
  }
}