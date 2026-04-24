#include "maze-gen.h"
#include <complex.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

#include <xmmintrin.h>

const float PI = 3.14159265358979323846f;

typedef struct {
    __m128 inner;
} Quat;

typedef struct {
    float x;
    float y;
} Vec2;

typedef struct {
    float cos;
    float sin;
} Rot2;

typedef struct {
    Vec2 position;
    Rot2 rotation;
} Isometry2;

Rot2 Rot2_identity(void) {
    return (Rot2){ .cos = 1.0f, .sin = 0.0f };
}

Rot2 Rot2_mul(const Rot2 a, const Rot2 b) {
    return (Rot2){
        .cos = a.cos * b.cos - a.sin * b.sin,
        .sin = a.cos * b.sin + a.sin * b.cos,
    };
}

Rot2 Rot2_from_radians(const float radians) {
    return (Rot2){ .cos = cosf(radians), .sin = sinf(radians) };
}

typedef struct {
    Vec2 position;
    Vec2 tex_coord;
} Vertex2;

typedef struct {
    Vec3 position;
    Vec3 tex_coord;
} Vertex3;

typedef struct {
    Vertex2 vertices[3];
} Triangle2;

typedef struct {
    Vertex3 vertices[3];
} Triangle3;

typedef enum { East, North, West, South } CompassQuadrant;

typedef enum { Rot0, Rot90, Rot180, Rot270 } Rotation;

const Rotation search_order[4] = { Rot90, Rot0, Rot270, Rot180 };

CompassQuadrant turn(const CompassQuadrant facing, const Rotation rotation) {
    return (facing + rotation) % 4;
}

IVec2 move_along(IVec2 pos, const CompassQuadrant dir) {
    switch (dir) {
    case East:
        pos.x += 1;
        break;
    case North:
        pos.y += 1;
        break;
    case West:
        pos.x -= 1;
        break;
    case South:
        pos.y -= 1;
        break;
    default:
        printf("In function `move_along`: invalid value for argument `dir`");
        abort();
    }
    return pos;
}

const char* CompassQuadrant_strings[4] = { "east", "north", "west", "south" };

const char* CompassQuadrant_to_string(const CompassQuadrant this) {
    if (this > 4 || this < 0) {
        printf("In function `CompassQuadrant_to_string`: invalid value for argument `this`");
        abort();
    }
    return CompassQuadrant_strings[this];
}

Rot2 CompassQuadrant_to_rotation(const CompassQuadrant this) {
    switch (this) {
    case East:
        return Rot2_identity();
    case North:
        return (Rot2){ .cos = 0.0f, .sin = 1.0f };
    case West:
        return (Rot2){ .cos = -1.0f, .sin = 0.0f };
    case South:
        return (Rot2){ .cos = 0.0f, .sin = -1.0f };
    default:
        printf("In function `CompassQuadrant_to_rotation`: invalid value for argument `this`");
        abort();
    }
}

Rot2 Rot2_inverse(const Rot2 this) {
    return (Rot2){ .cos = this.cos, .sin = -this.sin };
}

float Rot2_to_radians(const Rot2 this) {
    return atan2f(this.sin, this.cos);
}

float Rot2_to_degrees(const Rot2 this) {
    return Rot2_to_radians(this) * 180.0f / PI;
}

Vec2 Isometry2_transform(const Isometry2 this, const Vec2 point) {
    return (Vec2){
        .x = this.position.x + this.rotation.cos * point.x - this.rotation.sin * point.y,
        .y = this.position.y + this.rotation.sin * point.x + this.rotation.cos * point.y,
    };
}

IVec2 tmp_pos;
CompassQuadrant tmp_dir;
Maze* maze;
const IVec2 maze_dims = { .x = 40, .y = 40 };

Vec2 anim_pos;
Rot2 anim_rot;
float anim_wall_height;

void maze_step(void) {
    if (tmp_pos.x != maze_dims.x - 1 || tmp_pos.y != maze_dims.y - 1) {
        // Which way do we go?
        CompassQuadrant next_dir;
        for (size_t i = 0; i < 4; ++i) {
            next_dir = turn(tmp_dir, search_order[i]);
            const IVec2 target = move_along(tmp_pos, next_dir);
            if (target.x < 0 || target.y < 0 || target.x >= maze_dims.x || target.y >= maze_dims.y) continue;
            if (Maze_has_edge(maze, tmp_pos.x, tmp_pos.y, target.x, target.y)) break;
        }
        tmp_dir = next_dir;
        tmp_pos = move_along(tmp_pos, next_dir);
    }
}

float float_move_towards(float current, float target, float max_step) {
    const float offset = target - current;
    const float dist = fabsf(offset);
    if (dist < 0.001f) return target;
    const float dir = offset / dist;
    return current + dir * fminf(dist, max_step);
}

float Vec2_dist(const Vec2 a, const Vec2 b) {
    return sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
}

