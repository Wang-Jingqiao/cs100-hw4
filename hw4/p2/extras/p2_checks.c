/* Optional boundary checks for P2. Lives in p2/extras/ so the course test
 * runner never links this main function. */
#include "../framework/game.h"
#include "flappy.h"

#include <math.h>
#include <stdio.h>

static const float TOLERANCE = 1e-4f;
/* GAP_MARGIN = 56 and FLOOR_Y - PIPE_GAP_HEIGHT - GAP_MARGIN = 172 bound every
 * gap that the provided nextGap helper can produce. */
static const float MIN_GAP_Y = 56.0f;
static const float MAX_GAP_Y = 172.0f;

static int failures;
static int checksRun;

static void expect(bool ok, const char *what, int line) {
    ++checksRun;
    if (!ok) {
        ++failures;
        printf("FAIL line %d: %s\n", line, what);
    }
}

static void expectNear(float actual, float expected, const char *what, int line) {
    ++checksRun;
    if (fabsf(actual - expected) > TOLERANCE) {
        ++failures;
        printf("FAIL line %d: %s (actual %.4f, expected %.4f)\n", line, what, actual, expected);
    }
}

#define CHECK(cond) expect((cond), #cond, __LINE__)
#define CHECK_NEAR(actual, expected) expectNear((actual), (expected), #actual, __LINE__)

/* A game whose pipes all sit far right: nothing recycles by accident and the
 * bird never overlaps a pipe. */
static Game quietGame(void) {
    Game game = {0};
    game.m_bird_y = (FLOOR_Y - BIRD_HEIGHT) / 2.0f;
    for (int i = 0; i < PIPE_COUNT; ++i) {
        game.m_pipes[i] = (Pipe){1000.0f + i * PIPE_SPACING, 114.0f, false};
    }
    game.m_status = PLAYING;
    return game;
}

/* Field-by-field comparison; memcmp would also compare struct padding. */
static bool sameGame(const Game *left, const Game *right) {
    if (left->m_bird_y != right->m_bird_y || left->m_vy != right->m_vy ||
        left->m_elapsed != right->m_elapsed || left->m_score != right->m_score ||
        left->m_best != right->m_best || left->m_status != right->m_status) {
        return false;
    }
    for (int i = 0; i < PIPE_COUNT; ++i) {
        bool samePipe = left->m_pipes[i].m_x == right->m_pipes[i].m_x &&
                        left->m_pipes[i].m_gap_y == right->m_pipes[i].m_gap_y &&
                        left->m_pipes[i].m_scored == right->m_pipes[i].m_scored;
        if (!samePipe) {
            return false;
        }
    }
    return true;
}

