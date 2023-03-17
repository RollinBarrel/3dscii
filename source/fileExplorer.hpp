
#pragma once

#include <strings.h>
#include <algorithm>
#include <dirent.h>
#include <filesystem>
#include "asciiRenderer.hpp"
#include "asciiUI.hpp"

class FileExplorer {
  DIR* dir;

  ASCII::UI::Button* initBtn(int index);
public:
  typedef enum {
    EXPLORER_CREATE,
    EXPLORER_LOAD
  } EXPLORER_MODE;

  struct Entry {
    char name[256];
    bool isDirectory;

    bool operator<(const FileExplorer::Entry& o) {
      int dir = (int)o.isDirectory - (int)isDirectory;
      if (dir != 0)
        return dir < 0;

      return strcasecmp(name, o.name) < 0;
    }
  };

  ASCII::Renderer::RFiles* renderer;
  bool rendered = false;

  std::string upLevelLine = "Parent Directory";
  std::string newFileLine = "Create New File";
  std::string nextPageLine = "Next Page";
  std::string prevPageLine = "Previous Page";

  std::vector<Entry> entries;

  EXPLORER_MODE mode = EXPLORER_CREATE;
  std::string path;
  std::string rootPath;
  bool limitedToRoot = false;
  unsigned int pageSize;
  unsigned int page = 0;
  unsigned int pageCount = 0;
  std::string filter;
  bool filterEnabled = false;
  std::function<void(std::string path)> callback;
  std::vector<ASCII::UI::Button> btns;

  FileExplorer() {};
  FileExplorer(ASCII::Renderer::RFiles* renderer) {
    this->renderer = renderer;
    pageSize = renderer->maxElements - 4;
  };

  bool load(std::string path);
  void close() {
    rendered = false;
    page = 0;
    pageCount = 0;
    filterEnabled = false;
    entries.clear();
    renderer->clear();
  };
  void changePage(unsigned int page);
  void update(bool force = false);
};