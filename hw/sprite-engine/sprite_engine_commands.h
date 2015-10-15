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
  uint8_t sprite_size;
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

void fillUpdateOAM(uint8_t oam_index, uint32_t val, struct SECommandUpdateOAM *cmd) {
    memset(cmd, 0, sizeof(struct SECommandUpdateOAM));
    cmd->type = UPDATE_OAM;
    cmd->oam_index = oam_index;
    cmd->enable = ((val & 0x80000000) != 0) ? true : false;
    cmd->palette = (uint8_t) ((val & 0x3F000000) >> 24);
    cmd->flip_x = ((val & 0x00200000) != 0) ? true : false;
    cmd->flip_y = ((val & 0x00100000) != 0) ? true : false;
    cmd->x_offset = (uint16_t) ((val & 0x0007FE00) >> 9);
    cmd->y_offset = (uint16_t) (val & 0x000001FF);
    // other fields 0'd by memset
}

void fillUpdateInstOAM(uint8_t oam_index, uint64_t val, struct SECommandUpdateOAM *cmd) {
    memset(cmd, 0, sizeof(struct SECommandUpdateOAM));
    cmd->type = UPDATE_OAM;
    cmd->oam_index = oam_index;
    cmd->enable = ((val & 0x8000000000000000) != 0) ? true : false;
    cmd->palette = (uint8_t) ((val & 0x3F00000000000000) >> 56);
    cmd->flip_x = ((val & 0x0020000000000000) != 0) ? true : false;
    cmd->flip_y = ((val & 0x0010000000000000) != 0) ? true : false;
    cmd->x_offset = (uint16_t) ((val & 0x0007FE0000000000) >> 42);
    cmd->y_offset = (uint16_t) ((val & 0x000001FF00000000)) >> 32;
    cmd->sprite_size = (uint8_t) ((val & 0x0000000000000060)) >> 5;
    cmd->sprite = (uint8_t) ((val & 0x000000000000001E)) >> 1;
    cmd->transpose = ((val & 0x0000000000000001) != 0) ? true : false;
}

void debugUpdateOAM(int fd, struct SECommandUpdateOAM *cmd) {
    dprintf(fd, "SECommandUpdateOAM {\n"
                    "\toam_index:\t%d\n"
                    "\tenable:\t\t%d\n"
                    "\tpalette:\t%d\n"
                    "\tflip_x:\t\t%d\n"
                    "\tflip_y:\t\t%d\n"
                    "\tx_offset:\t%d\n"
                    "\ty_offset:\t%d\n"
                    "\tsprite_size:\t%d\n"
                    "\tsprite:\t\t%d\n"
                    "\ttranspose:\t%d\n"
                "}\n",
                cmd->oam_index,
                cmd->enable,
                cmd->palette,
                cmd->flip_x,
                cmd->flip_y,
                cmd->x_offset,
                cmd->y_offset,
                cmd->sprite_size,
                cmd->sprite,
                cmd->transpose
             );
}

void fillPriorityControl(uint8_t val, struct SECommandSetPriorityControl *cmd) {
    memset(cmd, 0, sizeof(struct SECommandSetPriorityControl));
    cmd->type = SET_PRIORITY_CTRL;
    cmd->iprctl = ((val & 0x01) != 0) ? true : false;
}

void debugPriorityControl(int fd, struct SECommandSetPriorityControl *cmd) {
    dprintf(fd, "SECommandSetPriorityControl {\n"
                    "\tiprctl:\t%d\n"
                    "}\n",
                    cmd->iprctl
           );
}

void fillUpdateCRAM(uint8_t cram_index, uint32_t val, struct SECommandUpdateCRAM *cmd) {
    memset(cmd, 0, sizeof(struct SECommandUpdateCRAM));
    cmd->type = UPDATE_CRAM;
    cmd->cram_index = cram_index;
    cmd->palette_index = ((val & 0xF0000000) >> 28);
    cmd->user = ((val & 0x0F000000) >> 24);
    cmd->red = ((val & 0x00FF0000) >> 16);
    cmd->green = ((val & 0x0000FF00) >> 8);
    cmd->blue = (val & 0x000000FF);
}

void debugUpdateCRAM(int fd, struct SECommandUpdateCRAM *cmd) {
    dprintf(fd, "SECommandUpdateCRAM {\n"
                    "\tcram_index:\t%d\n"
                    "\tpalette_index:\t%d\n"
                    "\tuser:\t\t%d\n"
                    "\tred:\t\t%d\n"
                    "\tgreen:\t\t%d\n"
                    "\tblue:\t\t%d\n"
                    "}\n",
                    cmd->cram_index,
                    cmd->palette_index,
                    cmd->user,
                    cmd->red,
                    cmd->green,
                    cmd->blue
           );
}

void fillUpdateVRAM(uint8_t cram_index, uint32_t val, struct SECommandUpdateVRAM *cmd) {
    memset(cmd, 0, sizeof(struct SECommandUpdateVRAM));
    cmd->type = UPDATE_VRAM;
    cmd->chunk = ((val & 0xFF800000) >> 23);
    cmd->pixel_y = ((val & 0x007E0000) >> 17);
    cmd->pixel_x = ((val & 0x0001F800) >> 11);
    cmd->p_data = (val & 0x0000000F);
}

void debugUpdateVRAM(int fd, struct SECommandUpdateVRAM *cmd) {
    dprintf(fd, "SECommandUpdateVRAM {\n"
                    "\tchunk:\t\t%d\n"
                    "\tpixel_y:\t%d\n"
                    "\tpixel_x:\t%d\n"
                    "\tp_data:\t\t%d\n"
                    "}\n",
                    cmd->chunk,
                    cmd->pixel_y,
                    cmd->pixel_x,
                    cmd->p_data
           );
}

#endif // SPRITE_ENGINE_COMMANDS_H
