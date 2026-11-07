/*
 * AFRIQUE MISSION - RPG homebrew pour PSP / PPSSPP (100% en francais)
 *
 * - Monde ouvert (96x96 cases) : Conakry, Kindia, Labe, Boke
 * - Le Chef Camara explique chaque mission, Mamadou est ton coequipier
 * - Marche d'armes : discute avec le vendeur Diallo et achete des armes
 * - Combat, niveaux, argent (GNF), 3 missions
 *
 * Controles :
 *   Croix directionnelle / stick : se deplacer
 *   X        : parler / continuer un dialogue / acheter
 *   Carre    : attaquer
 *   Rond     : quitter le marche
 *   HOME     : quitter le jeu
 *
 * Note : la police integree de la PSP n'a pas les accents,
 * le texte est donc ecrit sans accents.
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("AfriqueMission", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

/* u32 est deja defini par le PSPSDK (psptypes.h) */

#define SCR_W 480
#define SCR_H 272
#define BUF_W 512
#define VRAM ((u32 *)0x44000000)
#define RGB(r, g, b) (0xFF000000 | ((b) << 16) | ((g) << 8) | (r))

#define WW 96
#define WH 96
#define TILE 16
#define MAX_E 10

/* ====================== Quitter avec HOME ====================== */

static int exit_callback(int a1, int a2, void *c)
{
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback("exit_cb", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int thid = sceKernelCreateThread("cb_thread", callback_thread, 0x11, 0xFA0, 0, NULL);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
}

/* ====================== Dessin ====================== */

static void fill_rect(int x, int y, int w, int h, u32 color)
{
    int i, j;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            VRAM[(y + j) * BUF_W + x + i] = color;
}

static void text(int col, int row, u32 fg, u32 bg, const char *s)
{
    pspDebugScreenSetXY(col, row);
    pspDebugScreenSetTextColor(fg);
    pspDebugScreenSetBackColor(bg);
    pspDebugScreenPrintf("%s", s);
}

static void text_bar(int row, u32 fg, u32 bg, const char *s)
{
    char buf[80];
    snprintf(buf, sizeof(buf), "%-59.59s", s);
    text(0, row, fg, bg, buf);
}

/* ====================== Carte du monde ====================== */

/* 0 herbe, 1 arbre, 2 eau, 3 chemin/pont, 4 batiment, 5 sol de ville */
static unsigned char map[WH][WW];

typedef struct { const char *name; int x0, y0, x1, y1; } Zone;

static const Zone zones[4] = {
    { "Conakry", 8, 8, 28, 24 },
    { "Kindia", 60, 10, 74, 22 },
    { "Labe", 18, 64, 34, 78 },
    { "Boke", 68, 68, 84, 82 }
};

static int blocked_tile(int tx, int ty)
{
    unsigned char t;
    if (tx < 0 || ty < 0 || tx >= WW || ty >= WH) return 1;
    t = map[ty][tx];
    return t == 1 || t == 2 || t == 4;
}

static int blocked_px(int x, int y) { return blocked_tile(x >> 4, y >> 4); }

static int can_stand(int x, int y, int h)
{
    return !blocked_px(x - h, y - h) && !blocked_px(x + h, y - h) &&
           !blocked_px(x - h, y + h) && !blocked_px(x + h, y + h);
}

static void hline(int x0, int x1, int y)
{
    int x;
    for (x = x0; x <= x1; x++) { map[y][x] = 3; map[y + 1][x] = 3; }
}

static void vline(int x, int y0, int y1)
{
    int y;
    for (y = y0; y <= y1; y++) { map[y][x] = 3; map[y][x + 1] = 3; }
}

static void building(int x, int y)
{
    int i, j;
    for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++)
            map[y + j][x + i] = 4;
}