bool update_anim(void) {
    const Rot2 target_rot = CompassQuadrant_to_rotation(tmp_dir);
    const float delta = Rot2_to_radians(Rot2_mul(target_rot, Rot2_inverse(anim_rot)));
    if (tmp_pos.x == 0 && tmp_pos.y == 0 && anim_wall_height < 1.0f) {
        anim_wall_height = float_move_towards(anim_wall_height, 1.0f, 0.0167f);
        return false;
    }
    if (fabsf(delta) > 0.001f) {
        const float dist = fabsf(delta);
        const float dir = delta / dist;
        anim_rot = Rot2_mul(anim_rot, Rot2_from_radians(dir * fminf(dist, 0.0167f * PI / 2.0f)));
        return false;
    }
    if (Vec2_dist(anim_pos, (Vec2){ .x = tmp_pos.x, .y = tmp_pos.y }) > 0.001f) {
        anim_pos.x = float_move_towards(anim_pos.x, tmp_pos.x, 0.0167f);
        anim_pos.y = float_move_towards(anim_pos.y, tmp_pos.y, 0.0167f);
        return false;
    }
    if (tmp_pos.x == maze_dims.x - 1 && tmp_pos.y == maze_dims.y - 1) {
        if (anim_wall_height > 0.0f) {
            anim_wall_height = float_move_towards(anim_wall_height, 0.0f, 0.0167f);
        } else {
            Maze_delete(maze);
            maze = Maze_new(maze_dims.x, maze_dims.y);
            tmp_pos = (IVec2){ .x = 0, .y = 0 };
            tmp_dir = North;
            anim_pos = (Vec2){ .x = 0.0f, .y = 0.0f };
            anim_rot = CompassQuadrant_to_rotation(tmp_dir);
        }
        return false;
    }
    return true;
}

