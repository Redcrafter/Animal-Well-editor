#include "history.hpp"
#include "selection.hpp"
#include "globals.hpp"

static void highlightArea(glm::ivec3 start, glm::ivec2 size) {
    // select region that has been undone. only trigger change if actually moved
    mode1_layer = start.z;
    selection_handler.drag_begin(start);
    selection_handler.drag_end(glm::ivec2(start) + size - 1);
}

void AreaMove::apply() {
    auto& map = currentMap();

    dest_data.swap(map, dest);
    src_data.swap(map, src);

    std::swap(src_data, dest_data);
    std::swap(src, dest);

    highlightArea(dest, dest_data.size());
}

void AreaChange::apply() {
    tiles.swap(currentMap(), position);
    highlightArea(position, tiles.size());
}

void SingleChange::apply() {
    auto& map = currentMap();

    auto t = map.getTile(position.z, position.x, position.y);
    map.setTile(position.z, position.x, position.y, tile);
    tile = t.value();

    highlightArea(position, {1, 1});
}

void MapClear::apply() {
    std::swap(currentMap().rooms, rooms);
}

void SwitchLayer::apply() {
    std::swap(selectedMap, from);
}

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

    el->apply();
    updateGeometry = true;
    redo_buffer.push_back(std::move(el));
}
void HistoryManager::redo() {
    if(redo_buffer.empty()) return;
    selection_handler.release();

    auto el = std::move(redo_buffer.back());
    redo_buffer.pop_back();

    el->apply();
    updateGeometry = true;
    undo_buffer.push_back(std::move(el));
}
void HistoryManager::clear() {
    undo_buffer.clear();
    redo_buffer.clear();
}
