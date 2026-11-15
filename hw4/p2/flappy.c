#include "flappy.h"

/* One pipe slot describes two rectangles: the upper pipe fills the space above
 * the gap, and the lower pipe fills the space between the gap and the floor. */
static Box upperPipeBox(const Pipe *pipe) {
    return (Box){pipe->m_x, 0.0f, PIPE_WIDTH, pipe->m_gap_y};
}

static Box lowerPipeBox(const Pipe *pipe) {
    float top = pipe->m_gap_y + PIPE_GAP_HEIGHT;
    return (Box){pipe->m_x, top, PIPE_WIDTH, FLOOR_Y - top};
}

/* The playable band is [0, FLOOR_Y]; touching either edge exactly is safe. */
static bool birdOutsidePlayableBand(Box bird) {
    bool aboveCeiling = bird.m_y < 0.0f;
    bool belowFloor = bird.m_y + bird.m_height > FLOOR_Y;
    return aboveCeiling || belowFloor;
}

void advancePipes(Game *game, float dt) {
    float shift = PIPE_SPEED * dt;
    /* Every position must be current before the first recyclePipe call: the
     * helper derives the new column from the rightmost pipe, and stale
     * coordinates would place the recycled slot in the wrong column. */
    for (int i = 0; i < PIPE_COUNT; ++i) {
        game->m_pipes[i].m_x -= shift;
    }
    /* Ascending index order keeps the recycled slots in their original
     * sequence, because each new column is placed behind the previous one. */
    for (int i = 0; i < PIPE_COUNT; ++i) {
        if (game->m_pipes[i].m_x + PIPE_WIDTH <= 0.0f) {
            recyclePipe(game, i);
        }
    }
}

bool birdHitsObstacle(const Game *game) {
    Box bird = gameBirdBox(game);
    if (birdOutsidePlayableBand(bird)) {
        return true;
    }
    for (int i = 0; i < PIPE_COUNT; ++i) {
        const Pipe *pipe = &game->m_pipes[i];
        if (boxesOverlap(bird, upperPipeBox(pipe)) || boxesOverlap(bird, lowerPipeBox(pipe))) {
            return true;
        }
    }
    return false;
}

int scorePassedPipes(Game *game) {
    int newPoints = 0;
    for (int i = 0; i < PIPE_COUNT; ++i) {
        Pipe *pipe = &game->m_pipes[i];
        if (pipe->m_scored) {
            continue;
        }
        /* Strictly left of the bird: equality is not a pass yet. */
        if (pipe->m_x + PIPE_WIDTH < BIRD_X) {
            pipe->m_scored = true;
            ++newPoints;
        }
    }
    game->m_score += newPoints;
    return newPoints;
}

unsigned int nextRandom(unsigned int seed) {
    static unsigned int state = RANDOM_INITIAL_SEED;
    if (seed > 0) {
        state = seed % RANDOM_MODULUS;
    }
    state = (RANDOM_MULTIPLIER * state + RANDOM_INCREMENT) % RANDOM_MODULUS;
    return state;
}