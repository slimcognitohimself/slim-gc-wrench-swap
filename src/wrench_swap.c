/* Wrench Swap: a native OpenRAC mod for Ratchet & Clank: Going Commando (NTSC-U v1.01, NTSC-U v2.00
 * Greatest Hits and PAL).
 *
 * Adds "Wrench Swap" to the pause menu's Options page. Its "Omniwrench" setting switches
 * between the wrenches you have found: 8000 (the start), 10000 (Tabora) and 12000 (Aranos
 * Prison). It is greyed out until you have the 10000.
 *
 * A swap does what the game does when it gives you a new wrench (rac2-decomp: Tabora's
 * L08_003F6E18 and the prison's L14_003F4F98):
 *   menu_seek_chain_entry(10, n)        the wrench (item slot 10) to version n of its chain
 *   gfx_run_screen_update_steps(v)      its look: on Tabora and in the prison, the game's own
 * and elsewhere the same three steps that function takes:
 *   LoadPackSingleTexture(v)            the version's texture to VRAM, its model to the buffer
 *   gfx_send_gs_register_packet(v)      the texture registers (sets g_current_weapon_channel)
 *   load_copy_header_relocate_pointers  the hero class's header pointed at the new model
 * In challenge mode the chain's versions are 3 to 5 (Tabora gives 4, the prison 5), and all three
 * are offered from the start. A level load in challenge mode brings back the base wrench's model
 * (the record stays), so the one picked is remembered (with the save) and put back at the start
 * of each level.
 *
 * Cheat Mode (the launcher's setting "cheat_mode"): a normal game is handled like challenge mode,
 * with the normal three wrenches: all offered from the start, the pick remembered and put back at
 * each level start. The cheat never counts as finding a wrench, so turned off, the mod is back to
 * the wrenches really found (a cheat pick above them goes back to the best one found).
 */
#include <stddef.h>
#include <stdint.h>

#ifndef OPENRAC_MOD_TARGET_MINOR /* the build sets it from the manifest\'s "api" */
#define OPENRAC_MOD_TARGET_MINOR 16
#endif
#include "openrac/openrac_mod.h"
#include "addrs.h"

OPENRAC_MOD_DEFINE("rac2-wrench-swap", "0.2.1")

/* The disc's addresses: its fixed ones (the same in every level program) and its tables of
 * per-level functions (addrs.h). PAL's were found by matching its code against NTSC-U's; v2.00's
 * are rac2-decomp's (usa-v2, us2). Each was checked against OpenRAC's own build for its disc. */
typedef struct disc_addrs {
    const char* key;          /* OpenRAC's disc id */
    uint32_t map;             /* s32 current level */
    uint32_t game_mode;       /* s32 in levels: 0 play, 2 scene, 3 pause, 6 loading */
    uint32_t hero_char;       /* u8: 0 Ratchet, 1 Clank */
    uint32_t hero_moby;       /* u32: the moby the player controls */
    uint32_t wrench_record;   /* u8: g_item_slot_to_record[10] */
    uint32_t challenge;       /* u8: set in challenge mode (flag_is_byte_set) */
    uint32_t weapon_channel;  /* s32: the wrench look loaded now */
    uint32_t planets;         /* u8 a level: on the ship's map (Planet_Unlock) */
    const uint32_t *seek, *loadpack, *gspacket, *refresh;
} disc_addrs;

static const disc_addrs DISC_NTSC = {
    "ntsc", 0x1A79F0u, 0x1A8F00u, 0x18C0D4u, 0x18C0B0u, 0x139572u, 0x1A7A0Au, 0x1A72C0u, 0x1A7BC8u,
    ADDR_SEEK_NTSC, ADDR_LOADPACK_NTSC, ADDR_GSPACKET_NTSC, ADDR_REFRESH_NTSC};
static const disc_addrs DISC_PAL = {
    "pal", 0x1A7A78u, 0x1A8F80u, 0x18C154u, 0x18C130u, 0x1395F2u, 0x1A7A92u, 0x1A7340u, 0x1A7C50u,
    ADDR_SEEK_PAL, ADDR_LOADPACK_PAL, ADDR_GSPACKET_PAL, ADDR_REFRESH_PAL};
