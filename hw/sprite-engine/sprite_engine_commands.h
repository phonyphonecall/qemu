#ifndef SPRITE_ENGINE_COMMANDS_H
#define SPRITE_ENGINE_COMMANDS_H

enum SpriteEngineCommandType {
  UPDATE_OAM,
  SET_PRIORITY_CTRL,
  UPDATE_CRAM,
  UPDATE_VRAM
};

struct SECommandUpdateOAM {
  enum SpriteEngineCommandType type;
  uint8_t oam_index;
  bool enable;
  uint8_t palette;
  bool flip_x;
  bool flip_y;
  uint16_t x_offset, y_offset;
  uint8_t sprite;
  bool transpose;
};

struct SECommandSetPriorityControl {
  enum SpriteEngineCommandType type;
  bool iprctl;
};

struct SECommandUpdateCRAM {
  enum SpriteEngineCommandType type;
  uint8_t cram_index, palette_index, user, red, green, blue;
};

struct SECommandUpdateVRAM {
  enum SpriteEngineCommandType type;
  uint16_t chunk;
  uint8_t pixel_y, pixel_x, p_data;
};

union SECommand {
  enum SpriteEngineCommandType type;
  struct SECommandSetPriorityControl set_priority_control;
  struct SECommandUpdateCRAM update_cram;
  struct SECommandUpdateVRAM update_vram;
};

#endif // SPRITE_ENGINE_COMMANDS_H
