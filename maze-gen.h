#include <stdint.h>

struct Maze;
typedef struct Maze Maze;

typedef struct {
    int32_t x;
    int32_t y;
} IVec2;

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

Maze* Maze_new(int32_t x, int32_t y);
void Maze_delete(Maze* this);
IVec2 Maze_dims(const Maze* this);
int8_t Maze_has_edge(const Maze* this, int32_t x1, int32_t y1, int32_t x2, int32_t y2);