static const disc_addrs DISC_NTSC2 = {
    "ntsc2", 0x1A79F8u, 0x1A8F00u, 0x18C0D4u, 0x18C0B0u, 0x139572u, 0x1A7A12u, 0x1A72C0u, 0x1A7BD0u,
    ADDR_SEEK_NTSC2, ADDR_LOADPACK_NTSC2, ADDR_GSPACKET_NTSC2, ADDR_REFRESH_NTSC2};
static const disc_addrs* g_disc = &DISC_NTSC;

#define A_MAP            (g_disc->map)
#define A_GAME_MODE      (g_disc->game_mode)
#define A_HERO_CHAR      (g_disc->hero_char)
#define A_HERO_MOBY      (g_disc->hero_moby)
#define A_WRENCH_RECORD  (g_disc->wrench_record)
#define A_CHALLENGE      (g_disc->challenge)
#define A_WEAPON_CHANNEL (g_disc->weapon_channel)
#define A_PLANETS        (g_disc->planets)
#define ADDR_SEEK        (g_disc->seek)
#define ADDR_LOADPACK    (g_disc->loadpack)
#define ADDR_GSPACKET    (g_disc->gspacket)
#define ADDR_REFRESH     (g_disc->refresh)

#define HERO_CLASS 0x47           /* g_class_slot_indices[0x47]: the class whose header is relocated */
#define MAX_CHAIN 8

static const openrac_api* g_api;
static openrac_mod* g_mod;
static int g_page = -1;

static int g_rec_of_pos[MAX_CHAIN]; /* record index of each chain position, -1 unknown */
static int g_chain_len;             /* positions known (0: not probed yet) */
static int g_highest[2];            /* highest version unlocked: [0] normal, [1] challenge (0..2) */
static int g_pending = -1;          /* version asked for in the menu, not applied yet */
static int g_last_level = -2;
static int g_chosen[2] = {-1, -1};  /* picked in the menu, put back each level: [0] normal (Cheat
                                     * Mode), [1] challenge; -1 none */
static int g_cheat;                 /* the launcher's Cheat Mode setting */
static int g_restored_level = -2;   /* challenge mode: level whose start the pick was put back in */
static int g_restore_tries;         /* frames left to try putting it back this level */
static int g_restore_wait;          /* frames to wait in play before trying */
static int g_restore_backoff;       /* frames to wait after a failed try */
static int g_restore_forced;        /* the pick has been put back once this level */

#define RESTORE_WAIT 10   /* frames in play before putting the pick back */
#define RESTORE_TRIES 300 /* then watched for this many frames in play (5 s) */

static const char* const VERSION_NAMES[3] = {"8000", "10000", "12000"};

/* --- small helpers ------------------------------------------------------------------- */

static uint8_t rd8(uint32_t a) { return *(volatile uint8_t*)(g_api->memory + a); }
static uint16_t rd16(uint32_t a) { return *(volatile uint16_t*)(g_api->memory + a); }
static uint32_t rd32(uint32_t a) { return *(volatile uint32_t*)(g_api->memory + a); }
static void wr8(uint32_t a, uint8_t v) { *(volatile uint8_t*)(g_api->memory + a) = v; }
static void wr32(uint32_t a, uint32_t v) { *(volatile uint32_t*)(g_api->memory + a) = v; }

static int in_ram(uint32_t a) { return a >= 0x100000u && a < 0x2000000u; }

static void logf_(int level, const char* a, int n, const char* b) {
    char buf[160];
    int i = 0;
    for (; a && *a && i < 120; a++) buf[i++] = *a;
    if (n != -9999) {
        char d[12];
        int k = 0, neg = n < 0;
        unsigned u = neg ? (unsigned)(-n) : (unsigned)n;
        do { d[k++] = (char)('0' + u % 10); u /= 10; } while (u && k < 11);
        if (neg) buf[i++] = '-';
        while (k) buf[i++] = d[--k];
    }
    for (; b && *b && i < 158; b++) buf[i++] = *b;
    buf[i] = 0;
    g_api->log(g_mod, level, buf);
}
#define LOG(s) logf_(OPENRAC_LOG_INFO, (s), -9999, 0)
#define WARN(s) logf_(OPENRAC_LOG_WARN, (s), -9999, 0)

/* The level whose program is loaded and playing: -1 while loading or outside a level. */
static int level_now(void) {
    int32_t l = (int32_t)rd32(A_MAP);
    if (l < 0 || l >= LEVEL_COUNT || g_api->loaded_level() != l) return -1;
    return l;
}