static void gen_map(void)
{
    int x, y, z;

    srand(2024);
    for (y = 0; y < WH; y++)
        for (x = 0; x < WW; x++) {
            map[y][x] = (rand() % 100 < 13) ? 1 : 0;
            if (x == 0 || y == 0 || x == WW - 1 || y == WH - 1) map[y][x] = 2;
        }

    /* Riviere qui coupe la carte, avec deux ponts */
    for (y = 1; y < WH - 1; y++) {
        int rx = 44 + ((y / 6) % 3);
        for (x = rx; x < rx + 3; x++) map[y][x] = 2;
    }
    for (y = 14; y <= 17; y++) for (x = 40; x <= 52; x++) map[y][x] = 3;
    for (y = 70; y <= 73; y++) for (x = 40; x <= 52; x++) map[y][x] = 3;

    /* Villes et villages : terrain degage */
    for (z = 0; z < 4; z++)
        for (y = zones[z].y0; y <= zones[z].y1; y++)
            for (x = zones[z].x0; x <= zones[z].x1; x++)
                map[y][x] = (z == 0) ? 5 : 0;

    /* Routes */
    hline(18, 66, 15);   /* Conakry -> Kindia */
    vline(24, 16, 70);   /* Conakry -> Labe   */
    hline(24, 76, 72);   /* Labe -> Boke      */

    /* Batiments */
    for (z = 0; z < 4; z++) {
        const Zone *q = &zones[z];
        building(q->x0 + 1, q->y0 + 1);
        building(q->x1 - 3, q->y0 + 1);
        building(q->x0 + 1, q->y1 - 3);
        building(q->x1 - 3, q->y1 - 3);
    }
}

static void draw_tile(int t, int tx, int ty, int sx, int sy)
{
    switch (t) {
    case 0:
    case 1: {
        int v = (tx * 7 + ty * 13) % 3;
        fill_rect(sx, sy, 16, 16, v == 0 ? RGB(52, 150, 60) : v == 1 ? RGB(46, 140, 54) : RGB(58, 158, 66));
        if (t == 1) {
            fill_rect(sx + 6, sy + 9, 4, 7, RGB(100, 65, 30));
            fill_rect(sx + 1, sy, 14, 11, RGB(20, 100, 35));
            fill_rect(sx + 4, sy + 2, 4, 3, RGB(40, 130, 50));
        }
        break;
    }
    case 2:
        fill_rect(sx, sy, 16, 16, RGB(40, 100, 200));
        if ((tx + ty) & 1) fill_rect(sx + 4, sy + 6, 8, 2, RGB(110, 170, 240));
        break;
    case 3:
        fill_rect(sx, sy, 16, 16, RGB(205, 175, 115));
        break;
    case 4:
        fill_rect(sx, sy, 16, 16, RGB(160, 105, 70));
        fill_rect(sx, sy, 16, 4, RGB(185, 50, 40));
        fill_rect(sx + 6, sy + 8, 4, 8, RGB(70, 40, 20));
        break;
    default:
        fill_rect(sx, sy, 16, 16, RGB(150, 150, 156));
        fill_rect(sx, sy, 16, 1, RGB(125, 125, 132));
        fill_rect(sx, sy, 1, 16, RGB(125, 125, 132));
        break;
    }
}

/* ====================== Armes ====================== */

typedef struct { const char *name; int price, dmg, range; } Weapon;

static const Weapon weapons[4] = {
    { "Baton",    0,    6,  30 },
    { "Machette", 300,  14, 34 },
    { "Pistolet", 900,  26, 100 },
    { "Fusil",    2500, 44, 140 }
};

/* ====================== Etat du jeu ====================== */

typedef struct { int x, y, hp, maxhp, alive, cd, dmg, m; } Enemy;
typedef struct { int x, y; u32 body; const char *name; } Npc;

enum { NPC_CHIEF, NPC_VENDOR, NPC_VILL1, NPC_VILL2, NPC_VILL3, NPC_N };
enum { MODE_PLAY, MODE_DIALOG, MODE_SHOP };
enum { ACT_NONE, ACT_ACCEPT, ACT_TURNIN, ACT_FINISH, ACT_SHOP };

static Enemy enemies[MAX_E];
static Npc npcs[NPC_N];

static int px, py, mx, my;
static int hp, maxhp, level, xp, gnf, weapon;
static int owned[4];
static int state, arrived[3];
static int atk_cd, mate_cd, hurt_t, msg_t, flash_t;
static int fl_ax, fl_ay, fl_bx, fl_by;
static char msg[64];
static int mode;
static int shop_sel;
static char shop_msg[64];

/* ====================== Dialogues ====================== */

typedef struct { const char *who, *l1, *l2; } Line;

static const Line *dlg;
static int dlg_n, dlg_i, dlg_action, dlg_arg;
static Line tmp[1];

