#include "snake.h"

#include <stdlib.h>

/* The body is a circular singly linked list: in a nonempty snake
 * m_head->m_next is the tail, and following the links walks from the tail
 * through the body to the head and back to the tail. Every body cell stays
 * strictly inside the outer wall row and columns. */

/* Row and column change of one step; an invalid direction does not move. */
static Coord directionDelta(Direction direction) {
    switch (direction) {
    case DIR_UP:
        return (Coord){-1, 0};
    case DIR_RIGHT:
        return (Coord){0, 1};
    case DIR_DOWN:
        return (Coord){1, 0};
    case DIR_LEFT:
        return (Coord){0, -1};
    default:
        return (Coord){0, 0};
    }
}

/* An invalid direction is ignored, and so is a direct reverse while the snake
 * is longer than one node; the stored direction is kept in both cases. */
static Direction effectiveDirection(const Snake *snake, Direction requested) {
    if ((unsigned int)requested >= (unsigned int)DIRECTION_COUNT) {
        return snake->m_direction;
    }
    int opposite = ((int)requested + DIRECTION_COUNT / 2) % DIRECTION_COUNT;
    if (opposite == (int)snake->m_direction && snake->m_length > 1) {
        return snake->m_direction;
    }
    return requested;
}

/* Playable cells are at least BOARD_WALL_THICKNESS away from every edge. */
static bool isInsideBoard(const Snake *snake, Coord pos) {
    bool insideRows =
        pos.m_row >= BOARD_WALL_THICKNESS && pos.m_row < snake->m_rows - BOARD_WALL_THICKNESS;
    bool insideCols =
        pos.m_col >= BOARD_WALL_THICKNESS && pos.m_col < snake->m_cols - BOARD_WALL_THICKNESS;
    return insideRows && insideCols;
}

/* A normal move vacates the old tail cell, so only growth lets the tail block. */
static bool hitsBody(const Snake *snake, Coord pos, bool grow) {
    const SnakeNode *tail = snake->m_head->m_next;
    const SnakeNode *node = snake->m_head;
    for (size_t i = 0; i < snake->m_length; ++i) {
        if (node->m_pos.m_row == pos.m_row && node->m_pos.m_col == pos.m_col) {
            return grow || node != tail;
        }
        node = node->m_next;
    }
    return false;
}

/* Free one ring of `length` nodes. snakeInit uses this during rollback, before
 * the Snake struct owns the ring, so it cannot rely on snake->m_length. */
static void freeRing(SnakeNode *head, size_t length) {
    SnakeNode *node = head;
    for (size_t i = 0; i < length; ++i) {
        SnakeNode *next = node->m_next;
        free(node);
        node = next;
    }
}

bool snakeInit(Snake *snake, int rows, int cols, size_t length) {
    *snake = (Snake){0};
    if (rows < MIN_BOARD_SIZE || cols < MIN_BOARD_SIZE) {
        return false;
    }
    size_t maxLength = (size_t)(cols - 2 * BOARD_WALL_THICKNESS);
    if (length == 0 || length > maxLength) {
        return false;
    }
    int headRow = rows / 2;
    int headCol = cols / 2;
    if (headCol < (int)length) {
        headCol = (int)length; /* max(cols / 2, length) keeps the body on the board */
    }
    SnakeNode *head = malloc(sizeof *head);
    if (head == NULL) {
        return false;
    }
    head->m_pos = (Coord){headRow, headCol};
    head->m_next = head;
    SnakeNode *tail = head;
    size_t built = 1;
    /* Every new node becomes the tail and points at the previous tail, so the
     * finished ring walks from the leftmost body cell up to the head. */
    while (built < length) {
        SnakeNode *node = malloc(sizeof *node);
        if (node == NULL) {
            freeRing(head, built);
            return false;
        }
        node->m_pos = (Coord){headRow, headCol - (int)built};
        node->m_next = tail;
        head->m_next = node;
        tail = node;
        ++built;
    }
    snake->m_head = head;
    snake->m_length = length;
    snake->m_direction = DIR_RIGHT;
    snake->m_rows = rows;
    snake->m_cols = cols;
    return true;
}

Coord snakeNextPosition(const Snake *snake, Direction requested) {
    /* Defensive only: the contract assumes a successful snakeInit. */
    if (snake->m_head == NULL) {
        return (Coord){0, 0};
    }
    Coord delta = directionDelta(effectiveDirection(snake, requested));
    Coord head = snake->m_head->m_pos;
    return (Coord){head.m_row + delta.m_row, head.m_col + delta.m_col};
}

bool snakeContains(const Snake *snake, Coord pos) {
    if (snake->m_head == NULL || snake->m_length == 0) {
        return false;
    }
    const SnakeNode *node = snake->m_head;
    for (size_t i = 0; i < snake->m_length; ++i) {
        if (node->m_pos.m_row == pos.m_row && node->m_pos.m_col == pos.m_col) {
            return true;
        }
        node = node->m_next;
    }
    return false;
}

StepResult snakeStep(Snake *snake, Direction requested, bool grow) {
    /* Defensive only: an initialized snake always has a head. */
    if (snake->m_head == NULL || snake->m_length == 0) {
        return STEP_SELF;
    }
    Direction direction = effectiveDirection(snake, requested);
    Coord delta = directionDelta(direction);
    Coord head = snake->m_head->m_pos;
    Coord next = {head.m_row + delta.m_row, head.m_col + delta.m_col};
    if (!isInsideBoard(snake, next)) {
        return STEP_WALL;
    }
    if (hitsBody(snake, next, grow)) {
        return STEP_SELF;
    }
    if (grow) {
        SnakeNode *newHead = malloc(sizeof *newHead);
        if (newHead == NULL) {
            return STEP_NO_MEMORY;
        }
        SnakeNode *oldHead = snake->m_head;
        newHead->m_pos = next;
        newHead->m_next = oldHead->m_next; /* the old tail stays the tail */
        oldHead->m_next = newHead;
        snake->m_head = newHead;
        ++snake->m_length;
    } else {
        /* Reuse the old tail: it leaves its cell, so no other node changes. */
        SnakeNode *tail = snake->m_head->m_next;
        tail->m_pos = next;
        snake->m_head = tail;
    }
    snake->m_direction = direction;
    return STEP_OK;
}

void snakeDestroy(Snake *snake) {
    if (snake->m_head != NULL) {
        freeRing(snake->m_head, snake->m_length);
    }
    *snake = (Snake){0};
}

unsigned int nextRandom(unsigned int seed) {
    static unsigned int state = RANDOM_INITIAL_SEED;
    if (seed > 0) {
        state = seed % RANDOM_MODULUS;
    }
    state = (RANDOM_MULTIPLIER * state + RANDOM_INCREMENT) % RANDOM_MODULUS;
    return state;
}