static int challenge(void) { return rd8(A_CHALLENGE) != 0; }
static int base_pos(void) { return challenge() ? 3 : 0; }

typedef int (*seek_fn)(int slot, int n);
typedef void (*int_fn)(int value);

static void* game_fn(const uint32_t* table, int level) {
    if (level < 0 || level >= LEVEL_COUNT || table[level] == 0) return 0;
    return g_api->function_at(table[level]);
}

/* --- the wrench's chain ---------------------------------------------------------------- */

/* Learns the record of each position by asking the game (menu_seek_chain_entry) and putting
 * the wrench back. Only the record byte changes, and it is restored before the game runs. */
static int probe_chain(int level) {
    seek_fn seek = (seek_fn)game_fn(ADDR_SEEK, level);
    uint8_t keep;
    int n;
    if (!seek) return 0;
    keep = rd8(A_WRENCH_RECORD);
    for (n = 0; n < MAX_CHAIN; n++) g_rec_of_pos[n] = -1;
    g_chain_len = 0;
    for (n = 0; n < MAX_CHAIN; n++) {
        if (!seek(10, n)) break;
        g_rec_of_pos[n] = rd8(A_WRENCH_RECORD);
        g_chain_len = n + 1;
    }
    wr8(A_WRENCH_RECORD, keep);
    logf_(OPENRAC_LOG_INFO, "wrench chain has ", g_chain_len, " versions");
    return g_chain_len > 0;
}

static int current_pos(void) {
    int r = rd8(A_WRENCH_RECORD), n;
    for (n = 0; n < g_chain_len; n++)
        if (g_rec_of_pos[n] == r) return n;
    return -1;
}

/* Save data: 'W', format, highest normal, highest challenge[, challenge pick[, normal cheat pick]]
 * (a pick 0xFF: none). Format 1 (0.1.0-0.1.2) has no picks, 2 (0.1.3-0.1.4) the challenge one,
 * 3 both. */
static uint8_t pick_byte(int v) { return (uint8_t)(v >= 0 ? v : 0xFF); }

static void save_progress(void) {
    uint8_t data[6] = {'W', 3, (uint8_t)g_highest[0], (uint8_t)g_highest[1],
                       pick_byte(g_chosen[1]), pick_byte(g_chosen[0])};
    if (g_api->save_data_set) g_api->save_data_set(g_mod, data, sizeof data);
}

static void load_progress(void* user) {
    uint8_t data[6] = {0, 0, 0, 0, 0xFF, 0xFF};
    uint32_t got = 0;
    (void)user;
    g_highest[0] = g_highest[1] = 0;
    g_pending = -1;
    g_chosen[0] = g_chosen[1] = -1;
    g_restored_level = -2;
    if (g_api->save_data_get) got = g_api->save_data_get(g_mod, data, sizeof data);
    if (got >= 4 && got <= sizeof data && data[0] == 'W' && data[1] >= 1 && data[1] <= 3) {
        g_highest[0] = data[2] <= 2 ? data[2] : 2;
        g_highest[1] = data[3] <= 2 ? data[3] : 2;
        if (data[1] >= 2 && got >= 5 && data[4] <= 2) g_chosen[1] = data[4];
        if (data[1] >= 3 && got >= 6 && data[5] <= 2) g_chosen[0] = data[5];
        logf_(OPENRAC_LOG_INFO, "save data: format ", data[1], "");
        logf_(OPENRAC_LOG_INFO, "save data: normal pick ", g_chosen[0], "");
        logf_(OPENRAC_LOG_INFO, "save data: challenge pick ", g_chosen[1], "");
    } else {
        logf_(OPENRAC_LOG_INFO, "save data: none (bytes ", (int)got, ")");
    }
}

/* Planets on the ship's map (Planet_Unlock's bytes, one a level). */
#define LEVEL_DOBBO 9
#define LEVEL_GORN 15

static void unlock(int mode, int v) {
    if (v <= g_highest[mode]) return;
    g_highest[mode] = v;
    save_progress();
    if (mode) LOG("challenge mode: all three challenge wrenches available");
    else logf_(OPENRAC_LOG_INFO, "unlocked Omniwrench ", v == 1 ? 10000 : 12000, "");
}

/* What the menu offers now: Cheat Mode gives a normal game all three. */
static int available(void) {
    if (challenge()) return g_highest[1];
    return g_cheat ? 2 : g_highest[0];
}