static const Line d_brief0[] = {
    { "Chef Camara", "Ah, te voila enfin! La situation est grave.", "Des bandits attaquent le village de Kindia." },
    { "Chef Camara", "Tu pars avec Mamadou, ton coequipier.", "Elimine 4 bandits puis reviens me voir." },
    { "Mamadou", "Compris chef! Suis la route vers l'est,", "Kindia n'est pas tres loin de Conakry." },
    { "Chef Camara", "Passe d'abord au marche, sur la place.", "Le vendeur Diallo a de bonnes armes. Courage!" }
};
static const Line d_wait[] = {
    { "Chef Camara", "Qu'est-ce que tu fais encore ici?", "La mission t'attend, ne perds pas de temps!" }
};
static const Line d_turnin0[] = {
    { "Chef Camara", "Excellent travail! Kindia est sauve.", "Tiens, 500 GNF et mes remerciements." },
    { "Chef Camara", "Nouvelle mission: des voleurs sement la", "terreur a Labe, au sud de Conakry." },
    { "Mamadou", "On devra en eliminer 6 la-bas, chef?", "Pas de souci, on est prets. En route!" }
};
static const Line d_turnin1[] = {
    { "Chef Camara", "Labe respire enfin grace a vous deux!", "Voici 1200 GNF. Tu l'as bien merite." },
    { "Chef Camara", "Derniere mission: des rebelles occupent Boke,", "au sud-est. Elimine 8 hommes la-bas." },
    { "Mamadou", "Boke... c'est loin, mais on y va ensemble.", "Prends de bonnes armes au marche d'abord!" }
};
static const Line d_end[] = {
    { "Chef Camara", "Incroyable! Tu as libere Boke. La Guinee", "te doit beaucoup, mon ami. Voici 2500 GNF." },
    { "Mamadou", "Quelle aventure, chef! On a fait du bon boulot.", "Merci de m'avoir fait confiance." },
    { "Chef Camara", "Tu es un vrai heros. Repose-toi, tu l'as merite!", "FIN - merci d'avoir joue a Afrique Mission!" }
};
static const Line d_after[] = {
    { "Chef Camara", "Repose-toi, heros. Conakry est en paix.", "Reviens quand il y aura une nouvelle mission." }
};
static const Line d_vendor[] = {
    { "Diallo (vendeur)", "Bonjour mon ami! Bienvenue au marche d'armes!", "Tout ce que tu vois est de premiere qualite." },
    { "Diallo (vendeur)", "Machette 300 GNF, pistolet 900 GNF, fusil 2500.", "Pour toi, c'est le meilleur prix de Conakry!" }
};
static const Line d_arrive[3] = {
    { "Mamadou", "Voila Kindia! Je vois des bandits partout.", "Attention, on se bat ensemble! (Carre: attaquer)" },
    { "Mamadou", "Nous sommes a Labe. Ces voleurs ont l'air armes.", "Reste pres de moi et frappe fort!" },
    { "Mamadou", "Boke... c'est le camp des rebelles. Ils sont", "nombreux. Utilise ta meilleure arme!" }
};
static const Line d_done[] = {
    { "Mamadou", "Mission accomplie! Retournons voir le chef", "a Conakry pour avoir notre recompense." }
};
static const Line d_faint[] = {
    { "Mamadou", "Tu as perdu connaissance! Je t'ai ramene a", "Conakry. Ils ont pris un peu de ton argent." }
};

static void start_dialog(const Line *l, int n, int act, int arg)
{
    dlg = l; dlg_n = n; dlg_i = 0; dlg_action = act; dlg_arg = arg;
    mode = MODE_DIALOG;
}

static void set_msg(const char *s)
{
    snprintf(msg, sizeof(msg), "%s", s);
    msg_t = 180;
}

static void gain_xp(int n)
{
    xp += n;
    while (xp >= level * 50) {
        xp -= level * 50;
        level++;
        maxhp += 15;
        hp = maxhp;
        set_msg("NIVEAU SUPERIEUR! PV restaures.");
    }
}

static void spawn_enemies(int m)
{
    const Zone *z = &zones[m + 1];
    int n = (m == 0) ? 4 : (m == 1) ? 6 : 8;
    int i;

    memset(enemies, 0, sizeof(enemies));
    for (i = 0; i < n; i++) {
        int tries = 0, tx, ty;
        do {
            tx = z->x0 + rand() % (z->x1 - z->x0 + 1);
            ty = z->y0 + rand() % (z->y1 - z->y0 + 1);
            tries++;
        } while (!can_stand(tx * TILE + 8, ty * TILE + 8, 5) && tries < 100);
        enemies[i].x = tx * TILE + 8;
        enemies[i].y = ty * TILE + 8;
        enemies[i].maxhp = 30 + m * 20;
        enemies[i].hp = enemies[i].maxhp;
        enemies[i].dmg = 5 + m * 3;
        enemies[i].m = m;
        enemies[i].alive = 1;
    }
}