void draw(void) {
    if (update_anim()) {
        maze_step();
    }
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-Rot2_to_degrees(anim_rot), 0.0f, 0.0f, 1.0f);
    glTranslatef(-anim_pos.x, -anim_pos.y, 0.0f);

    glDisable(GL_TEXTURE_2D);
    glColor3f(0.0f, 1.0f, 0.0f);

    // Arrow points to the right
    Triangle2 cursor = {
        .vertices = {
            { .position = { .x = -0.5f, .y = -0.25f } },
            { .position = { .x = 0.5f, .y = 0.0f } },
            { .position = { .x = -0.5f, .y = 0.25f } },
        }
    };
    for (size_t i = 0; i < 3; ++i) {
        cursor.vertices[i].position = Isometry2_transform((Isometry2){ .position = anim_pos, .rotation = anim_rot }, cursor.vertices[i].position);
    }

    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < 3; ++i) {
        glVertex2f(cursor.vertices[i].position.x, cursor.vertices[i].position.y);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_TRIANGLES);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(0.5f, -0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(maze_dims.x - 1.0f, 1.0f);
    glVertex3f(maze_dims.x - 0.5f, -0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glTexCoord2f(maze_dims.x - 1.0f, 1.0f);
    glVertex3f(maze_dims.x - 0.5f, -0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(maze_dims.x - 1.0f, 0.0f);
    glVertex3f(maze_dims.x - 0.5f, -0.5f, -0.5f);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, maze_dims.y - 0.5f, -0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.5f, maze_dims.y - 0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(maze_dims.x - 1.0f, 1.0f);
    glVertex3f(maze_dims.x - 1.5f, maze_dims.y - 0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, maze_dims.y - 0.5f, -0.5f);
    glTexCoord2f(maze_dims.x - 1.0f, 1.0f);
    glVertex3f(maze_dims.x - 1.5f, maze_dims.y - 0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(maze_dims.x - 1.0f, 0.0f);
    glVertex3f(maze_dims.x - 1.5f, maze_dims.y - 0.5f, -0.5f);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-0.5f, -0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(maze_dims.y, 1.0f);
    glVertex3f(-0.5f, maze_dims.y - 0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glTexCoord2f(maze_dims.y, 1.0f);
    glVertex3f(-0.5f, maze_dims.y - 0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(maze_dims.y, 0.0f);
    glVertex3f(-0.5f, maze_dims.y - 0.5f, -0.5f);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(maze_dims.x - 0.5f, -0.5f, -0.5f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(maze_dims.x - 0.5f, -0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(maze_dims.y, 1.0f);
    glVertex3f(maze_dims.x - 0.5f, maze_dims.y - 0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(maze_dims.x - 0.5f, -0.5f, -0.5f);
    glTexCoord2f(maze_dims.y, 1.0f);
    glVertex3f(maze_dims.x - 0.5f, maze_dims.y - 0.5f, anim_wall_height - 0.5f);
    glTexCoord2f(maze_dims.y, 0.0f);
    glVertex3f(maze_dims.x - 0.5f, maze_dims.y - 0.5f, -0.5f);

    for (int32_t y = 0; y < maze_dims.y; y++) {
        for (int32_t x = 0; x < maze_dims.x; x++) {
            if (x > 0) {
                if (!Maze_has_edge(maze, x, y, x - 1, y)) {
                    glTexCoord2f(0.0f, 0.0f);
                    glVertex3f(x - 0.5f, y - 0.5f, -0.5f);
                    glTexCoord2f(0.0f, 1.0f);
                    glVertex3f(x - 0.5f, y - 0.5f, anim_wall_height - 0.5f);
                    glTexCoord2f(1.0f, 1.0f);
                    glVertex3f(x - 0.5f, y + 0.5f, anim_wall_height - 0.5f);
                    glTexCoord2f(0.0f, 0.0f);
                    glVertex3f(x - 0.5f, y - 0.5f, -0.5f);
                    glTexCoord2f(1.0f, 1.0f);
                    glVertex3f(x - 0.5f, y + 0.5f, anim_wall_height - 0.5f);
                    glTexCoord2f(1.0f, 0.0f);
                    glVertex3f(x - 0.5f, y + 0.5f, -0.5f);
                }
            }
            if (y > 0) {
                if (!Maze_has_edge(maze, x, y, x, y - 1)) {
                    glTexCoord2f(0.0f, 0.0f);
                    glVertex3f(x - 0.5f, y - 0.5f, -0.5f);
                    glTexCoord2f(0.0f, 1.0f);
                    glVertex3f(x - 0.5f, y - 0.5f, anim_wall_height - 0.5f);
                    glTexCoord2f(1.0f, 1.0f);
                    glVertex3f(x + 0.5f, y - 0.5f, anim_wall_height - 0.5f);
                    glTexCoord2f(0.0f, 0.0f);
                    glVertex3f(x - 0.5f, y - 0.5f, -0.5f);
                    glTexCoord2f(1.0f, 1.0f);
                    glVertex3f(x + 0.5f, y - 0.5f, anim_wall_height - 0.5f);
                    glTexCoord2f(1.0f, 0.0f);
                    glVertex3f(x + 0.5f, y - 0.5f, -0.5f);
                }
            }
        }
    }
    
    glEnd();

    glutSwapBuffers();
}

void timer(const int _) {
    glutTimerFunc(16, timer, 0);
    glutPostRedisplay();
}

void init_rendering(int* const argc, char** const argv) {
    glutInit(argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(720, 720);
    glutCreateWindow("Maze");
    glutDisplayFunc(draw);

    glEnable(GL_DEPTH_TEST);

    FILE* const brick = fopen("brick.raw", "rb");
    if (!brick) {
        printf("Failed to open texture file `brick.raw`\n");
        abort();
    }
    unsigned char data[64 * 64 * 4];
    for (size_t i = 0; i < 64; ++i) {
        fread(&data[(63 - i) * 64 * 4], sizeof(unsigned char), 64 * 4, brick);
    }
    // fread(data, sizeof(unsigned char), 64 * 64 * 4, brick);
    fclose(brick);
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glViewport(0, 0, 720, 720);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.1f, 0.1f, -0.1f, 0.1f, 0.08f, 100.0f);
    
    glMatrixMode(GL_MODELVIEW);

    glutTimerFunc(16, timer, 0);
}

int main(int argc, char** const argv) {
    maze = Maze_new(maze_dims.x, maze_dims.y);
    const size_t buffer_size = 2 * (1 + maze_dims.x) * (1 + maze_dims.y) - 3;
    char* const buffer = malloc(sizeof(char) * buffer_size);
    for (int32_t i = 0; i < maze_dims.x - 1; ++i) {
        buffer[2 * i] = ' ';
        buffer[2 * i + 1] = '_';
    }
    buffer[2 * (maze_dims.x - 1)] = '\n';
    const size_t offset = 2 * (maze_dims.x - 1) + 1;
    for (int32_t y = maze_dims.y - 1; y > -1; --y) { // (2 + 2 * maze_dims.x) * maze_dims.y
        buffer[offset + (maze_dims.y - y - 1) * (2 + 2 * maze_dims.x)] = '|'; // 1
        for (int32_t x = 0; x < maze_dims.x; ++x) { // 2 * maze_dims.x
            const int8_t fallback = x == 0 && y == 0;
            const int8_t south_edge = y > 0 ? Maze_has_edge(maze, x, y, x, y - 1) : fallback;
            const int8_t east_edge = x < maze_dims.x - 1 ? Maze_has_edge(maze, x, y, x + 1, y) : 0;
            buffer[offset + (maze_dims.y - y - 1) * (2 + 2 * maze_dims.x) + 1 + 2 * x] = south_edge ? ' ' : '_'; // 1
            buffer[offset + (maze_dims.y - y - 1) * (2 + 2 * maze_dims.x) + 1 + 2 * x + 1] = east_edge ? ' ' : '|'; // 1
        }
        buffer[offset + (maze_dims.y - y - 1) * (2 + 2 * maze_dims.x) + 1 + 2 * maze_dims.x] = '\n'; // 1
    }
    fwrite(buffer, sizeof(char), buffer_size, stdout);

    // Traversal
    tmp_pos = (IVec2){ .x = 0, .y = 0 };
    tmp_dir = North;
    anim_pos = (Vec2){ .x = 0.0f, .y = 0.0f };
    anim_rot = CompassQuadrant_to_rotation(tmp_dir);
    anim_wall_height = 0.0f;
    
    init_rendering(&argc, argv);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutMainLoop();

    Maze_delete(maze);

    return 0;
}