/* What counts as unlocked, checked every tick. In challenge mode, all three of its wrenches
 * from the start. In a normal game: the wrench the game holds, and the story so far: Dobbo on
 * the map means Tabora's 10000 was found, Gorn the prison's 12000. */
static void track_unlocks(void) {
    int pos, b;
    if (challenge()) {
        unlock(1, 2);
        return;
    }
    pos = current_pos();
    b = base_pos();
    /* The wrench held counts as found, unless Cheat Mode is on or put it there. */
    if (!g_cheat && g_chosen[0] < 0 && pos >= 0 && pos - b >= 0 && pos - b <= 2) unlock(0, pos - b);
    if (rd8(A_PLANETS + LEVEL_GORN)) unlock(0, 2);
    else if (rd8(A_PLANETS + LEVEL_DOBBO)) unlock(0, 1);
}

/* --- the look -------------------------------------------------------------------------- */

/* The hero class's header (the `dest` of load_copy_header_relocate_pointers): the class
 * pointer (+0x24) of the class-0x47 moby, which the level loader relocated. */
static uint32_t find_hero_class(void) {
    uint32_t hero = rd32(A_HERO_MOBY), p;
    int i;
    if (!in_ram(hero)) return 0;
    if (rd16(hero + 0xAA) == HERO_CLASS && in_ram(rd32(hero + 0x24))) return rd32(hero + 0x24);
    for (i = 1; i < 2048; i++) { /* the moby array around the hero's, 0x100 bytes a moby */
        p = hero - (uint32_t)i * 0x100u;
        if (in_ram(p) && rd16(p + 0xAA) == HERO_CLASS && rd8(p + 0x20) < 0x80 && in_ram(rd32(p + 0x24)))
            return rd32(p + 0x24);
        p = hero + (uint32_t)i * 0x100u;
        if (in_ram(p) && rd16(p + 0xAA) == HERO_CLASS && rd8(p + 0x20) < 0x80 && in_ram(rd32(p + 0x24)))
            return rd32(p + 0x24);
    }
    return 0;
}

/* The buffer the header came from (`src`): dest+0 = src + src[4], dest+0x20 = src + src[8],
 * dest[4..7] = src[0..3]. Searched back from dest+0 until all three agree. */
static uint32_t find_model_buffer(uint32_t dest) {
    uint32_t d0 = rd32(dest), d20 = rd32(dest + 0x20), k, src;
    if (!in_ram(d0) || !in_ram(d20)) return 0;
    for (k = 0; k <= 0x200000u && k <= d0 - 0x100000u; k += 4) {
        src = d0 - k;
        if (rd32(src + 4) != k) continue;
        if (src + rd32(src + 8) != d20) continue;
        if (rd8(dest + 4) != rd8(src) || rd8(dest + 5) != rd8(src + 1) ||
            rd8(dest + 6) != rd8(src + 2) || rd8(dest + 7) != rd8(src + 3))
            continue;
        return src;
    }
    return 0;
}

static int refresh_look(int level, int v) {
    int_fn steps, loadpack, gspacket;
    uint32_t dest, src;

    /* Tabora and the prison have the game's own function for it. */
    steps = (int_fn)game_fn(ADDR_REFRESH, level);
    if (steps) {
        steps(v);
        return 1;
    }
    dest = find_hero_class();
    src = dest ? find_model_buffer(dest) : 0;
    loadpack = (int_fn)game_fn(ADDR_LOADPACK, level);
    gspacket = (int_fn)game_fn(ADDR_GSPACKET, level);
    if (!dest || !src || !loadpack || !gspacket) {
        logf_(OPENRAC_LOG_WARN, "could not refresh the wrench's look here (step ",
              !dest ? 1 : !src ? 2 : 3, "); it changes on the next level load");
        return 0;
    }
    loadpack(v);
    gspacket(v);
    /* load_copy_header_relocate_pointers(dest, src) */
    wr8(dest + 4, rd8(src + 0));
    wr8(dest + 5, rd8(src + 1));
    wr8(dest + 6, rd8(src + 2));
    wr8(dest + 7, rd8(src + 3));
    wr32(dest + 0x00, src + rd32(src + 4));
    wr32(dest + 0x20, src + rd32(src + 8));
    return 1;
}

/* --- swapping --------------------------------------------------------------------------- */

/* The wrench is version v with v's look loaded. */
static int holding(int v) {
    return current_pos() == base_pos() + v && (int32_t)rd32(A_WEAPON_CHANNEL) == v;
}