static void give_reward(int m)
{
    static const int g[3] = { 500, 1200, 2500 };
    static const int x[3] = { 40, 70, 120 };
    char b[64];
    gnf += g[m];
    snprintf(b, sizeof(b), "Recompense: +%d GNF", g[m]);
    set_msg(b);
    gain_xp(x[m]);
}

static void dialog_end(void)
{
    mode = MODE_PLAY;
    switch (dlg_action) {
    case ACT_ACCEPT:
        state = 1 + 2 * dlg_arg;
        spawn_enemies(dlg_arg);
        break;
    case ACT_TURNIN:
        give_reward(dlg_arg);
        state = 1 + 2 * (dlg_arg + 1);
        spawn_enemies(dlg_arg + 1);
        break;
    case ACT_FINISH:
        give_reward(2);
        state = 7;
        break;
    case ACT_SHOP:
        mode = MODE_SHOP;
        shop_sel = 1;
        snprintf(shop_msg, sizeof(shop_msg), "Que veux-tu acheter?");
        break;
    }
}

static void talk_npc(int i)
{
    if (i == NPC_CHIEF) {
        if (state == 0)      start_dialog(d_brief0, 4, ACT_ACCEPT, 0);
        else if (state == 2) start_dialog(d_turnin0, 3, ACT_TURNIN, 0);
        else if (state == 4) start_dialog(d_turnin1, 3, ACT_TURNIN, 1);
        else if (state == 6) start_dialog(d_end, 3, ACT_FINISH, 0);
        else if (state == 7) start_dialog(d_after, 1, ACT_NONE, 0);
        else                 start_dialog(d_wait, 1, ACT_NONE, 0);
    } else if (i == NPC_VENDOR) {
        start_dialog(d_vendor, 2, ACT_SHOP, 0);
    } else {
        int m = i - NPC_VILL1;
        if (state >= 2 + 2 * m) {
            tmp[0].who = "Villageois";
            tmp[0].l1 = "Merci! Notre village est de nouveau en paix.";
            tmp[0].l2 = "Que Dieu vous protege, toi et Mamadou!";
        } else {
            tmp[0].who = "Villageois";
            tmp[0].l1 = "Au secours! Des hommes armes sont partout";
            tmp[0].l2 = "dans le village! Aidez-nous, soldat!";
        }
        start_dialog(tmp, 1, ACT_NONE, 0);
    }
}

static void talk_mate(void)
{
    tmp[0].who = "Mamadou";
    if (state == 0) {
        tmp[0].l1 = "Va voir le chef, au centre de Conakry.";
        tmp[0].l2 = "Il t'expliquera la mission.";
    } else if (state == 7) {
        tmp[0].l1 = "Belle aventure! On forme une bonne equipe.";
        tmp[0].l2 = "Merci de m'avoir fait confiance!";
    } else if (state & 1) {
        tmp[0].l1 = "Suis le cap indique en haut de l'ecran.";
        tmp[0].l2 = "Je te couvre, mais fais attention!";
    } else {
        tmp[0].l1 = "Retourne voir le chef pour la recompense.";
        tmp[0].l2 = "Il est au centre de Conakry.";
    }
    start_dialog(tmp, 1, ACT_NONE, 0);
}

/* ====================== Logique ====================== */

static int in_zone(int z)
{
    int tx = px >> 4, ty = py >> 4;
    return tx >= zones[z].x0 && tx <= zones[z].x1 && ty >= zones[z].y0 && ty <= zones[z].y1;
}

static int alive_count(void)
{
    int i, n = 0;
    for (i = 0; i < MAX_E; i++) if (enemies[i].alive) n++;
    return n;
}

static int find_npc(void)
{
    int i;
    for (i = 0; i < NPC_N; i++) {
        int dx = npcs[i].x - px, dy = npcs[i].y - py;
        if (dx * dx + dy * dy < 28 * 28) return i;
    }
    return -1;
}

static Enemy *nearest_enemy(int x, int y, int range)
{
    Enemy *best = NULL;
    int bd = range * range + 1, i;
    for (i = 0; i < MAX_E; i++) {
        int dx, dy, d2;
        if (!enemies[i].alive) continue;
        dx = enemies[i].x - x; dy = enemies[i].y - y;
        d2 = dx * dx + dy * dy;
        if (d2 < bd) { bd = d2; best = &enemies[i]; }
    }
    return best;
}

