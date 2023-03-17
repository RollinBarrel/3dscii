#include <ranges>
#include "asciiCommand.hpp"

namespace ASCII {

void CPaint::execute(Data* asciiData, Renderer* renderer) {
  asciiData->layers[layer].data[index] = after;
  renderer->canvas.canvasLayers[layer].writeCell(index, asciiData);
}

void CPaint::restore(Data* asciiData, Renderer* renderer) {
  asciiData->layers[layer].data[index] = before;
  renderer->canvas.canvasLayers[layer].writeCell(index, asciiData);
}

bool CPaint::equal(ICommand* to) {
  if (to->type != COMMAND_PAINT)
    return false;
  return ((CPaint*)to)->index == index;
}

ICommand* CAction::push(ICommand* command) {
  // if we have an identical command, we'll just overwrite it with a new one
  // (i.e. painting same cell twice w/ the same stroke)
  for (auto it = commands.begin(); it != commands.end(); ++it) {
    if ((*it)->equal(command)) {
      delete *it;
      *it = command;
      return command;
    }
  }
  commands.push_back(command);
  return command;
}

void CAction::execute(Data* asciiData, Renderer* renderer) {
  for (ICommand* cmd : commands) {
    cmd->execute(asciiData, renderer);
  }
}

void CAction::restore(Data* asciiData, Renderer* renderer) {
  for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
    (*it)->restore(asciiData, renderer);
  }
}

ICommand* History::push(ICommand* command) {
  for (ICommand* cmd : undone) {
    delete cmd;
  }
  undone.clear();


  if (commands.size() >= maxCommands) {
    delete commands.front();
    commands.pop_front();
  }

  commands.push_back(command);
  return commands.back();
}

ICommand* History::undo() {
  if (commands.empty())
    return NULL;
  ICommand* cmd = commands.back();
  commands.pop_back();
  undone.push_back(cmd);
  return cmd;
}

ICommand* History::redo() {
  if (undone.empty())
    return NULL;
  ICommand* cmd = undone.back();
  undone.pop_back();
  commands.push_back(cmd);
  return cmd;
}


void History::clear() {
  for (ICommand* cmd : commands) {
    delete cmd;
  }
  commands.clear();

  for (ICommand* cmd : undone) {
    delete cmd;
  }
  undone.clear();
}

}