/* 1: holding version v with its look, 0: not (yet). `force` redoes it even when it looks done. */
static int apply_swap(int level, int v, int force) {
    seek_fn seek = (seek_fn)game_fn(ADDR_SEEK, level);
    int pos = base_pos() + v;
    if (!seek || pos >= g_chain_len) {
        WARN("cannot swap the wrench in this level");
        return 0;
    }
    if (!force && holding(v)) return 1;
    seek(10, pos);
    if (!refresh_look(level, v)) return 0;
    logf_(OPENRAC_LOG_INFO, "now holding the Omniwrench ", v == 0 ? 8000 : v == 1 ? 10000 : 12000, "");
    return 1;
}

/* Challenge mode (and a normal game in Cheat Mode): a level load keeps the wrench's record but loads the base wrench's model, so
 * the one picked in the menu is put back once a level, a few frames after play starts (Ratchet in
 * control): always once (the record alone can't tell), then whenever record or look stop matching
 * during a short watch. The rest of the level is left alone, so a wrench the game gives later
 * (Tabora, the prison) stays. */
static void restore_pick(int level) {
    int playing = (int32_t)rd32(A_GAME_MODE) == 0 && rd8(A_HERO_CHAR) == 0;
    int mode = challenge(), pick = g_chosen[mode];
    if (!mode && !g_cheat) {
        /* Cheat Mode turned off: a cheat pick above what was found goes back to the best found. */
        if (g_chosen[0] >= 0 && playing) {
            int pos = current_pos();
            if (g_restore_backoff > 0) {
                g_restore_backoff--;
            } else if (pos < 0 || pos <= g_highest[0] || apply_swap(level, g_highest[0], 0)) {
                g_chosen[0] = -1;
                save_progress();
                LOG("Cheat Mode off: back to the wrenches found");
            } else {
                g_restore_backoff = 60; /* couldn't swap here yet: try again in a second */
            }
        }
        return;
    }
    if (pick < 0) {
        /* No pick in the mod's data (a save written before the pick, or by an older version): the
         * game's own save still has the wrench held, so that one is kept. */
        int held = current_pos() - base_pos();
        if (held < 0 || held > 2 || held > available()) return;
        pick = g_chosen[mode] = held;
        save_progress();
        logf_(OPENRAC_LOG_INFO, "no pick saved: keeping the wrench held, version ", held, "");
    }
    if (pick > available()) return;
    if (level != g_restored_level) {
        g_restored_level = level;
        g_restore_wait = RESTORE_WAIT;
        g_restore_tries = RESTORE_TRIES;
        g_restore_backoff = 0;
        g_restore_forced = 0;
    }
    if (g_restore_tries <= 0 || !playing) return;
    if (g_restore_wait > 0) {
        g_restore_wait--;
        return;
    }
    /* Watched for the whole window, not just once, in case the game sets it a little late. */
    g_restore_tries--;
    if (g_restore_forced && holding(pick)) return;
    if (g_restore_backoff > 0) { /* after a failed try, wait before the next (keeps the log short) */
        g_restore_backoff--;
        return;
    }
    if (apply_swap(level, pick, 1)) {
        if (!g_restore_forced) LOG("put the picked wrench back after the level load");
        g_restore_forced = 1;
    } else
        g_restore_backoff = 60;
}

static void update_page(void);

static void on_frame(void* user) {
    int level = level_now();
    (void)user;
    if (level < 0 || (int32_t)rd32(A_GAME_MODE) == 6) {
        g_restored_level = -2; /* a load: the next level start (even of the same level) counts */
        return;
    }
    if (g_chain_len == 0 && level != g_last_level) {
        g_last_level = level; /* one try a level */
        probe_chain(level);
    }
    if (g_chain_len == 0) return;
    track_unlocks();
    if (g_pending >= 0 && (int32_t)rd32(A_GAME_MODE) == 0 && rd8(A_HERO_CHAR) == 0) {
        int v = g_pending;
        g_pending = -1;
        if (v <= available()) {
            int mode = challenge();
            if ((mode || g_cheat) && v != g_chosen[mode]) {
                g_chosen[mode] = v; /* remembered even if this swap fails, so the next level gets it */
                save_progress();
            }
            apply_swap(level, v, 0);
        }
    }
    restore_pick(level);
}

/* --- the menu page ----------------------------------------------------------------------- */