static void damage_enemy(Enemy *e, int d)
{
    e->hp -= d;
    if (e->hp <= 0) {
        char b[64];
        int g = 20 + e->m * 15;
        e->alive = 0;
        gnf += g;
        snprintf(b, sizeof(b), "Ennemi elimine! +%d GNF", g);
        set_msg(b);
        gain_xp(12 + e->m * 6);
    }
}

static void attack(void)
{
    Enemy *e = nearest_enemy(px, py, weapons[weapon].range);
    atk_cd = (weapon >= 2) ? 25 : 16;
    if (e) {
        fl_ax = px; fl_ay = py; fl_bx = e->x; fl_by = e->y;
        flash_t = 5;
        damage_enemy(e, weapons[weapon].dmg + level * 2);
    }
}

static void respawn_player(void)
{
    px = 18 * TILE + 8; py = 16 * TILE + 8;
    mx = px - 20; my = py;
    hp = maxhp;
    gnf -= gnf / 5;
    start_dialog(d_faint, 1, ACT_NONE, 0);
}

static void update_play(SceCtrlData *pad, u32 pressed)
{
    int dx = 0, dy = 0, sp = 2, i, m;

    if (pad->Buttons & PSP_CTRL_LEFT)  dx = -sp;
    if (pad->Buttons & PSP_CTRL_RIGHT) dx = sp;
    if (pad->Buttons & PSP_CTRL_UP)    dy = -sp;
    if (pad->Buttons & PSP_CTRL_DOWN)  dy = sp;
    if (pad->Lx < 70)  dx = -sp;
    if (pad->Lx > 186) dx = sp;
    if (pad->Ly < 70)  dy = -sp;
    if (pad->Ly > 186) dy = sp;

    if (dx && can_stand(px + dx, py, 5)) px += dx;
    if (dy && can_stand(px, py + dy, 5)) py += dy;

    /* Coequipier : suit le joueur */
    {
        int ex = px - mx, ey = py - my;
        if (ex * ex + ey * ey > 26 * 26) {
            mx += ((ex > 0) - (ex < 0)) * 2;
            my += ((ey > 0) - (ey < 0)) * 2;
        }
    }
    if (mate_cd > 0) mate_cd--;
    if (mate_cd == 0) {
        Enemy *e = nearest_enemy(mx, my, 55);
        if (e) {
            fl_ax = mx; fl_ay = my; fl_bx = e->x; fl_by = e->y;
            flash_t = 5;
            mate_cd = 50;
            damage_enemy(e, 8 + level);
        }
    }

    if (atk_cd > 0) atk_cd--;
    if (hurt_t > 0) hurt_t--;
    if (msg_t > 0) msg_t--;
    if (flash_t > 0) flash_t--;

    /* Parler */
    if (pressed & PSP_CTRL_CROSS) {
        int n = find_npc();
        if (n >= 0) {
            talk_npc(n);
            return;
        } else {
            int ex = mx - px, ey = my - py;
            if (ex * ex + ey * ey < 40 * 40) {
                talk_mate();
                return;
            }
        }
    }

    /* Attaquer */
    if ((pressed & PSP_CTRL_SQUARE) && atk_cd <= 0)
        attack();

    /* Ennemis */
    for (i = 0; i < MAX_E; i++) {
        Enemy *e = &enemies[i];
        int ex, ey, d2;
        if (!e->alive) continue;
        ex = px - e->x; ey = py - e->y;
        d2 = ex * ex + ey * ey;
        if (e->cd > 0) e->cd--;
        if (d2 < 120 * 120 && d2 > 12 * 12) {
            int sx = (ex > 0) - (ex < 0), sy = (ey > 0) - (ey < 0);
            if (can_stand(e->x + sx, e->y, 5)) e->x += sx;
            if (can_stand(e->x, e->y + sy, 5)) e->y += sy;
        }
        if (d2 < 15 * 15 && e->cd == 0) {
            hp -= e->dmg;
            e->cd = 40;
            hurt_t = 8;
        }
    }

    if (hp <= 0) {
        respawn_player();
        return;
    }

    /* Evenements de mission */
    if (state > 0 && state < 7 && (state & 1)) {
        m = (state - 1) / 2;
        if (!arrived[m] && in_zone(m + 1)) {
            arrived[m] = 1;
            start_dialog(&d_arrive[m], 1, ACT_NONE, 0);
            return;
        }
        if (alive_count() == 0) {
            state++;
            start_dialog(d_done, 1, ACT_NONE, 0);
            return;
        }
    }
}