int main(void) {
    /* nextRandom first: nothing else has touched the static state yet. */
    CHECK(nextRandom(0) == 11921);
    CHECK(nextRandom(0) == 22458);
    CHECK(nextRandom(1) == 1130);
    CHECK(nextRandom(0) == 25887);
    CHECK(nextRandom(0) == 4656);
    CHECK(nextRandom(1) == 1130);
    CHECK(nextRandom(32768) == 1021);

    /* ---- advancePipes: movement ---- */
    Game game = quietGame();
    game.m_pipes[0] = (Pipe){100.0f, 120.0f, true};
    advancePipes(&game, 0.25f); /* shift = 35 */
    CHECK_NEAR(game.m_pipes[0].m_x, 65.0f);
    CHECK_NEAR(game.m_pipes[1].m_x, 1221.0f);
    /* Unrecycled slots keep their gap and their flag. */
    CHECK_NEAR(game.m_pipes[0].m_gap_y, 120.0f);
    CHECK(game.m_pipes[0].m_scored);

    /* ---- advancePipes: zero dt still recycles, right edge exactly 0 ---- */
    game = quietGame();
    game.m_pipes[0] = (Pipe){-64.0f, 120.0f, true};
    game.m_pipes[1] = (Pipe){100.0f, 130.0f, true};
    game.m_pipes[2] = (Pipe){356.0f, 140.0f, false};
    game.m_pipes[3] = (Pipe){612.0f, 150.0f, false};
    advancePipes(&game, 0.0f);
    /* No pipe is right of VIEW_W, so the helper's floor sets the column. */
    CHECK_NEAR(game.m_pipes[0].m_x, (float)(VIEW_W + PIPE_SPACING));
    CHECK(!game.m_pipes[0].m_scored);
    CHECK(game.m_pipes[0].m_gap_y >= MIN_GAP_Y && game.m_pipes[0].m_gap_y <= MAX_GAP_Y);
    CHECK_NEAR(game.m_pipes[1].m_x, 100.0f);
    CHECK_NEAR(game.m_pipes[2].m_x, 356.0f);
    CHECK_NEAR(game.m_pipes[3].m_x, 612.0f);

    /* ---- advancePipes: all four slots off screen, recycled in order ---- */
    game = quietGame();
    game.m_pipes[0] = (Pipe){-64.0f, 100.0f, true};
    game.m_pipes[1] = (Pipe){-320.0f, 100.0f, true};
    game.m_pipes[2] = (Pipe){-576.0f, 100.0f, true};
    game.m_pipes[3] = (Pipe){-832.0f, 100.0f, true};
    advancePipes(&game, 0.0f);
    for (int i = 0; i < PIPE_COUNT; ++i) {
        CHECK_NEAR(game.m_pipes[i].m_x, (float)(VIEW_W + (i + 1) * PIPE_SPACING));
        CHECK(!game.m_pipes[i].m_scored);
    }

    /* ---- advancePipes: positions must be updated before recycling ---- */
    game = quietGame();
    game.m_pipes[0] = (Pipe){10.0f, 100.0f, true};
    game.m_pipes[1] = (Pipe){2000.0f, 120.0f, false};
    game.m_pipes[2] = (Pipe){2256.0f, 140.0f, false};
    game.m_pipes[3] = (Pipe){2512.0f, 160.0f, false};
    advancePipes(&game, 0.75f); /* shift = 105 */
    /* Stale coordinates would give 2512 + 256 instead of 2407 + 256. */
    CHECK_NEAR(game.m_pipes[0].m_x, 2663.0f);
    CHECK(!game.m_pipes[0].m_scored);
    CHECK_NEAR(game.m_pipes[1].m_x, 1895.0f);
    CHECK_NEAR(game.m_pipes[2].m_x, 2151.0f);
    CHECK_NEAR(game.m_pipes[3].m_x, 2407.0f);
    CHECK_NEAR(game.m_pipes[1].m_gap_y, 120.0f);

    /* ---- advancePipes: exact boundary reached by movement ---- */
    /* The other slots come from quietGame and move to 1151, 1407 and 1663, so
     * the rightmost pipe before recycling is 1663 and the new column is 1919. */
    game = quietGame();
    game.m_pipes[0] = (Pipe){41.0f, 100.0f, true};
    advancePipes(&game, 0.75f); /* 41 - 105 = -64, right edge lands on 0 */
    CHECK_NEAR(game.m_pipes[0].m_x, 1919.0f);
    CHECK(!game.m_pipes[0].m_scored);
    CHECK_NEAR(game.m_pipes[3].m_x, 1663.0f);

    /* ---- birdHitsObstacle: ceiling and floor ---- */
    game = quietGame();
    game.m_bird_y = 0.0f; /* the box now touches the ceiling exactly */
    CHECK(!birdHitsObstacle(&game));
    game.m_bird_y = -0.5f;
    CHECK(birdHitsObstacle(&game));
    game.m_bird_y = (float)(FLOOR_Y - BIRD_HEIGHT); /* bottom edge on the floor */
    CHECK(!birdHitsObstacle(&game));
    game.m_bird_y = (float)(FLOOR_Y - BIRD_HEIGHT) + 0.5f;
    CHECK(birdHitsObstacle(&game));

    /* ---- birdHitsObstacle: a pipe pair at columns [160, 224] ---- */
    /* The gap is [100, 256] and the bird is 22 tall, so m_bird_y in
     * [100, 234] fits; both ends touch a pipe edge exactly. */
    game = quietGame();
    game.m_pipes[0] = (Pipe){160.0f, 100.0f, false};
    game.m_bird_y = 100.0f;
    CHECK(!birdHitsObstacle(&game));
    game.m_bird_y = 99.0f;
    CHECK(birdHitsObstacle(&game));
    game.m_bird_y = 234.0f;
    CHECK(!birdHitsObstacle(&game));
    game.m_bird_y = 235.0f;
    CHECK(birdHitsObstacle(&game));
    game.m_bird_y = 178.0f; /* centred in the gap */
    CHECK(!birdHitsObstacle(&game));

    /* ---- birdHitsObstacle: shared vertical edges do not collide ---- */
    game = quietGame();
    game.m_bird_y = 50.0f; /* overlaps the upper pipe vertically */
    game.m_pipes[0] = (Pipe){(float)(BIRD_X + BIRD_WIDTH), 100.0f, false}; /* left edge 220 */
    CHECK(!birdHitsObstacle(&game));
    game.m_pipes[0].m_x = (float)(BIRD_X + BIRD_WIDTH - 1); /* left edge 219 */
    CHECK(birdHitsObstacle(&game));

    /* ---- birdHitsObstacle must not change the game ---- */
    game = quietGame();
    game.m_bird_y = 50.0f;
    game.m_pipes[0] = (Pipe){160.0f, 100.0f, true};
    Game copy = game;
    CHECK(birdHitsObstacle(&game));
    CHECK(sameGame(&copy, &game));

    /* ---- scorePassedPipes: one point, then no double counting ---- */
    game = quietGame();
    game.m_score = 5;
    game.m_best = 9;
    game.m_pipes[0] = (Pipe){100.0f, 114.0f, false}; /* right edge 164 < 192 */
    CHECK(scorePassedPipes(&game) == 1);
    CHECK(game.m_score == 6);
    CHECK(game.m_pipes[0].m_scored);
    CHECK(scorePassedPipes(&game) == 0);
    CHECK(game.m_score == 6);
    CHECK(game.m_best == 9);
    CHECK(game.m_status == PLAYING);

    /* ---- scorePassedPipes: equality is not a pass ---- */
    game = quietGame();
    game.m_pipes[0] = (Pipe){(float)(BIRD_X - PIPE_WIDTH), 114.0f, false}; /* right edge 192 */
    CHECK(scorePassedPipes(&game) == 0);
    CHECK(game.m_score == 0);
    game.m_pipes[0].m_x = (float)(BIRD_X - PIPE_WIDTH - 1); /* right edge 191 */
    CHECK(scorePassedPipes(&game) == 1);
    CHECK(game.m_score == 1);

    /* ---- scorePassedPipes: several passes in one call ---- */
    game = quietGame();
    game.m_pipes[0] = (Pipe){-100.0f, 114.0f, false};
    game.m_pipes[1] = (Pipe){0.0f, 114.0f, false};
    game.m_pipes[2] = (Pipe){100.0f, 114.0f, false};
    game.m_pipes[3] = (Pipe){500.0f, 114.0f, false};
    CHECK(scorePassedPipes(&game) == 3);
    CHECK(game.m_score == 3);
    CHECK(!game.m_pipes[3].m_scored);

    /* ---- scorePassedPipes: a recycled slot scores again ---- */
    game = quietGame();
    game.m_pipes[0] = (Pipe){100.0f, 114.0f, false};
    CHECK(scorePassedPipes(&game) == 1);
    recyclePipe(&game, 0); /* clears the flag and moves the slot right */
    CHECK(!game.m_pipes[0].m_scored);
    CHECK(game.m_pipes[0].m_x > (float)BIRD_X);
    game.m_pipes[0].m_x = 100.0f;
    CHECK(scorePassedPipes(&game) == 1);
    CHECK(game.m_score == 2);

    /* ---- integration through the provided gameStep ---- */
    Game run;
    gameInit(&run);
    CHECK(run.m_status == READY);
    float firstPipeX = run.m_pipes[0].m_x;
    Input flap = {.m_flap_pressed = true};
    Input idle = {.m_flap_pressed = false};
    gameStep(&run, flap, STEP);
    CHECK(run.m_status == PLAYING);
    CHECK(run.m_pipes[0].m_x < firstPipeX);
    run.m_bird_y = 380.0f;
    run.m_vy = 0.0f;
    gameStep(&run, idle, STEP); /* gravity pushes the bird through the floor */
    CHECK(run.m_status == DEAD);

    printf("%d checks, %d failures\n", checksRun, failures);
    return failures == 0 ? 0 : 1;
}