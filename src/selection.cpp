#include "selection.hpp"
#include "globals.hpp"
#include "history.hpp"

void SelectionHandler::drag_begin(glm::ivec2 pos) {
    release();
    start_pos = pos;
}
void SelectionHandler::drag_end(glm::ivec2 pos) {
    assert(start_pos != glm::ivec2(-1, -1));
    _size = glm::abs(pos - start_pos) + 1;
    start_pos = glm::min(start_pos, pos); // take min so start is always in the top left

    orig_pos = {start_pos, mode1_layer};

    temp_buffer.fill({}, _size);
    selection_buffer.copy(currentMap(), orig_pos, _size);
}

void SelectionHandler::start_from_paste(glm::ivec2 pos, const MapSlice& data) {
    release();
    if(data.size() == glm::ivec2(0)) return;

    _size = data.size();
    start_pos = pos;

    orig_pos = {start_pos, mode1_layer};

    temp_buffer.copy(currentMap(), orig_pos, _size);
    history.push_action(std::make_unique<AreaChange>(orig_pos, temp_buffer));

    // put copied tiles down
    data.paste(currentMap(), orig_pos);
    selection_buffer = data;
}

void SelectionHandler::apply() {
    if(start_pos != glm::ivec2(-1) && orig_pos != glm::ivec3(start_pos, mode1_layer)) {
        history.push_action(std::make_unique<AreaMove>(glm::ivec3(start_pos, mode1_layer), orig_pos, temp_buffer, selection_buffer));
    }
    orig_pos = glm::ivec3(start_pos, mode1_layer);
}

void SelectionHandler::release() {
    apply();
    orig_pos = {-1, -1, -1};
    start_pos = {-1, -1};
    _size = {0, 0};
}

void SelectionHandler::erase() {
    apply();

    // orign_pos == current_pos after apply
    temp_buffer.paste(currentMap(), orig_pos); // put original data back
    history.push_action(std::make_unique<AreaChange>(orig_pos, selection_buffer));
    updateGeometry = true;
    release();
}

void SelectionHandler::cut() {
    clipboard = selection_buffer;
    erase();
}

// move selection to different layer
void SelectionHandler::change_layer(int from, int to) {
    if(from == to) return;
    if(!holding()) return;

    auto& map = currentMap();
    temp_buffer.paste(map, glm::ivec3(start_pos, from)); // put original data back
    temp_buffer.copy(map, glm::ivec3(start_pos, to), _size); // store underlying
    selection_buffer.paste(map, glm::ivec3(start_pos, to)); // place preview on top
    updateGeometry = true;
}

void SelectionHandler::move(glm::ivec2 delta) {
    if(delta == glm::ivec2(0, 0)) return;
    auto& map = currentMap();

    temp_buffer.paste(map, glm::ivec3(start_pos, mode1_layer)); // put original data back

    // move to new pos
    start_pos += delta;

    // store underlying
    temp_buffer.copy(map, glm::ivec3(start_pos, mode1_layer), _size);
    selection_buffer.paste(map, glm::ivec3(start_pos, mode1_layer)); // place preview on top

    updateGeometry = true;
}

bool SelectionHandler::selecting() const {
    return start_pos != glm::ivec2(-1, -1) && _size == glm::ivec2(0);
}
bool SelectionHandler::holding() const {
    return start_pos != glm::ivec2(-1, -1) && _size != glm::ivec2(0);
}
bool SelectionHandler::contains(glm::ivec2 pos) const {
    return pos.x >= start_pos.x && pos.y >= start_pos.y &&
           pos.x < (start_pos.x + _size.x) && pos.y < (start_pos.y + _size.y);
}
glm::ivec3 SelectionHandler::start() const { return glm::ivec3(start_pos, mode1_layer); }
glm::ivec2 SelectionHandler::size() const { return _size; }