static void update_dialog(u32 pressed)
{
    if (pressed & PSP_CTRL_CROSS) {
        dlg_i++;
        if (dlg_i >= dlg_n) dialog_end();
    }
}

static void update_shop(u32 pressed)
{
    if (pressed & PSP_CTRL_UP)   shop_sel = (shop_sel + 3) % 4;
    if (pressed & PSP_CTRL_DOWN) shop_sel = (shop_sel + 1) % 4;
    if (pressed & PSP_CTRL_CIRCLE) mode = MODE_PLAY;
    if (pressed & PSP_CTRL_CROSS) {
        if (owned[shop_sel]) {
            weapon = shop_sel;
            snprintf(shop_msg, sizeof(shop_msg), "%s equipe. Bonne chasse!", weapons[shop_sel].name);
        } else if (gnf >= weapons[shop_sel].price) {
            gnf -= weapons[shop_sel].price;
            owned[shop_sel] = 1;
            weapon = shop_sel;
            snprintf(shop_msg, sizeof(shop_msg), "Merci! Belle affaire, ami!");
        } else {
            snprintf(shop_msg, sizeof(shop_msg), "Pas assez de GNF. Reviens plus tard!");
        }
    }
}

/* ====================== Affichage ====================== */

static void draw_person(int wx, int wy, int camx, int camy, u32 body, u32 skin)
{
    int sx = wx - camx, sy = wy - camy;
    fill_rect(sx - 5, sy - 6, 10, 12, body);
    fill_rect(sx - 4, sy - 12, 8, 6, skin);
}

static void draw_line_fx(int ax, int ay, int bx, int by, int camx, int camy)
{
    int i;
    for (i = 0; i <= 8; i++) {
        int x = ax + (bx - ax) * i / 8 - camx;
        int y = ay + (by - ay) * i / 8 - camy;
        fill_rect(x - 1, y - 1, 3, 3, RGB(255, 235, 80));
    }
}

static void objective(char *out, size_t n)
{
    int m = (state - 1) / 2;
    if (state == 0)
        snprintf(out, n, "Objectif: parle au Chef Camara (Conakry)");
    else if (state == 7)
        snprintf(out, n, "Mission terminee! Tu es un heros!");
    else if (state & 1)
        snprintf(out, n, "Objectif: elimine les ennemis a %s (%d)", zones[m + 1].name, alive_count());
    else
        snprintf(out, n, "Objectif: retourne voir le Chef Camara");
}

static void compass(char *out, size_t n)
{
    int tx, ty, dx, dy;
    const char *ns, *eo;
    if (state == 7) { out[0] = 0; return; }
    if (state & 1) {
        int m = (state - 1) / 2;
        tx = (zones[m + 1].x0 + zones[m + 1].x1) / 2;
        ty = (zones[m + 1].y0 + zones[m + 1].y1) / 2;
    } else {
        tx = 16; ty = 14;
    }
    dx = tx - (px >> 4); dy = ty - (py >> 4);
    ns = dy < -3 ? "Nord " : dy > 3 ? "Sud " : "";
    eo = dx < -3 ? "Ouest" : dx > 3 ? "Est" : "";
    snprintf(out, n, "Cap: %s%s (%d m)", ns, eo, (abs(dx) + abs(dy)) * 4);
}

