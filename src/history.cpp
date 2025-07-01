#include "history.hpp"
#include "selection.hpp"
#include "globals.hpp"

void HistoryManager::push_action(std::unique_ptr<HistoryItem> item) {
    undo_buffer.push_back(std::move(item));
    if(undo_buffer.size() > max_undo_size)
        undo_buffer.pop_front();
    redo_buffer.clear();
}

// undo most recent item
void HistoryManager::undo() {
    if(undo_buffer.empty()) return;
    selection_handler.release();

    auto el = std::move(undo_buffer.back());
    undo_buffer.pop_back();

    auto area = el->apply(game_data.maps[selectedMap]);
    updateGeometry = true;

    // select region that has been undone. only trigger change if actually moved
    mode1_layer = area.first.z;
    selection_handler.drag_begin(area.first);
    selection_handler.drag_end(glm::ivec2(area.first) + area.second - 1);

    redo_buffer.push_back(std::move(el));
}
void HistoryManager::redo() {
    if(redo_buffer.empty()) return;
    selection_handler.release();

    auto el = std::move(redo_buffer.back());
    redo_buffer.pop_back();

    auto area = el->apply(game_data.maps[selectedMap]);
    updateGeometry = true;

    // selct region that has been redone. only trigger change if actually moved
    mode1_layer = area.first.z;
    selection_handler.drag_begin(area.first);
    selection_handler.drag_end(glm::ivec2(area.first) + area.second - 1);

    undo_buffer.push_back(std::move(el));
}
void HistoryManager::clear() {
    undo_buffer.clear();
    redo_buffer.clear();
}
