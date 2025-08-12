// adungeon.c — ASCII dungeon using A1/A2/A3 tiling (pure C99)
// Build: gcc -std=c99 -O2 -Wall -Wextra -o adungeon adungeon.c
// Run:   ./adungeon [max_blocks] [place_prob%] [seed]
// e.g.   ./adungeon 1200 70 12345 > dungeon.txt
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define A1_H 8
#define A1_W 13
#define A2_N 225
#define TILE  4

typedef struct { int x, y; } Pos;

// ==== Global arrays ====
static int A1[A1_H][A1_W];
static int A2[A2_N][A2_N];

// ==== Frontier queue & seen grid ====
static Pos *front = NULL;
static int qh = 0, qt = 0;
static unsigned char *seen = NULL;
static int gridW = 0, gridH = 0;

// ==== Helpers ====
static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int in_bounds_block(int x, int y) {
    return x >= 0 && y >= 0 && x + TILE <= A2_N && y + TILE <= A2_N;
}

static int aligned(int v) { return ((v - 5) % TILE) == 0; }

static void gen_A1(void) {
    for (int r = 0; r < A1_H; ++r)
        for (int c = 0; c < A1_W; ++c)
            A1[r][c] = 1 + (rand() % 4);
}

static void sample_A3_from_A1(int A3[TILE][TILE]) {
    int r0 = rand() % (A1_H - TILE + 1);
    int c0 = rand() % (A1_W - TILE + 1);
    for (int r = 0; r < TILE; ++r)
        for (int c = 0; c < TILE; ++c)
            A3[r][c] = A1[r0 + r][c0 + c];
}

static int touches_existing(int x, int y) {
    // If any cell in footprint is already nonzero, or any edge-adjacent cell is nonzero
    for (int r = 0; r < TILE; ++r)
        for (int c = 0; c < TILE; ++c)
            if (A2[y + r][x + c] != 0) return 1;

    if (x - 1 >= 0)
        for (int r = 0; r < TILE; ++r)
            if (A2[y + r][x - 1] != 0) return 1;

    if (x + TILE < A2_N)
        for (int r = 0; r < TILE; ++r)
            if (A2[y + r][x + TILE] != 0) return 1;

    if (y - 1 >= 0)
        for (int c = 0; c < TILE; ++c)
            if (A2[y - 1][x + c] != 0) return 1;

    if (y + TILE < A2_N)
        for (int c = 0; c < TILE; ++c)
            if (A2[y + TILE][x + c] != 0) return 1;

    return 0;
}

static void place_block(int x, int y, int A3[TILE][TILE]) {
    for (int r = 0; r < TILE; ++r)
        for (int c = 0; c < TILE; ++c)
            if (A2[y + r][x + c] == 0)
                A2[y + r][x + c] = A3[r][c];
}

static int idx(int x, int y) {
    int gx = (x - 5) / TILE;
    int gy = (y - 5) / TILE;
    if (gx < 0 || gy < 0 || gx >= gridW || gy >= gridH) return -1;
    return gy * gridW + gx;
}

static void push(int x, int y) {
    if (!in_bounds_block(x, y) || !aligned(x) || !aligned(y)) return;
    int k = idx(x, y);
    if (k < 0) return;
    if (seen[k]) return;
    seen[k] = 1;
    front[qt++] = (Pos){x, y};
}

// ==== Main ====
int main(int argc, char **argv) {
    int max_blocks = 1200;                       // how many 4x4 placements to attempt
    int place_prob = 70;                         // % chance to place when visiting a frontier cell
    unsigned seed = (unsigned)time(NULL);       // default seed

    if (argc > 1) max_blocks = atoi(argv[1]);
    if (argc > 2) place_prob = atoi(argv[2]);
    if (argc > 3) seed = (unsigned)strtoul(argv[3], NULL, 10);
    place_prob = clamp(place_prob, 0, 100);

    srand(seed);

    // Prepare A1 / A2
    gen_A1();
    memset(A2, 0, sizeof(A2));

    // Frontier & seen buffers
    front = (Pos*)malloc(sizeof(Pos) * (1u << 20));
    if (!front) { fprintf(stderr, "OOM(front)\n"); return 1; }
    gridW = (A2_N - 5 + TILE - 1) / TILE;
    gridH = (A2_N - 5 + TILE - 1) / TILE;
    seen = (unsigned char*)calloc((size_t)gridW * gridH, 1);
    if (!seen) { fprintf(stderr, "OOM(seen)\n"); free(front); return 1; }

    int placed = 0;
    int A3[TILE][TILE];

    // Place the seed block at (5,5) unconditionally
    sample_A3_from_A1(A3);
    place_block(5, 5, A3);
    placed++;

    // mark seed seen and prime frontier with 4-neighbors
    int k0 = idx(5, 5); if (k0 >= 0) seen[k0] = 1;
    push(5 + TILE, 5);
    push(5, 5 + TILE);
    push(5 - TILE, 5);
    push(5, 5 - TILE);

    // Growth loop
    while (qh < qt && placed < max_blocks) {
        int pick = qh + (rand() % (qt - qh));   // pop random to reduce bias
        Pos cur = front[pick];
        front[pick] = front[qh++];

        if ((rand() % 100) < place_prob && touches_existing(cur.x, cur.y)) {
            sample_A3_from_A1(A3);
            place_block(cur.x, cur.y, A3);
            placed++;
            // 4-neighbors
            push(cur.x + TILE, cur.y);
            push(cur.x - TILE, cur.y);
            push(cur.x, cur.y + TILE);
            push(cur.x, cur.y - TILE);
        }
        // else: skip → intentional gaps
    }

    // Print map
    const char lut[5] = { '#', '.', ',', ':', ';' };
    printf("# seed=%u blocks=%d prob=%d%%\n", seed, placed, place_prob);
    for (int y = 0; y < A2_N; ++y) {
        for (int x = 0; x < A2_N; ++x) {
            int v = A2[y][x];
            putchar((v >= 0 && v <= 4) ? lut[v] : '?');
        }
        putchar('\n');
    }

    free(seen);
    free(front);
    return 0;
}