static void render_world(void)
{
    int camx = px - SCR_W / 2, camy = py - SCR_H / 2;
    int tx0, ty0, tx, ty, i;

    if (camx < 0) camx = 0;
    if (camy < 0) camy = 0;
    if (camx > WW * TILE - SCR_W) camx = WW * TILE - SCR_W;
    if (camy > WH * TILE - SCR_H) camy = WH * TILE - SCR_H;

    tx0 = camx >> 4; ty0 = camy >> 4;
    for (ty = ty0; ty <= ty0 + 17 && ty < WH; ty++)
        for (tx = tx0; tx <= tx0 + 30 && tx < WW; tx++)
            draw_tile(map[ty][tx], tx, ty, tx * TILE - camx, ty * TILE - camy);

    for (i = 0; i < NPC_N; i++)
        draw_person(npcs[i].x, npcs[i].y, camx, camy, npcs[i].body, RGB(120, 80, 50));

    for (i = 0; i < MAX_E; i++) {
        Enemy *e = &enemies[i];
        int sx, sy;
        if (!e->alive) continue;
        draw_person(e->x, e->y, camx, camy, RGB(200, 30, 30), RGB(90, 55, 35));
        sx = e->x - camx; sy = e->y - camy;
        fill_rect(sx - 8, sy - 17, 16, 2, RGB(60, 0, 0));
        fill_rect(sx - 8, sy - 17, 16 * e->hp / e->maxhp, 2, RGB(60, 230, 60));
    }

    draw_person(mx, my, camx, camy, RGB(50, 110, 230), RGB(110, 70, 45));
    draw_person(px, py, camx, camy, hurt_t > 0 ? RGB(255, 255, 255) : RGB(40, 200, 90), RGB(130, 85, 55));

    if (flash_t > 0)
        draw_line_fx(fl_ax, fl_ay, fl_bx, fl_by, camx, camy);

    /* Mini-carte */
    {
        int x, y;
        fill_rect(426, 44, 52, 52, RGB(0, 0, 0));
        for (y = 0; y < 48; y++)
            for (x = 0; x < 48; x++) {
                static const u32 col[6] = {
                    RGB(52, 150, 60), RGB(20, 100, 35), RGB(40, 100, 200),
                    RGB(205, 175, 115), RGB(160, 105, 70), RGB(150, 150, 156)
                };
                VRAM[(46 + y) * BUF_W + 428 + x] = col[map[y * 2][x * 2]];
            }
        fill_rect(428 + (px >> 5) - 1, 46 + (py >> 5) - 1, 3, 3, RGB(255, 255, 0));
    }

    /* Barre d'info */
    {
        char b[96], obj[96], cap[48];
        const char *lieu = "Pleine nature";
        int z;
        for (z = 0; z < 4; z++) if (in_zone(z)) lieu = zones[z].name;

        fill_rect(0, 0, SCR_W, 40, RGB(0, 0, 0));
        snprintf(b, sizeof(b), "Niv %d  PV %d/%d  GNF %d  XP %d/%d", level, hp, maxhp, gnf, xp, level * 50);
        text_bar(0, RGB(255, 255, 255), RGB(0, 0, 0), b);
        snprintf(b, sizeof(b), "Arme: %s   Lieu: %s", weapons[weapon].name, lieu);
        text_bar(1, RGB(255, 215, 0), RGB(0, 0, 0), b);
        objective(obj, sizeof(obj));
        text_bar(2, RGB(120, 220, 255), RGB(0, 0, 0), obj);
        compass(cap, sizeof(cap));
        snprintf(b, sizeof(b), "%s", cap);
        text_bar(3, RGB(160, 255, 160), RGB(0, 0, 0), b);

        if (mode == MODE_PLAY) {
            int n = find_npc();
            if (msg_t > 0) {
                fill_rect(0, 40, 300, 8, RGB(0, 0, 0));
                text(0, 5, RGB(255, 255, 120), RGB(0, 0, 0), msg);
            } else if (n >= 0) {
                static const char *names[NPC_N] = { "Chef Camara", "Diallo (vendeur)", "le villageois", "la villageoise", "le villageois" };
                snprintf(b, sizeof(b), "X: parler a %s", names[n]);
                fill_rect(0, 40, 300, 8, RGB(0, 0, 0));
                text(0, 5, RGB(255, 255, 255), RGB(0, 0, 0), b);
            }
        }
    }
}

static void render_dialog(void)
{
    const Line *l = &dlg[dlg_i];
    u32 bg = RGB(20, 30, 80);
    fill_rect(6, 194, 468, 74, RGB(200, 200, 255));
    fill_rect(8, 196, 464, 70, bg);
    text(2, 25, RGB(255, 215, 0), bg, l->who);
    text(2, 27, RGB(255, 255, 255), bg, l->l1);
    text(2, 28, RGB(255, 255, 255), bg, l->l2);
    text(44, 31, RGB(150, 200, 255), bg, "X: suite");
}

