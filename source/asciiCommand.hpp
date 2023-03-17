#pragma once

#include <list>
#include <memory>
#include "ascii.hpp"
#include "asciiRenderer.hpp"

namespace ASCII {

typedef enum {
  COMMAND_NONE,
  COMMAND_PAINT,
  COMMAND_ACTION
} COMMAND_TYPE;

struct ICommand {
  COMMAND_TYPE type = COMMAND_NONE;

  virtual ~ICommand() {};
  virtual void execute(Data* asciiData, Renderer* renderer) = 0;
  virtual void restore(Data* asciiData, Renderer* renderer) = 0;
  virtual bool equal(ICommand* to) = 0;
};

struct CPaint : public ICommand {
  COMMAND_TYPE type = COMMAND_PAINT;
  
  int layer;
  int index;
  Cell before;
  Cell after;

  ~CPaint() {};
  CPaint(int layer, int index, Cell before, Cell after) {
    this->layer = layer;
    this->index = index;
    this->before = before;
    this->after = after;
  };
  void execute(Data* asciiData, Renderer* renderer);
  void restore(Data* asciiData, Renderer* renderer);
  bool equal(ICommand* to);
};

struct CAction : public ICommand {
  COMMAND_TYPE type = COMMAND_ACTION;

  std::list<ICommand*> commands;

  ~CAction() {
    for (ICommand* cmd : commands) {
      delete cmd;
    }
  };
  ICommand* push(ICommand* command);
  void execute(Data* asciiData, Renderer* renderer);
  void restore(Data* asciiData, Renderer* renderer);
  bool equal(ICommand* to) {return false;};
};

struct History {
  std::list<ICommand*> commands;
  std::list<ICommand*> undone;

  size_t maxCommands = 32;
  
  ICommand* push(ICommand* command);
  ICommand* undo();
  ICommand* redo();
  void clear();
};

}