static int shown_version(void) {
    int v = g_pending >= 0 ? g_pending : current_pos() - base_pos();
    int hi = available();
    if (v < 0) v = 0;
    return v > hi ? hi : v;
}

static void update_page(void) {
    openrac_ui_item items[2];
    openrac_ui_content c;
    int hi = available();
    if (g_page < 0) return;
    items[0].kind = OPENRAC_UI_CHOICE;
    items[0].flags = hi == 0 ? OPENRAC_UI_DISABLED : 0;
    items[0].label = "Omniwrench";
    items[0].choices = VERSION_NAMES;
    items[0].choice_count = (uint32_t)(hi + 1);
    items[0].choice = (uint32_t)shown_version();
    items[1].kind = OPENRAC_UI_TEXT;
    items[1].flags = 0;
    items[1].label = hi == 0 ? "Find the Omniwrench 10000 first" : "Changes when you leave the menu";
    items[1].choices = 0;
    items[1].choice_count = 0;
    items[1].choice = 0;
    c.size = sizeof c;
    c.title = "Wrench Swap";
    c.items = items;
    c.item_count = 2;
    c.footer = 0;
    c.footer_count = 0;
    g_api->ui_set(g_mod, g_page, &c);
}

static void on_page(void* user, int page, const openrac_ui_event* ev) {
    (void)user;
    (void)page;
    if (ev->kind == OPENRAC_UI_SHOW) {
        update_page();
    } else if (ev->kind == OPENRAC_UI_CHANGE && ev->item == 0) {
        if (ev->value >= 0 && ev->value <= available()) g_pending = ev->value;
        update_page();
    }
}

/* --- Cheat Mode ------------------------------------------------------------------------- */

static void read_settings(void* user) {
    int was = g_cheat;
    (void)user;
    g_cheat = g_api->setting_bool(g_mod, "cheat_mode") != 0;
    if (g_cheat != was) LOG(g_cheat ? "Cheat Mode on" : "Cheat Mode off");
    update_page();
}

/* --- loading ----------------------------------------------------------------------------- */

static int same(const char* a, const char* b) {
    while (*a && *a == *b) a++, b++;
    return *a == *b;
}

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    int n;
    g_api = api;
    g_mod = mod;
    for (n = 0; n < MAX_CHAIN; n++) g_rec_of_pos[n] = -1;
    if (api->api_major != 1 || api->api_minor < 16) {
        api->fail(mod, "needs OpenRAC's mod interface 1.16 or newer");
        return 1;
    }
    if (!api->game || api->game[0] != 'r' || api->game[3] != '2' ||
        !api->version) {
        api->fail(mod, "only for Going Commando: NTSC-U v1.01, v2.00 or PAL");
        return 1;
    }
    if (same(api->version, "ntsc")) g_disc = &DISC_NTSC;
    else if (same(api->version, "pal")) g_disc = &DISC_PAL;
    else if (same(api->version, "ntsc2")) g_disc = &DISC_NTSC2;
    else {
        api->fail(mod, "only for Going Commando: NTSC-U v1.01, v2.00 or PAL");
        return 1;
    }
    LOG(g_disc == &DISC_PAL ? "PAL disc (SCES_516.07)"
        : g_disc == &DISC_NTSC2 ? "NTSC-U Greatest Hits disc (v2.00)" : "NTSC-U disc (v1.01)");
    g_page = api->ui_page(mod, 0, on_page, 0);
    if (g_page < 0 || api->ui_pause_item(mod, "Wrench Swap", g_page) != OPENRAC_HOOK_OK) {
        api->fail(mod, "could not add Wrench Swap to the pause menu (another mod may use its two places)");
        return 1;
    }
    api->on_frame(mod, on_frame, 0);
    api->on_save_data(mod, load_progress, 0);
    api->on_settings(mod, read_settings, 0);
    g_cheat = api->setting_bool(mod, "cheat_mode") != 0;
    if (g_cheat) LOG("Cheat Mode on");
    load_progress(0);
    return 0;
}

/* No C runtime: the DLL's entry point. */
/* Only for a build without the C runtime (clang -ffreestanding, lld-link /noentry /nodefaultlib);
 * the SDK's CMake build links the runtime, which has its own. */
#ifdef OPENRAC_FREESTANDING
int __stdcall _DllMainCRTStartup(void* h, unsigned reason, void* p) {
    (void)h; (void)reason; (void)p;
    return 1;
}
#endif