static void render_shop(void)
{
    u32 bg = RGB(40, 25, 10);
    char b[64];
    int i;

    fill_rect(40, 30, 400, 200, RGB(220, 180, 100));
    fill_rect(43, 33, 394, 194, bg);
    text(6, 5, RGB(255, 215, 0), bg, "MARCHE D'ARMES - Diallo, vendeur");
    for (i = 0; i < 4; i++) {
        snprintf(b, sizeof(b), "%c %-9s %5d GNF  Degats %-3d %s",
                 i == shop_sel ? '>' : ' ', weapons[i].name, weapons[i].price,
                 weapons[i].dmg, i == weapon ? "[equipee]" : owned[i] ? "[possedee]" : "");
        text(6, 8 + i * 2, i == shop_sel ? RGB(255, 255, 255) : RGB(190, 160, 120), bg, b);
    }
    snprintf(b, sizeof(b), "Tu as: %d GNF          ", gnf);
    text(6, 18, RGB(120, 255, 120), bg, b);
    snprintf(b, sizeof(b), "%-46s", shop_msg);
    text(6, 20, RGB(255, 255, 255), bg, b);
    text(6, 24, RGB(150, 200, 255), bg, "Haut/Bas: choisir  X: acheter  O: partir");
}

/* ====================== Initialisation ====================== */

static void init_game(void)
{
    px = 18 * TILE + 8; py = 16 * TILE + 8;
    mx = px - 20; my = py;
    maxhp = 100; hp = maxhp; level = 1; xp = 0; gnf = 100;
    weapon = 0;
    memset(owned, 0, sizeof(owned));
    owned[0] = 1;
    state = 0;
    memset(arrived, 0, sizeof(arrived));
    memset(enemies, 0, sizeof(enemies));
    mode = MODE_PLAY;

    npcs[NPC_CHIEF].x = 16 * TILE + 8;  npcs[NPC_CHIEF].y = 14 * TILE + 8;  npcs[NPC_CHIEF].body = RGB(240, 200, 40);
    npcs[NPC_VENDOR].x = 21 * TILE + 8; npcs[NPC_VENDOR].y = 16 * TILE + 8; npcs[NPC_VENDOR].body = RGB(170, 60, 200);
    npcs[NPC_VILL1].x = 67 * TILE + 8;  npcs[NPC_VILL1].y = 16 * TILE + 8;  npcs[NPC_VILL1].body = RGB(230, 140, 40);
    npcs[NPC_VILL2].x = 26 * TILE + 8;  npcs[NPC_VILL2].y = 71 * TILE + 8;  npcs[NPC_VILL2].body = RGB(230, 100, 160);
    npcs[NPC_VILL3].x = 76 * TILE + 8;  npcs[NPC_VILL3].y = 75 * TILE + 8;  npcs[NPC_VILL3].body = RGB(150, 200, 230);
}

static void title_screen(SceCtrlData *pad)
{
    u32 bg = RGB(10, 25, 15);
    u32 prev = 0, pressed;
    for (;;) {
        sceCtrlPeekBufferPositive(pad, 1);
        pressed = pad->Buttons & ~prev;
        prev = pad->Buttons;
        sceDisplayWaitVblankStart();
        fill_rect(0, 0, SCR_W, SCR_H, bg);
        fill_rect(0, 90, SCR_W, 6, RGB(206, 17, 38));
        fill_rect(0, 96, SCR_W, 6, RGB(252, 209, 22));
        fill_rect(0, 102, SCR_W, 6, RGB(0, 148, 96));
        text(18, 8, RGB(255, 215, 0), bg, "A F R I Q U E   M I S S I O N");
        text(14, 14, RGB(255, 255, 255), bg, "Un jeu d'aventure en monde ouvert - en francais");
        text(10, 20, RGB(190, 220, 190), bg, "Parle au Chef, pars en mission, achete des armes!");
        text(20, 25, RGB(150, 200, 255), bg, "Appuie sur X pour jouer");
        text(8, 31, RGB(120, 140, 120), bg, "Carre: attaquer  X: parler  Rond: quitter le marche");
        if (pressed & PSP_CTRL_CROSS) return;
    }
}

int main(void)
{
    SceCtrlData pad;
    u32 prev = 0, pressed;

    setup_callbacks();
    pspDebugScreenInit();
    pspDebugScreenEnableBackColor(1);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    gen_map();
    srand(sceKernelGetSystemTimeLow());
    init_game();
    title_screen(&pad);
    prev = pad.Buttons;

    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev;
        prev = pad.Buttons;

        sceDisplayWaitVblankStart();

        if (mode == MODE_PLAY)        update_play(&pad, pressed);
        else if (mode == MODE_DIALOG) update_dialog(pressed);
        else                          update_shop(pressed);

        render_world();
        if (mode == MODE_DIALOG)    render_dialog();
        else if (mode == MODE_SHOP) render_shop();
    }

    return 0;
}
