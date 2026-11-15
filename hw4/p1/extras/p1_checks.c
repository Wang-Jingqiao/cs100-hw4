/* Optional boundary checks for P1. Lives in p1/extras/ so the course test
 * runner never links this main function. */
#include "snake.h"

#include <stdio.h>

static int failures;
static int checksRun;

static void expect(bool ok, const char *what, int line) {
    ++checksRun;
    if (!ok) {
        ++failures;
        printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(cond) expect((cond), #cond, __LINE__)

/* Compare the ring in link order from m_head; a valid ring returns to the head
 * after exactly `length` links, with no NULL in between. */
static bool checkShape(const Snake *snake, const Coord *expected, size_t length,
                       Direction direction) {
    if (snake->m_head == NULL || snake->m_length != length || snake->m_direction != direction) {
        return false;
    }
    const SnakeNode *node = snake->m_head;
    for (size_t i = 0; i < length; ++i) {
        if (node->m_pos.m_row != expected[i].m_row || node->m_pos.m_col != expected[i].m_col) {
            return false;
        }
        node = node->m_next;
        if (node == NULL) {
            return false;
        }
    }
    return node == snake->m_head;
}

int main(void) {
    Coord headCell = {6, 12};
    Coord tailCell = {6, 9};
    Coord freeCell = {6, 13};

    /* nextRandom first: nothing else has touched the static state yet. */
    CHECK(nextRandom(0) == 11921);
    CHECK(nextRandom(0) == 22458);
    CHECK(nextRandom(1) == 1130);
    CHECK(nextRandom(0) == 25887);
    CHECK(nextRandom(0) == 4656);
    CHECK(nextRandom(1) == 1130);
    CHECK(nextRandom(32768) == 1021);

    Snake snake = {0};
    CHECK(!snakeInit(&snake, 5, 24, 3));   /* rows too small */
    CHECK(!snakeInit(&snake, 12, 5, 3));   /* cols too small */
    CHECK(!snakeInit(&snake, 12, 24, 0));  /* empty snake */
    CHECK(!snakeInit(&snake, 12, 24, 23)); /* longer than the playable row */
    CHECK(snake.m_head == NULL && snake.m_length == 0 && snake.m_direction == DIR_UP);
    CHECK(!snakeContains(&snake, headCell));

    /* 12x24, length 4: head (6,12), body (6,9)..(6,11); link order starts at
     * the tail. */
    static const Coord startFour[] = {{6, 12}, {6, 9}, {6, 10}, {6, 11}};
    CHECK(snakeInit(&snake, 12, 24, 4));
    CHECK(checkShape(&snake, startFour, 4, DIR_RIGHT));

    Coord next = snakeNextPosition(&snake, DIR_LEFT); /* reverse: ignored */
    CHECK(next.m_row == 6 && next.m_col == 13);
    next = snakeNextPosition(&snake, DIR_UP);
    CHECK(next.m_row == 5 && next.m_col == 12);
    next = snakeNextPosition(&snake, (Direction)99); /* invalid: ignored */
    CHECK(next.m_row == 6 && next.m_col == 13);
    CHECK(checkShape(&snake, startFour, 4, DIR_RIGHT));

    CHECK(snakeContains(&snake, headCell));
    CHECK(snakeContains(&snake, tailCell));
    CHECK(!snakeContains(&snake, freeCell));

    /* A normal move reuses the tail node and keeps the length. */
    const SnakeNode *oldTail = snake.m_head->m_next;
    CHECK(snakeStep(&snake, DIR_UP, false) == STEP_OK);
    CHECK(snake.m_head == oldTail && snake.m_length == 4);
    static const Coord afterUp[] = {{5, 12}, {6, 10}, {6, 11}, {6, 12}};
    CHECK(checkShape(&snake, afterUp, 4, DIR_UP));

    CHECK(snakeStep(&snake, DIR_LEFT, false) == STEP_OK);
    static const Coord afterLeft[] = {{5, 11}, {6, 11}, {6, 12}, {5, 12}};
    CHECK(checkShape(&snake, afterLeft, 4, DIR_LEFT));

    /* (6,11) is the old tail cell: growth must not enter it, a normal move may. */
    CHECK(snakeStep(&snake, DIR_DOWN, true) == STEP_SELF);
    CHECK(checkShape(&snake, afterLeft, 4, DIR_LEFT));
    CHECK(snakeStep(&snake, DIR_DOWN, false) == STEP_OK);
    static const Coord looped[] = {{6, 11}, {6, 12}, {5, 12}, {5, 11}};
    CHECK(checkShape(&snake, looped, 4, DIR_DOWN));

    /* A one-node snake may reverse and grows into a two-node ring. */
    snakeDestroy(&snake);
    static const Coord startOne[] = {{6, 12}};
    CHECK(snakeInit(&snake, 12, 24, 1));
    CHECK(checkShape(&snake, startOne, 1, DIR_RIGHT));
    next = snakeNextPosition(&snake, DIR_LEFT);
    CHECK(next.m_row == 6 && next.m_col == 11);
    CHECK(snakeStep(&snake, DIR_RIGHT, true) == STEP_OK);
    static const Coord grewOne[] = {{6, 13}, {6, 12}};
    CHECK(checkShape(&snake, grewOne, 2, DIR_RIGHT));

    /* A snake that fills the only playable row of a 6x6 board: its head already
     * touches the wall, so any rightward step fails before body checks run. */
    snakeDestroy(&snake);
    static const Coord fullRow[] = {{3, 4}, {3, 1}, {3, 2}, {3, 3}};
    CHECK(snakeInit(&snake, 6, 6, 4));
    CHECK(checkShape(&snake, fullRow, 4, DIR_RIGHT));
    CHECK(snakeStep(&snake, DIR_RIGHT, false) == STEP_WALL);
    /* DIR_LEFT is a reverse turn and is ignored, so the snake keeps facing right
     * and hits the wall again. An implementation that honoured the reverse would
     * move into the body cell (3,3) and report STEP_SELF instead. */
    CHECK(snakeStep(&snake, DIR_LEFT, false) == STEP_WALL);
    CHECK(snakeStep(&snake, DIR_RIGHT, true) == STEP_WALL); /* growth is walled too */
    CHECK(checkShape(&snake, fullRow, 4, DIR_RIGHT));

    /* A longer snake curls into a middle body cell: STEP_SELF for both a normal
     * move and growth, with every field unchanged afterwards. */
    snakeDestroy(&snake);
    CHECK(snakeInit(&snake, 12, 24, 5));
    CHECK(snakeStep(&snake, DIR_UP, false) == STEP_OK);
    CHECK(snakeStep(&snake, DIR_LEFT, false) == STEP_OK);
    static const Coord curled[] = {{5, 11}, {6, 10}, {6, 11}, {6, 12}, {5, 12}};
    CHECK(checkShape(&snake, curled, 5, DIR_LEFT));
    CHECK(snakeStep(&snake, DIR_DOWN, false) == STEP_SELF);
    CHECK(checkShape(&snake, curled, 5, DIR_LEFT));
    CHECK(snakeStep(&snake, DIR_DOWN, true) == STEP_SELF);
    CHECK(checkShape(&snake, curled, 5, DIR_LEFT));

    /* Walking into the top wall leaves every field unchanged. */
    snakeDestroy(&snake);
    CHECK(snakeInit(&snake, 12, 24, 3));
    while (snake.m_head->m_pos.m_row > BOARD_WALL_THICKNESS) {
        CHECK(snakeStep(&snake, DIR_UP, false) == STEP_OK);
    }
    CHECK(snakeStep(&snake, DIR_UP, false) == STEP_WALL);
    CHECK(snake.m_head->m_pos.m_row == BOARD_WALL_THICKNESS && snake.m_length == 3);

    /* An invalid direction value is ignored by snakeStep as well. */
    snakeDestroy(&snake);
    static const Coord startThree[] = {{6, 12}, {6, 10}, {6, 11}};
    CHECK(snakeInit(&snake, 12, 24, 3));
    CHECK(checkShape(&snake, startThree, 3, DIR_RIGHT));
    CHECK(snakeStep(&snake, (Direction)99, false) == STEP_OK);
    static const Coord afterInvalid[] = {{6, 13}, {6, 11}, {6, 12}};
    CHECK(checkShape(&snake, afterInvalid, 3, DIR_RIGHT));

    snakeDestroy(&snake);
    CHECK(snake.m_head == NULL && snake.m_length == 0 && snake.m_rows == 0 && snake.m_cols == 0);
    CHECK(!snakeContains(&snake, headCell));
    snakeDestroy(&snake); /* repeated cleanup is safe */
    CHECK(snake.m_head == NULL);

    printf("%d checks, %d failures\n", checksRun, failures);
    return failures == 0 ? 0 : 1;
}