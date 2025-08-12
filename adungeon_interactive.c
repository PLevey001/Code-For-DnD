// adungeon_interactive.c — Interactive A1/A2/A3 tiling dungeon generator (C99)
// Build: gcc -std=c99 -O2 -Wall -Wextra -o adungeon_interactive adungeon_interactive.c
// Run:   ./adungeon_interactive
//
// Output file format:
//   Line 1: metadata "# seed=<n> blocks=<n> prob=<n>%"
//   Then 225 lines of 225 characters, each '0'..'4' (0 = empty/wall; 1..4 = tile types)
// adungeon_interactive_walls.c — Interactive version with 0→'#'
// Build: gcc -std=c99 -O2 -Wall -Wextra -o adungeon_interactive_walls adungeon_interactive_walls.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define A1_H 8
#define A1_W 13
#define A2_N 225
#define TILE  4

typedef struct { int x, y; } Pos;

static int A1[A1_H][A1_W];
static int A2[A2_N][A2_N];
static Pos *frontQ = NULL;
static int qh = 0, qt = 0;
static unsigned char *seen = NULL;
static int gridW = 0, gridH = 0;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int in_bounds_block(int x, int y) { return x >= 0 && y >= 0 && x + TILE <= A2_N && y + TILE <= A2_N; }
static int aligned4(int v) { return ((v - 5) % TILE) == 0; }

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
    if (!in_bounds_block(x, y) || !aligned4(x) || !aligned4(y)) return;
    int k = idx(x, y);
    if (k < 0) return;
    if (seen[k]) return;
    seen[k] = 1;
    frontQ[qt++] = (Pos){x, y};
}

static void flush_to_file(const char *fname, unsigned seed, int placed, int place_prob) {
    FILE *fp = fopen(fname, "w");
    if (!fp) {
        fprintf(stderr, "Error: could not open '%s' for writing.\n", fname);
        return;
    }
    fprintf(fp, "# seed=%u blocks=%d prob=%d%%\n", seed, placed, place_prob);
    for (int y = 0; y < A2_N; ++y) {
        for (int x = 0; x < A2_N; ++x) {
            int v = A2[y][x];
            if (v == 0) fputc('#', fp);
            else fputc('0' + v, fp);
        }
        fputc('\n', fp);
    }
    fclose(fp);
}

int main(void) {
    int max_blocks = 1200;
    int place_prob = 70;
    unsigned seed;

    char line[256];
    char outname[256];

    printf("Max blocks — upper bound on 4x4 placements. Typical: 800–3000.\nEnter max blocks [%d]: ", max_blocks);
    if (fgets(line, sizeof line, stdin)) {
        int tmp = atoi(line);
        if (tmp > 0) max_blocks = tmp;
    }

    printf("\nPlace probability (0–100) — chance to place when visiting a frontier cell.\nHigher => denser dungeon.\nEnter place probability [%d]: ", place_prob);
    if (fgets(line, sizeof line, stdin)) {
        int tmp = atoi(line);
        place_prob = clampi(tmp, 0, 100);
    }

    printf("\nSeed — fixes randomness (0 = random based on time).\nEnter seed [0]: ");
    unsigned long tmpseed = 0UL;
    if (fgets(line, sizeof line, stdin)) {
        tmpseed = strtoul(line, NULL, 10);
    }
    if (tmpseed == 0UL) seed = (unsigned)time(NULL);
    else seed = (unsigned)tmpseed;

    printf("\nOutput filename (e.g., dungeon.txt): ");
    if (!fgets(outname, sizeof outname, stdin)) {
        fprintf(stderr, "No filename; aborting.\n");
        return 1;
    }
    size_t L = strlen(outname);
    if (L && outname[L-1] == '\n') outname[L-1] = '\0';
    if (outname[0] == '\0') {
        fprintf(stderr, "Empty filename; aborting.\n");
        return 1;
    }

    srand(seed);
    gen_A1();
    memset(A2, 0, sizeof(A2));

    frontQ = malloc(sizeof(Pos) * (1u << 20));
    gridW = (A2_N - 5 + TILE - 1) / TILE;
    gridH = (A2_N - 5 + TILE - 1) / TILE;
    seen = calloc((size_t)gridW * gridH, 1);

    int placed = 0;
    int A3[TILE][TILE];
    sample_A3_from_A1(A3);
    place_block(5, 5, A3);
    placed++;
    int k0 = idx(5, 5); if (k0 >= 0) seen[k0] = 1;
    push(5 + TILE, 5);
    push(5, 5 + TILE);
    push(5 - TILE, 5);
    push(5, 5 - TILE);

    while (qh < qt && placed < max_blocks) {
        int pick = qh + (rand() % (qt - qh));
        Pos cur = frontQ[pick];
        frontQ[pick] = frontQ[qh++];
        if ((rand() % 100) < place_prob && touches_existing(cur.x, cur.y)) {
            sample_A3_from_A1(A3);
            place_block(cur.x, cur.y, A3);
            placed++;
            push(cur.x + TILE, cur.y);
            push(cur.x - TILE, cur.y);
            push(cur.x, cur.y + TILE);
            push(cur.x, cur.y - TILE);
        }
    }

    flush_to_file(outname, seed, placed, place_prob);
    printf("\nWrote %s (seed=%u, blocks=%d, prob=%d%%)\n", outname, seed, placed, place_prob);

    free(seen);
    free(frontQ);
    return 0;
}
