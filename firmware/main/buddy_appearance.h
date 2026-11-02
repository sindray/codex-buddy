#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "buddy_state.h"
#define BUDDY_COLOR_COUNT 6u
#define BUDDY_GEAR_COUNT 6u
#define BUDDY_APPEARANCE_FIELDS 4u
typedef enum { B_GEAR_NONE,B_GEAR_HEADPHONES,B_GEAR_CROWN,B_GEAR_BOW,B_GEAR_ANTENNA,B_GEAR_SCARF } buddy_gear_t;
typedef struct { uint8_t eye,gear,tint; } buddy_appearance_t;
typedef struct { bool open; unsigned field,preview; buddy_appearance_t value; } buddy_appearance_menu_t;
extern const uint32_t buddy_appearance_colors[BUDDY_COLOR_COUNT];
extern const char *const buddy_color_names[BUDDY_COLOR_COUNT];
extern const char *const buddy_gear_names[BUDDY_GEAR_COUNT];
extern const char *const buddy_appearance_fields[BUDDY_APPEARANCE_FIELDS];
buddy_appearance_t buddy_appearance_default(void);
bool buddy_appearance_valid(buddy_appearance_t value);
/* A versioned word keeps storage atomic and rejects older/corrupt records. */
uint32_t buddy_appearance_pack(buddy_appearance_t value);
buddy_appearance_t buddy_appearance_unpack(uint32_t word);
void buddy_appearance_adjust(buddy_appearance_menu_t *menu,int direction);
void buddy_appearance_next(buddy_appearance_menu_t *menu);
buddy_state_t buddy_appearance_preview(const buddy_appearance_menu_t *menu);
