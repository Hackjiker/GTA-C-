// =====================================================================
//  GTA-RUB 4 — мини-GTA на чистом C++ (WinAPI + OpenGL), без библиотек.
//  Люди с руками, полиция и звёзды розыска, оружие (снайперка, РПГ),
//  магазины, окна, вертолёт с лестницей, освещение и тени.
//  Рубли: за убийства и за ограбление банка.
//  v3: хитбоксы (деревья, фонари, машины, вертолёт), прыжки на капот и крышу,
//      полицейские машины, спецназ, модели получше.
//  v4: оружие можно найти, банк, лестницы на крыши, лавочки и стулья, падение людей,
//      спицы в колёсах, лестница вертолёта убирается, NPC обходят стены.
// =====================================================================
#include <windows.h>
#include <GL/gl.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

static const float PI_F  = 3.14159265f;
static const float WORLD = 125.0f;            // половина размера карты, метры

struct Vec3 { float x, y, z; };
struct Col  { float r, g, b, a; };
static Col Rgba(int r, int g, int b, int a = 255) { return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f }; }
static Col Darker(Col c, float k = 0.7f) { return { c.r * k, c.g * k, c.b * k, c.a }; }
static const Col C_WHITE = Rgba(255, 255, 255), C_BLACK = Rgba(0, 0, 0), C_RED = Rgba(230, 41, 55),
                 C_YELLOW = Rgba(253, 249, 0), C_GOLD = Rgba(255, 203, 0), C_SKY = Rgba(102, 191, 255),
                 C_GREEN = Rgba(0, 228, 48), C_LIME = Rgba(0, 200, 60), C_GRAY = Rgba(130, 130, 130),
                 C_DGRAY = Rgba(60, 60, 66), C_SKIN = Rgba(232, 190, 150), C_BLUE = Rgba(40, 100, 230),
                 C_BROWN = Rgba(110, 70, 40), C_GLASS = Rgba(120, 175, 215);

// ---------------------------- данные ---------------------------------
struct Building { Vec3 pos; Vec3 size; Col color; Col accent; int kind; int facing; std::wstring name; Vec3 door; };
// kind: 0 обычное, 1 оружейный магазин, 2 аптека, 3 полиция, 4 кафе, 5 банк
struct Ruble  { Vec3 pos; int value; float life; };
struct Person { Vec3 pos; float yaw, hp; bool alive; float timer, wander, shootT, phase, amp; int type; Col shirt, pants, hair; };
// type: 0 прохожий, 1 бандит, 2 полицейский, 3 спецназ
struct Proj   { Vec3 pos, vel; float life, dmg; bool enemy, rocket; };
struct Boom   { Vec3 pos; float t; };
struct Car    { Vec3 pos; float yaw, speed, hp, wheel; };
struct PCar   { Vec3 pos; float yaw, speed, hp, siren, hitT, stuck, rev, revDir, wheel; bool swat, unloaded; };   // полицейская машина / фургон спецназа
struct Solid  { Vec3 pos; Vec3 size; };   // невидимый хитбокс (дерево, фонарь, лавочка): pos — центр, size.y — высота
struct WPick  { Vec3 pos; int w; };       // оружие, лежащее на земле
struct Seat   { Vec3 pos; int kind, dir; };   // kind: 0 лавочка, 1 стул
struct Heli   { Vec3 pos; float yaw, vy, speed, rotor; };
struct Sign   { Vec3 pos; std::wstring text; float w; };
struct Weapon { const wchar_t* name; float dmg, rate; int pellets; float spread, speed, life; int price; bool automatic; float muzzle; };

static const Weapon WEAPONS[5] = {
    { L"Пистолет",  34,  0.28f,  1, 0.004f, 150, 1.5f,    0, false, 0.85f },
    { L"Автомат",   22,  0.085f, 1, 0.018f, 160, 1.5f,  400, true,  1.40f },
    { L"Дробовик",  16,  0.85f,  9, 0.090f, 120, 0.45f, 600, false, 1.40f },
    { L"Снайперка", 170, 1.5f,   1, 0.0f,   330, 1.2f, 1000, false, 1.85f },
    { L"РПГ",       0,   2.2f,   1, 0.0f,    50, 4.0f, 2000, false, 1.50f },
};

static std::vector<Building> buildings;
static std::vector<Ruble> rubles;
static std::vector<Person> people;
static std::vector<Proj> projs;
static std::vector<Boom> booms;
static std::vector<Sign> signs;
static std::vector<Vec3> trees;
static std::vector<Solid> solids;
static std::vector<WPick> wpicks;
static std::vector<Seat> seats;
static std::vector<PCar> pcars;

static int gMoney = 0, gKills = 0, gStars = 0, gWeapon = 0, gWheel = 0;
static bool gOwned[5] = { true, false, false, false, false };
static std::wstring gPopup;
static float gPopupT = 0, gWantedT = 0, gHp = 100, gHurtT = 0, gShootT = 0, gFlashT = 0, gCarT = 0;
static Vec3 gPlayer = { -4.0f, 0.0f, 0.0f }, gFlashPos = { 0.0f, 0.0f, 0.0f }, gSun = { 0.45f, 1.0f, 0.30f };
static Car gCar = { { 4.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 100.0f };
static Heli gHeli = { { 75.0f, 0.0f, 25.0f }, 1.2f, 0.0f, 0.0f, 0.0f };
static bool gInCar = false, gInHeli = false, gCarAlive = true, gShadowPass = false, gQuit = false, gStandCar = false;
static GLuint gCityList = 0, gCityShadow = 0;
static float gLadder = 1.0f, gRobT = -1.0f, gBankCD = 0.0f;   // лестница вертолёта (1 — выдвинута), ограбление банка

// ---------------------------- мелочи ---------------------------------
static float Rnd(float a, float b) { return a + (b - a) * ((float)(rand() % 10001) / 10000.0f); }
static int RandInt(int a, int b) { return a + rand() % (b - a + 1); }
static float Clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float Dist2D(Vec3 a, Vec3 b) { return sqrtf((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)); }
static Vec3 Fwd(float yaw)   { return { sinf(yaw), 0.0f, cosf(yaw) }; }
static Vec3 Right(float yaw) { return { -cosf(yaw), 0.0f, sinf(yaw) }; }
static float AngleDiff(float to, float from) {
    float d = fmodf(to - from + PI_F, 2 * PI_F);
    if (d < 0) d += 2 * PI_F;
    return d - PI_F;
}
static std::wstring I2W(int v) {
    if (v == 0) return L"0";
    bool neg = v < 0;
    if (neg) v = -v;
    std::wstring s;
    while (v > 0) { s.insert(s.begin(), (wchar_t)(L'0' + v % 10)); v /= 10; }
    if (neg) s.insert(s.begin(), L'-');
    return s;
}
static void Popup(const std::wstring& s) { gPopup = s; gPopupT = 2.2f; }

static bool CollideAABB(Vec3& p, float r, float bx, float bz, float sx, float sz) {   // круг радиуса r против коробки
    float hx = sx / 2, hz = sz / 2;
    float cx = Clampf(p.x, bx - hx, bx + hx);
    float cz = Clampf(p.z, bz - hz, bz + hz);
    float dx = p.x - cx, dz = p.z - cz, d2 = dx * dx + dz * dz;
    if (d2 >= r * r) return false;
    if (d2 > 1e-6f) { float d = sqrtf(d2); p.x = cx + dx / d * r; p.z = cz + dz / d * r; }
    else {
        float px = hx - fabsf(p.x - bx), pz = hz - fabsf(p.z - bz);
        if (px < pz) p.x = bx + (p.x >= bx ? 1 : -1) * (hx + r);
        else         p.z = bz + (p.z >= bz ? 1 : -1) * (hz + r);
    }
    return true;
}
static bool Collide(Vec3& p, float r, const Building& b) { return CollideAABB(p, r, b.pos.x, b.pos.z, b.size.x, b.size.z); }

// коробка, повёрнутая на yaw (машина, вертолёт): o — центр, cx/cz/sx/sz — её размеры в её собственных осях
static bool CollideOBB(Vec3& p, float r, float footY, Vec3 o, float yaw, float cx, float cz, float sx, float sz, float top) {
    if (footY >= top - 0.06f) return false;               // мы уже выше коробки: стоим на ней, не упираемся в неё
    float s = sinf(yaw), c = cosf(yaw), dx = p.x - o.x, dz = p.z - o.z;
    Vec3 lp = { dx * c - dz * s, 0.0f, dx * s + dz * c };
    if (!CollideAABB(lp, r, cx, cz, sx, sz)) return false;
    p.x = o.x + lp.x * c + lp.z * s;
    p.z = o.z - lp.x * s + lp.z * c;
    return true;
}
static const float CAR_BODY_TOP = 1.01f, CAR_ROOF_TOP = 1.75f;
static void CarLocal(Vec3 p, Vec3 o, float yaw, float& lx, float& lz) {   // точка мира -> оси машины
    float s = sinf(yaw), c = cosf(yaw), dx = p.x - o.x, dz = p.z - o.z;
    lx = dx * c - dz * s; lz = dx * s + dz * c;
}
static float CarTopAt(float lx, float lz) {               // высота машины в точке (0 — машины там нет)
    if (fabsf(lx) < 0.92f && lz > -1.30f && lz < 0.85f) return CAR_ROOF_TOP;    // салон с крышей
    if (fabsf(lx) < 1.0f && fabsf(lz) < 2.2f) return CAR_BODY_TOP;               // капот, багажник, кузов
    return 0.0f;
}
static bool CarBlocks(Vec3& p, float r, float footY, Vec3 o, float yaw) {         // машина как два блока: кузов и салон
    bool a = CollideOBB(p, r, footY, o, yaw, 0.0f, 0.0f, 2.0f, 4.4f, CAR_BODY_TOP);
    bool b = CollideOBB(p, r, footY, o, yaw, 0.0f, -0.225f, 1.84f, 2.15f, CAR_ROOF_TOP);
    return a || b;
}
static bool CarBoxHit(Vec3 q, Vec3 o, float yaw) {         // попала ли точка (пуля) в машину
    float lx, lz;
    CarLocal(q, o, yaw, lx, lz);
    float t = CarTopAt(lx, lz);
    return t > 0.0f && q.y > 0.1f && q.y < t;
}
// на какой высоте под нами опора: земля, крыша дома, капот / крыша / багажник машины
static float GroundHeight(Vec3 p, float foot, bool& onCar) {
    float h = 0.0f, lim = foot + 0.35f;                    // ступеньку до 35 см берём сразу
    onCar = false;
    for (auto& b : buildings)
        if (b.size.y <= lim && b.size.y > h && fabsf(p.x - b.pos.x) < b.size.x / 2 && fabsf(p.z - b.pos.z) < b.size.z / 2) h = b.size.y;
    for (auto& s : solids)
        if (s.size.y < 1.5f && s.size.y <= lim && s.size.y > h && fabsf(p.x - s.pos.x) < s.size.x / 2 && fabsf(p.z - s.pos.z) < s.size.z / 2) h = s.size.y;
    if (gCarAlive) {
        float lx, lz;
        CarLocal(p, gCar.pos, gCar.yaw, lx, lz);
        float t = CarTopAt(lx, lz);
        if (t > 0.0f && t <= lim && t > h) { h = t; onCar = true; }
    }
    for (auto& pc : pcars) {
        float lx, lz;
        CarLocal(p, pc.pos, pc.yaw, lx, lz);
        float t = CarTopAt(lx, lz);
        if (t > 0.0f && t <= lim && t > h) { h = t; onCar = false; }
    }
    return h;
}
// все твёрдые блоки мира: дома, деревья, фонари и (если vehicles) машины и вертолёт
static void CollideWorld(Vec3& p, float r, float footY, bool vehicles) {
    for (auto& b : buildings) if (footY < b.size.y - 0.05f) Collide(p, r, b);
    for (auto& s : solids) if (footY < s.size.y - 0.05f) CollideAABB(p, r, s.pos.x, s.pos.z, s.size.x, s.size.z);
    if (!vehicles) return;
    if (gCarAlive) CarBlocks(p, r, footY, gCar.pos, gCar.yaw);
    for (auto& pc : pcars) CarBlocks(p, r, footY, pc.pos, pc.yaw);
    if (gHeli.pos.y < footY + 1.9f) {
        CollideOBB(p, r, footY, gHeli.pos, gHeli.yaw, 0.0f, -0.1f, 2.0f, 3.4f, gHeli.pos.y + 2.35f);    // кабина
        CollideOBB(p, r, footY, gHeli.pos, gHeli.yaw, 0.0f, -3.4f, 0.5f, 3.6f, gHeli.pos.y + 2.1f);     // хвост
    }
}
static bool Blocked(Vec3 p, float m) {
    for (auto& b : buildings)
        if (fabsf(p.x - b.pos.x) < b.size.x / 2 + m && fabsf(p.z - b.pos.z) < b.size.z / 2 + m) return true;
    return false;
}
static bool OnRoad(float x, float z) {
    for (int k = -2; k <= 2; k++)
        if (fabsf(x - k * 50.0f) < 7.0f || fabsf(z - k * 50.0f) < 7.0f) return true;
    return false;
}
static Vec3 RandomFreePoint(Vec3 from = { 0.0f, 0.0f, 0.0f }, float minDist = 0.0f) {
    for (int i = 0; i < 300; i++) {
        Vec3 p = { Rnd(-WORLD + 6, WORLD - 6), 0.0f, Rnd(-WORLD + 6, WORLD - 6) };
        if (!Blocked(p, 2.0f) && Dist2D(p, from) >= minDist) return p;
    }
    return { 0.0f, 0.0f, 0.0f };
}
static Vec3 SpawnNear(Vec3 c, float minD, float maxD) {
    for (int i = 0; i < 40; i++) {
        float a = Rnd(0, 2 * PI_F), d = Rnd(minD, maxD);
        Vec3 p = { Clampf(c.x + cosf(a) * d, -WORLD + 4, WORLD - 4), 0.0f, Clampf(c.z + sinf(a) * d, -WORLD + 4, WORLD - 4) };
        if (!Blocked(p, 1.5f)) return p;
    }
    return RandomFreePoint(c, minD);
}
static Vec3 SpawnOnRoad(Vec3 c, float minD, float maxD) {    // точка на дороге (для полицейских машин)
    for (int i = 0; i < 80; i++) {
        float a = Rnd(0, 2 * PI_F), d = Rnd(minD, maxD);
        Vec3 p = { c.x + cosf(a) * d, 0.0f, c.z + sinf(a) * d };
        if (fabsf(p.x) > WORLD - 8 || fabsf(p.z) > WORLD - 8) continue;
        if (OnRoad(p.x, p.z) && !Blocked(p, 3.0f)) return p;
    }
    return SpawnNear(c, minD, maxD);
}
static bool Obstructed(Vec3 p, float m) {          // дом или дерево/фонарь в этой точке (с запасом m)
    if (Blocked(p, m)) return true;
    for (auto& s : solids)
        if (s.size.y >= 1.5f && fabsf(p.x - s.pos.x) < s.size.x / 2 + m && fabsf(p.z - s.pos.z) < s.size.z / 2 + m) return true;
    return false;
}
static bool WallAhead(Vec3 p, float yaw, float dist) {   // есть ли стена впереди на dist метров
    Vec3 f = Fwd(yaw);
    Vec3 a = { p.x + f.x * dist * 0.5f, 0.0f, p.z + f.z * dist * 0.5f }, b = { p.x + f.x * dist, 0.0f, p.z + f.z * dist };
    return Obstructed(a, 0.5f) || Obstructed(b, 0.5f) || fabsf(b.x) > WORLD - 2 || fabsf(b.z) > WORLD - 2;
}
static float FreeYaw(Vec3 p, float yaw, float sgn) {     // повернуться туда, где стены нет
    static const float offs[8] = { 0.5f, 1.0f, 1.6f, 2.3f, -0.5f, -1.0f, -1.6f, -2.3f };
    for (int i = 0; i < 8; i++) {
        float o = (i < 4 ? sgn : -sgn) * fabsf(offs[i]);
        if (!WallAhead(p, yaw + o, 2.6f)) return yaw + o;
    }
    return yaw + PI_F;
}
static bool LineClear(Vec3 a, Vec3 b) {   // нет ли дома между a и b
    int n = (int)(Dist2D(a, b) / 2.0f) + 1;
    for (int i = 1; i < n; i++) {
        float t = (float)i / n;
        Vec3 p = { a.x + (b.x - a.x) * t, 1.4f, a.z + (b.z - a.z) * t };
        for (auto& bl : buildings)
            if (fabsf(p.x - bl.pos.x) < bl.size.x / 2 && fabsf(p.z - bl.pos.z) < bl.size.z / 2 && p.y < bl.size.y) return false;
    }
    return true;
}
static Col RubleColor(int v) { return v < 30 ? Rgba(120, 180, 90) : (v < 100 ? Rgba(90, 150, 220) : (v < 200 ? Rgba(210, 165, 60) : Rgba(170, 90, 200))); }

// ---------------------------- примитивы (с нормалями) ----------------
static void SetCol(const Col& c) { if (!gShadowPass) glColor4f(c.r, c.g, c.b, c.a); }

static void Box(Vec3 p, float w, float h, float l, Col c) {
    float x0 = p.x - w / 2, x1 = p.x + w / 2, y0 = p.y - h / 2, y1 = p.y + h / 2, z0 = p.z - l / 2, z1 = p.z + l / 2;
    SetCol(c);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);  glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glNormal3f(0, 0, 1);  glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glNormal3f(0, 0, -1); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y0, z0);
    glNormal3f(1, 0, 0);  glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
    glNormal3f(-1, 0, 0); glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    glNormal3f(0, -1, 0); glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
    glEnd();
}
// коробка вдоль стены: axis 1 — стена смотрит по z (ширина по x), axis 0 — по x
static void BoxA(Vec3 c, int axis, float along, float h, float depth, Col col) {
    if (axis == 1) Box(c, along, h, depth, col); else Box(c, depth, h, along, col);
}
static void Panel(Vec3 c, int axis, float sg, float w, float h, Col col) {   // плоский прямоугольник на стене
    SetCol(col);
    glBegin(GL_QUADS);
    if (axis == 0) {
        glNormal3f(sg, 0, 0);
        glVertex3f(c.x, c.y - h / 2, c.z - w / 2); glVertex3f(c.x, c.y + h / 2, c.z - w / 2);
        glVertex3f(c.x, c.y + h / 2, c.z + w / 2); glVertex3f(c.x, c.y - h / 2, c.z + w / 2);
    } else {
        glNormal3f(0, 0, sg);
        glVertex3f(c.x - w / 2, c.y - h / 2, c.z); glVertex3f(c.x + w / 2, c.y - h / 2, c.z);
        glVertex3f(c.x + w / 2, c.y + h / 2, c.z); glVertex3f(c.x - w / 2, c.y + h / 2, c.z);
    }
    glEnd();
}
static void Sphere(Vec3 p, float r, int seg, Col c) {
    SetCol(c);
    for (int i = 0; i < seg; i++) {
        float a0 = PI_F * i / seg - PI_F / 2, a1 = PI_F * (i + 1) / seg - PI_F / 2;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= seg * 2; j++) {
            float b = 2 * PI_F * j / (seg * 2), cb = cosf(b), sb = sinf(b);
            glNormal3f(cosf(a0) * cb, sinf(a0), cosf(a0) * sb);
            glVertex3f(p.x + r * cosf(a0) * cb, p.y + r * sinf(a0), p.z + r * cosf(a0) * sb);
            glNormal3f(cosf(a1) * cb, sinf(a1), cosf(a1) * sb);
            glVertex3f(p.x + r * cosf(a1) * cb, p.y + r * sinf(a1), p.z + r * cosf(a1) * sb);
        }
        glEnd();
    }
}
static void CylX(Vec3 p, float r, float len, int n, Col c) {   // цилиндр вдоль оси x (колёса)
    SetCol(c);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= n; i++) {
        float a = 2 * PI_F * i / n, cy = cosf(a), sz = sinf(a);
        glNormal3f(0, cy, sz);
        glVertex3f(p.x - len / 2, p.y + cy * r, p.z + sz * r);
        glVertex3f(p.x + len / 2, p.y + cy * r, p.z + sz * r);
    }
    glEnd();
    for (int s = -1; s <= 1; s += 2) {
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f((float)s, 0, 0);
        glVertex3f(p.x + s * len / 2, p.y, p.z);
        for (int i = 0; i <= n; i++) { float a = 2 * PI_F * i / n; glVertex3f(p.x + s * len / 2, p.y + cosf(a) * r, p.z + sinf(a) * r); }
        glEnd();
    }
}
static void CylY(Vec3 p, float r, float h, int n, Col c) {     // вертикальный цилиндр от p.y вверх
    SetCol(c);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= n; i++) {
        float a = 2 * PI_F * i / n, x = cosf(a), z = sinf(a);
        glNormal3f(x, 0, z);
        glVertex3f(p.x + x * r, p.y, p.z + z * r);
        glVertex3f(p.x + x * r, p.y + h, p.z + z * r);
    }
    glEnd();
}

// ---------------------------- шрифт ----------------------------------
struct GLFont { GLuint asc, cyr, rub; int px; HFONT hf; };
static GLFont MakeFont(HDC dc, int px, bool bold) {
    HFONT f = CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HGDIOBJ old = SelectObject(dc, f);
    GLFont r;
    r.px = px; r.hf = f;
    r.asc = glGenLists(96); wglUseFontBitmapsW(dc, 32, 96, r.asc);          // латиница, цифры
    r.cyr = glGenLists(96); wglUseFontBitmapsW(dc, 0x400, 96, r.cyr);       // кириллица
    r.rub = glGenLists(1);  wglUseFontBitmapsW(dc, 0x20BD, 1, r.rub);       // знак рубля
    SelectObject(dc, old);
    return r;
}
static float TextWidth(HDC dc, const GLFont& f, const std::wstring& s) {
    HGDIOBJ old = SelectObject(dc, f.hf);
    SIZE sz = { 0, 0 };
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    SelectObject(dc, old);
    return (float)sz.cx;
}
static void DrawChars(const std::wstring& s, const GLFont& f) {
    for (wchar_t ch : s) {
        if (ch >= 32 && ch < 128) glCallList(f.asc + (ch - 32));
        else if (ch >= 0x400 && ch < 0x460) glCallList(f.cyr + (ch - 0x400));
        else if (ch == 0x20BD) glCallList(f.rub);
    }
}
// (x, y) — левый верхний угол текста в пикселях (2D-режим)
static void Txt(const std::wstring& s, float x, float y, const GLFont& f, Col c) {
    for (int pass = 0; pass < 2; pass++) {
        float o = pass == 0 ? 1.0f : 0.0f;
        if (pass == 0) glColor4f(0, 0, 0, 0.8f); else glColor4f(c.r, c.g, c.b, c.a);
        glRasterPos2f(x + o, y + o + f.px * 0.85f);
        DrawChars(s, f);
    }
}
static void SignText(const Sign& s, const GLFont& f) {   // надпись, висящая в 3D-мире
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0) glColor4f(0, 0, 0, 1); else glColor4f(1, 1, 1, 1);
        glRasterPos3f(s.pos.x, s.pos.y, s.pos.z);
        glBitmap(0, 0, 0, 0, -s.w / 2 + (pass == 0 ? 1.0f : 0.0f), (pass == 0 ? -1.0f : 0.0f) - f.px * 0.35f, NULL);
        DrawChars(s.text, f);
    }
}

// ---------------------------- город ----------------------------------
static void FaceDirs(int f, float& nx, float& nz, float& tx, float& tz) {
    nx = 0; nz = 0; tx = 0; tz = 0;
    if (f == 0) { nz = 1; tx = 1; } else if (f == 1) { nz = -1; tx = 1; }
    else if (f == 2) { nx = 1; tz = 1; } else { nx = -1; tz = 1; }
}
static Vec3 RoofLadderBase(const Building& b, float out) {   // низ лестницы на крышу — на задней стене здания
    float nx, nz, tx, tz;
    FaceDirs(b.facing, nx, nz, tx, tz);
    float half = b.facing < 2 ? b.size.z / 2 : b.size.x / 2;
    return { b.pos.x - nx * (half + out), 0.0f, b.pos.z - nz * (half + out) };
}
static void AddBuilding(float cx, float cz, float w, float h, float l, int kind, int facing, const wchar_t* name, Col color, Col accent) {
    Building b;
    b.pos = { cx, 0.0f, cz }; b.size = { w, h, l }; b.color = color; b.accent = accent;
    b.kind = kind; b.facing = facing; b.name = name;
    float nx, nz, tx, tz;
    FaceDirs(facing, nx, nz, tx, tz);
    float half = facing < 2 ? l / 2 : w / 2;
    b.door = { cx + nx * (half + 3.0f), 0.0f, cz + nz * (half + 3.0f) };
    buildings.push_back(b);
}
static void BuildCity() {
    srand(7);   // один и тот же город каждый раз
    Col pal[5] = { Rgba(150, 150, 160), Rgba(170, 130, 105), Rgba(120, 140, 165), Rgba(165, 160, 130), Rgba(135, 115, 135) };
    Col acc[5] = { Rgba(200, 60, 50), Rgba(40, 110, 190), Rgba(40, 150, 90), Rgba(210, 150, 30), Rgba(130, 60, 160) };
    const wchar_t* names[8] = { L"МАГАЗИН", L"ОТЕЛЬ", L"ПИЦЦА", L"СУПЕРМАРКЕТ", L"КИНО", L"ОФИС", L"БАР", L"КОФЕЙНЯ" };
    for (int i = -2; i <= 1; i++)
        for (int j = -2; j <= 1; j++) {
            float cx = i * 50.0f + 25, cz = j * 50.0f + 25;
            if (i == 1 && j == 0) continue;                                   // площадь с вертолётом
            if (i == 0 && j == 0)       AddBuilding(cx, cz, 22, 11, 22, 1, 1, L"ОРУЖЕЙНЫЙ МАГАЗИН", Rgba(120, 120, 130), Rgba(190, 40, 40));
            else if (i == -1 && j == 0) AddBuilding(cx, cz, 22, 11, 22, 2, 1, L"АПТЕКА", Rgba(225, 225, 225), Rgba(40, 170, 90));
            else if (i == 0 && j == -1) AddBuilding(cx, cz, 22, 11, 22, 3, 0, L"ПОЛИЦИЯ", Rgba(190, 195, 215), Rgba(30, 60, 160));
            else if (i == -1 && j == -1) AddBuilding(cx, cz, 22, 11, 22, 4, 0, L"КАФЕ", Rgba(200, 170, 130), Rgba(200, 120, 40));
            else {
                if (RandInt(0, 6) == 0) continue;
                int c = RandInt(0, 4);
                AddBuilding(cx + Rnd(-3, 3), cz + Rnd(-3, 3), Rnd(16, 28), Rnd(14, 40), Rnd(16, 28), 0, RandInt(0, 3), names[RandInt(0, 7)], pal[c], acc[RandInt(0, 4)]);
            }
        }
    for (int i = 0; i < 40; i++) {                                            // деревья
        Vec3 p = RandomFreePoint();
        if (!OnRoad(p.x, p.z) && Dist2D(p, { 75.0f, 0.0f, 25.0f }) > 14 && !Blocked(p, 2.5f)) {
            trees.push_back(p);
            solids.push_back({ { p.x, 0.0f, p.z }, { 0.9f, 5.6f, 0.9f } });
        }
    }
}

// геометрия одного здания: стены, окна, витрины, навес, вывеска
static void BuildingGeom(const Building& b, HDC dc, const GLFont& fSign) {
    Vec3 c = { b.pos.x, b.size.y / 2, b.pos.z };
    Box(c, b.size.x, b.size.y, b.size.z, b.color);
    Box({ b.pos.x, b.size.y + 0.25f, b.pos.z }, b.size.x + 0.5f, 0.5f, b.size.z + 0.5f, Darker(b.color, 0.6f));   // карниз
    for (int k = 0; k < 2; k++)                                                                                    // кондиционеры
        Box({ b.pos.x + Rnd(-b.size.x * 0.3f, b.size.x * 0.3f), b.size.y + 0.95f, b.pos.z + Rnd(-b.size.z * 0.3f, b.size.z * 0.3f) }, 1.8f, 0.9f, 1.4f, C_GRAY);
    Col glass[3] = { Rgba(110, 160, 205), Rgba(85, 135, 185), Rgba(140, 190, 225) };
    for (int f = 0; f < 4; f++) {                                                                                  // окна
        float nx, nz, tx, tz;
        FaceDirs(f, nx, nz, tx, tz);
        int axis = f < 2 ? 1 : 0;
        float sg = nx + nz, half = f < 2 ? b.size.z / 2 : b.size.x / 2, width = f < 2 ? b.size.x : b.size.z;
        float y0 = (f == b.facing) ? 5.4f : 1.6f;
        int cols = (int)((width - 3.0f) / 3.4f);
        for (float y = y0; y + 2.3f < b.size.y - 0.6f; y += 3.6f)
            for (int k = 0; k < cols; k++) {
                float off = (k - (cols - 1) / 2.0f) * 3.4f;
                Vec3 pc = { b.pos.x + nx * (half + 0.04f) + tx * off, y + 1.1f, b.pos.z + nz * (half + 0.04f) + tz * off };
                Panel(pc, axis, sg, 2.2f, 2.4f, Rgba(45, 45, 55));                       // рама
                pc.x += nx * 0.03f; pc.z += nz * 0.03f;
                Panel(pc, axis, sg, 1.9f, 2.1f, glass[RandInt(0, 2)]);                    // стекло
            }
    }
    // --- витрины, дверь, навес и вывеска на фасаде ---
    float nx, nz, tx, tz;
    FaceDirs(b.facing, nx, nz, tx, tz);
    int axis = b.facing < 2 ? 1 : 0;
    float sg = nx + nz, half = b.facing < 2 ? b.size.z / 2 : b.size.x / 2, width = b.facing < 2 ? b.size.x : b.size.z;
    auto at = [&](float t, float y, float out) {
        return Vec3{ b.pos.x + nx * (half + out) + tx * t, y, b.pos.z + nz * (half + out) + tz * t };
    };
    for (int s = -1; s <= 1; s += 2) {
        Panel(at(s * 4.4f, 2.0f, 0.05f), axis, sg, 5.6f, 3.2f, Rgba(60, 80, 95));          // рама витрины
        Panel(at(s * 4.4f, 2.0f, 0.08f), axis, sg, 5.2f, 2.8f, C_GLASS);                    // витринное стекло
        Panel(at(s * 4.4f, 1.0f, 0.10f), axis, sg, 5.2f, 0.12f, Rgba(255, 255, 255));       // блик
    }
    Panel(at(0, 1.4f, 0.09f), axis, sg, 1.9f, 2.8f, C_BROWN);                                // дверь
    Panel(at(0, 1.8f, 0.11f), axis, sg, 1.3f, 1.4f, C_GLASS);
    BoxA(at(0, 3.75f, 1.2f), axis, width - 3.0f, 0.22f, 2.4f, b.accent);                     // навес
    BoxA(at(0, 3.6f, 1.2f), axis, width - 3.2f, 0.12f, 2.2f, Darker(b.accent, 0.6f));
    float bw = fminf(width * 0.75f, 14.0f);
    Panel(at(0, 4.65f, 0.10f), axis, sg, bw, 1.2f, Darker(b.accent, 0.55f));                 // вывеска
    Panel(at(0, 4.65f, 0.12f), axis, sg, bw - 0.4f, 0.9f, b.accent);
    Sign sn;
    sn.pos = at(0, 4.55f, 0.4f); sn.text = b.name; sn.w = TextWidth(dc, fSign, b.name);
    signs.push_back(sn);
    {   // лестница на крышу — на задней стене
        float ntx, ntz, ttx, ttz;
        FaceDirs(b.facing, ntx, ntz, ttx, ttz);
        Vec3 lb = RoofLadderBase(b, 0.08f);
        Col rail = Rgba(170, 170, 180), rung = Rgba(120, 120, 130);
        for (int s = -1; s <= 1; s += 2) Box({ lb.x + ttx * s * 0.4f, b.size.y / 2, lb.z + ttz * s * 0.4f }, 0.07f, b.size.y, 0.07f, rail);
        for (float y = 0.5f; y < b.size.y; y += 0.8f) BoxA({ lb.x, y, lb.z }, axis, 0.8f, 0.05f, 0.08f, rung);
    }
}

static void BuildLists(HDC dc, const GLFont& fSign) {
    gCityList = glGenLists(1);
    glNewList(gCityList, GL_COMPILE);
    Box({ 0.0f, -0.6f, 0.0f }, 1200.0f, 1.0f, 1200.0f, Rgba(80, 130, 75));                  // земля до горизонта
    Box({ 0.0f, -0.05f, 0.0f }, WORLD * 2 + 16, 0.1f, WORLD * 2 + 16, Rgba(105, 150, 90));  // газон города
    for (int k = -2; k <= 2; k++) {                                                          // дороги и разметка
        Box({ k * 50.0f, 0.02f, 0.0f }, 12.0f, 0.04f, WORLD * 2 + 16, Rgba(62, 62, 68));
        Box({ 0.0f, 0.021f, k * 50.0f }, WORLD * 2 + 16, 0.04f, 12.0f, Rgba(62, 62, 68));
        for (float s = -WORLD; s < WORLD; s += 10.0f) {
            Box({ k * 50.0f, 0.05f, s }, 0.3f, 0.02f, 4.0f, Rgba(230, 210, 80));
            Box({ s, 0.05f, k * 50.0f }, 4.0f, 0.02f, 0.3f, Rgba(230, 210, 80));
        }
        for (int l = -2; l <= 2; l++) {                                                      // фонари на перекрёстках
            float lx = k * 50.0f + 6.8f, lz = l * 50.0f + 6.8f;
            Box({ lx, 2.6f, lz }, 0.15f, 5.2f, 0.15f, C_DGRAY);
            Box({ lx - 0.4f, 5.2f, lz }, 1.0f, 0.15f, 0.4f, Rgba(250, 240, 190));
            solids.push_back({ { lx, 0.0f, lz }, { 0.5f, 5.2f, 0.5f } });
        }
    }
    for (auto& b : buildings) BuildingGeom(b, dc, fSign);
    for (auto& s : seats) {                                                                  // лавочки и стулья
        Col wood = Rgba(130, 90, 55), metal = Rgba(60, 60, 66);
        if (s.kind == 0) {
            float side = (s.pos.x - floorf(s.pos.x / 50.0f + 0.5f) * 50.0f) > 0 ? 1.0f : -1.0f;   // спинкой от дороги
            Box({ s.pos.x, 0.45f, s.pos.z }, 0.55f, 0.07f, 1.9f, wood);
            Box({ s.pos.x + side * 0.27f, 0.72f, s.pos.z }, 0.07f, 0.42f, 1.9f, wood);
            for (int k = -1; k <= 1; k += 2) Box({ s.pos.x, 0.22f, s.pos.z + k * 0.8f }, 0.5f, 0.44f, 0.07f, metal);
        } else {
            Box({ s.pos.x, 0.45f, s.pos.z }, 0.5f, 0.06f, 0.5f, wood);
            Box({ s.pos.x, 0.72f, s.pos.z - 0.22f }, 0.5f, 0.5f, 0.05f, wood);
            for (int kx = -1; kx <= 1; kx += 2) for (int kz = -1; kz <= 1; kz += 2) Box({ s.pos.x + kx * 0.2f, 0.21f, s.pos.z + kz * 0.2f }, 0.05f, 0.42f, 0.05f, metal);
        }
    }
    for (auto& t : trees) {                                                                  // деревья
        Box({ t.x, 1.2f, t.z }, 0.45f, 2.4f, 0.45f, C_BROWN);
        Box({ t.x, 3.4f, t.z }, 2.6f, 2.4f, 2.6f, Rgba(50, 130, 55));
        Box({ t.x, 5.0f, t.z }, 1.6f, 1.2f, 1.6f, Rgba(60, 150, 65));
    }
    Box({ 75.0f, 0.06f, 25.0f }, 18.0f, 0.12f, 18.0f, Rgba(95, 95, 100));                   // вертолётная площадка
    Box({ 75.0f, 0.13f, 25.0f }, 0.8f, 0.02f, 8.0f, C_YELLOW);
    Box({ 71.5f, 0.13f, 25.0f }, 0.8f, 0.02f, 8.0f, C_YELLOW);
    Box({ 78.5f, 0.13f, 25.0f }, 0.8f, 0.02f, 8.0f, C_YELLOW);
    Box({ 75.0f, 0.13f, 25.0f }, 7.0f, 0.02f, 0.8f, C_YELLOW);
    glEndList();

    gCityShadow = glGenLists(1);                                                             // только то, что отбрасывает тень
    glNewList(gCityShadow, GL_COMPILE);
    gShadowPass = true;
    for (auto& b : buildings) Box({ b.pos.x, b.size.y / 2, b.pos.z }, b.size.x, b.size.y, b.size.z, C_BLACK);
    for (auto& t : trees) Box({ t.x, 2.8f, t.z }, 2.4f, 5.6f, 2.4f, C_BLACK);
    gShadowPass = false;
    glEndList();
}

// ---------------------------- люди и оружие --------------------------
static void DrawWeaponModel(int id) {   // оружие смотрит в +z, начало — у рук
    Col dk = Rgba(35, 35, 40), wood = Rgba(120, 80, 45);
    switch (id) {
    case 0:
        Box({ 0.0f, 0.02f, 0.18f }, 0.06f, 0.08f, 0.32f, Rgba(70, 70, 78));
        Box({ 0.0f, -0.07f, 0.06f }, 0.06f, 0.16f, 0.08f, dk);
        break;
    case 1:
        Box({ 0.0f, 0.0f, 0.30f }, 0.08f, 0.12f, 0.70f, dk);
        Box({ 0.0f, -0.16f, 0.25f }, 0.06f, 0.22f, 0.08f, Rgba(60, 60, 66));
        Box({ 0.0f, -0.02f, -0.10f }, 0.06f, 0.10f, 0.25f, wood);
        Box({ 0.0f, 0.02f, 0.78f }, 0.04f, 0.04f, 0.25f, C_BLACK);
        break;
    case 2:
        Box({ 0.0f, 0.02f, 0.45f }, 0.06f, 0.06f, 0.90f, Rgba(70, 70, 78));
        Box({ 0.0f, -0.04f, 0.35f }, 0.09f, 0.08f, 0.30f, wood);
        Box({ 0.0f, -0.04f, -0.25f }, 0.07f, 0.14f, 0.40f, wood);
        break;
    case 3:
        Box({ 0.0f, 0.0f, 0.40f }, 0.07f, 0.10f, 1.10f, Rgba(70, 85, 70));
        Box({ 0.0f, 0.11f, 0.40f }, 0.07f, 0.07f, 0.40f, C_BLACK);
        Box({ 0.0f, -0.03f, -0.20f }, 0.07f, 0.13f, 0.40f, Rgba(70, 85, 70));
        Box({ 0.0f, 0.01f, 1.10f }, 0.035f, 0.035f, 0.50f, C_BLACK);
        break;
    default:
        Box({ 0.0f, 0.05f, 0.20f }, 0.16f, 0.16f, 1.30f, Rgba(85, 100, 60));
        Box({ 0.0f, 0.05f, 0.95f }, 0.28f, 0.28f, 0.30f, Rgba(60, 75, 45));
        Box({ 0.0f, -0.12f, 0.10f }, 0.06f, 0.20f, 0.08f, dk);
        break;
    }
}

// человек с руками и ногами; aim — руки вытянуты с оружием вперёд
// hat: 0 волосы, 1 фуражка полиции, 2 бандит, 3 спецназ (каска, бронежилет)
static void DrawPerson(Vec3 p, float yaw, float phase, float amp, Col shirt, Col pants, Col hair, int hat, int weapon, bool aim) {
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(yaw * 180.0f / PI_F, 0, 1, 0);
    float sw = sinf(phase) * amp * 38.0f;
    bool cop = hat == 1, swat = hat == 3;
    Col shoe = Rgba(30, 30, 35), vest = Rgba(48, 54, 50), skinD = Darker(C_SKIN, 0.85f);
    for (int s = -1; s <= 1; s += 2) {                                   // ноги
        glPushMatrix();
        glTranslatef(s * 0.15f, 0.88f, 0);
        glRotatef(s * sw, 1, 0, 0);
        Box({ 0.0f, -0.22f, 0.0f }, 0.25f, 0.44f, 0.27f, pants);                    // бедро
        Box({ 0.0f, -0.62f, 0.0f }, 0.23f, 0.40f, 0.25f, Darker(pants, 0.9f));      // голень
        if (swat) Box({ 0.0f, -0.45f, 0.14f }, 0.21f, 0.15f, 0.05f, vest);          // наколенник
        Box({ 0.0f, -0.82f, 0.07f }, 0.25f, 0.12f, 0.40f, shoe);                    // ботинок
        Box({ 0.0f, -0.87f, 0.08f }, 0.26f, 0.03f, 0.42f, Rgba(15, 15, 18));        // подошва
        glPopMatrix();
    }
    Box({ 0.0f, 1.20f, 0.0f }, 0.64f, 0.66f, 0.34f, shirt);              // торс
    Box({ 0.0f, 0.90f, 0.0f }, 0.66f, 0.08f, 0.36f, Rgba(30, 30, 35));   // ремень
    Box({ 0.0f, 0.90f, 0.185f }, 0.10f, 0.07f, 0.02f, Rgba(190, 170, 90));   // пряжка
    Box({ 0.0f, 1.53f, 0.0f }, 0.30f, 0.07f, 0.20f, Darker(shirt, 0.8f));    // воротник
    Box({ 0.0f, 1.56f, 0.0f }, 0.13f, 0.10f, 0.13f, C_SKIN);             // шея
    if (cop) {
        Box({ 0.0f, 1.30f, 0.175f }, 0.07f, 0.40f, 0.02f, Rgba(15, 15, 25));         // галстук
        Box({ 0.14f, 1.38f, 0.18f }, 0.09f, 0.09f, 0.02f, C_GOLD);                    // жетон
        Box({ -0.15f, 1.12f, 0.175f }, 0.16f, 0.12f, 0.02f, Darker(shirt, 0.8f));     // карманы
        Box({ 0.15f, 1.12f, 0.175f }, 0.16f, 0.12f, 0.02f, Darker(shirt, 0.8f));
    }
    if (swat) {
        Box({ 0.0f, 1.22f, 0.0f }, 0.70f, 0.56f, 0.42f, vest);                        // бронежилет
        Box({ -0.17f, 1.05f, 0.23f }, 0.18f, 0.16f, 0.08f, Darker(vest, 0.75f));      // подсумки
        Box({ 0.17f, 1.05f, 0.23f }, 0.18f, 0.16f, 0.08f, Darker(vest, 0.75f));
        Box({ 0.0f, 1.34f, 0.22f }, 0.30f, 0.12f, 0.02f, C_GOLD);                     // нашивка
        Box({ -0.40f, 1.50f, 0.0f }, 0.22f, 0.10f, 0.26f, vest);                      // наплечники
        Box({ 0.40f, 1.50f, 0.0f }, 0.22f, 0.10f, 0.26f, vest);
    }
    Sphere({ 0.0f, 1.78f, 0.0f }, 0.22f, 7, C_SKIN);                     // голова
    Box({ -0.22f, 1.77f, 0.0f }, 0.04f, 0.09f, 0.06f, skinD);            // уши
    Box({  0.22f, 1.77f, 0.0f }, 0.04f, 0.09f, 0.06f, skinD);
    for (int s = -1; s <= 1; s += 2) {
        Box({ s * 0.08f, 1.81f, 0.205f }, 0.06f, 0.05f, 0.04f, C_WHITE);            // глаза
        Box({ s * 0.08f, 1.81f, 0.222f }, 0.03f, 0.04f, 0.02f, C_BLACK);
        Box({ s * 0.08f, 1.865f, 0.2f }, 0.09f, 0.02f, 0.03f, Darker(hair, 0.8f));   // брови
    }
    Box({ 0.0f, 1.76f, 0.225f }, 0.05f, 0.07f, 0.06f, skinD);            // нос
    Box({ 0.0f, 1.69f, 0.215f }, 0.10f, 0.02f, 0.03f, Rgba(150, 60, 60)); // рот
    if (hat == 0) {
        Box({ 0.0f, 1.94f, -0.03f }, 0.42f, 0.14f, 0.40f, hair);          // волосы
        Box({ 0.0f, 1.82f, -0.16f }, 0.40f, 0.30f, 0.12f, hair);          // затылок
        Box({ -0.21f, 1.88f, -0.02f }, 0.04f, 0.18f, 0.28f, hair);        // виски
        Box({ 0.21f, 1.88f, -0.02f }, 0.04f, 0.18f, 0.28f, hair);
    } else if (hat == 1) {                                                // фуражка полиции
        Box({ 0.0f, 1.96f, 0.0f }, 0.46f, 0.11f, 0.46f, Rgba(25, 35, 80));
        Box({ 0.0f, 1.91f, 0.26f }, 0.32f, 0.03f, 0.16f, Rgba(15, 15, 25));
        Box({ 0.0f, 1.97f, 0.24f }, 0.07f, 0.07f, 0.03f, C_GOLD);
        Box({ 0.0f, 1.86f, -0.1f }, 0.40f, 0.16f, 0.22f, hair);           // волосы из-под фуражки
    } else if (hat == 2) {                                                // бандит: шапка и маска
        Box({ 0.0f, 1.96f, 0.0f }, 0.46f, 0.12f, 0.46f, Rgba(20, 20, 20));
        Box({ 0.0f, 1.84f, -0.12f }, 0.44f, 0.26f, 0.20f, Rgba(20, 20, 20));
        Box({ 0.0f, 1.70f, 0.17f }, 0.30f, 0.14f, 0.08f, C_RED);
    } else {                                                              // спецназ: каска, забрало, балаклава
        Col helm = Rgba(25, 28, 32);
        Box({ 0.0f, 1.96f, -0.01f }, 0.50f, 0.22f, 0.50f, helm);
        Box({ -0.25f, 1.82f, -0.02f }, 0.05f, 0.22f, 0.34f, helm);
        Box({ 0.25f, 1.82f, -0.02f }, 0.05f, 0.22f, 0.34f, helm);
        Box({ 0.0f, 1.80f, 0.225f }, 0.36f, 0.12f, 0.03f, Rgba(20, 30, 45));
        Box({ 0.0f, 1.68f, 0.17f }, 0.34f, 0.14f, 0.14f, Rgba(20, 20, 24));
    }
    for (int s = -1; s <= 1; s += 2) {                                   // руки
        glPushMatrix();
        glTranslatef(s * 0.40f, 1.46f, 0);
        if (aim) { glRotatef(-s * 22.0f, 0, 1, 0); glRotatef(-85.0f, 1, 0, 0); }
        else glRotatef(-s * sw * 0.9f, 1, 0, 0);
        Box({ 0.0f, -0.20f, 0.0f }, 0.19f, 0.40f, 0.19f, shirt);
        Box({ 0.0f, -0.55f, 0.0f }, 0.16f, 0.32f, 0.16f, (cop || swat) ? shirt : C_SKIN);
        Box({ 0.0f, -0.74f, 0.0f }, 0.14f, 0.12f, 0.15f, swat ? Rgba(20, 20, 22) : C_SKIN);   // кисть / перчатка
        glPopMatrix();
    }
    if (aim && weapon >= 0) {
        glPushMatrix();
        glTranslatef(0.0f, 1.40f, 0.50f);
        DrawWeaponModel(weapon);
        glPopMatrix();
    }
    glPopMatrix();
}

static Person MakePerson(int type, Vec3 pos) {
    Col shirts[6] = { Rgba(200, 60, 60), Rgba(60, 160, 90), Rgba(230, 200, 70), Rgba(240, 240, 240), Rgba(150, 90, 190), Rgba(70, 130, 200) };
    Col pantsC[4] = { Rgba(50, 60, 100), Rgba(90, 70, 50), Rgba(40, 40, 45), Rgba(120, 120, 130) };
    Col hairs[4]  = { Rgba(40, 30, 25), Rgba(110, 70, 35), Rgba(210, 180, 90), Rgba(25, 25, 25) };
    Person p;
    p.pos = pos; p.yaw = Rnd(0, 2 * PI_F); p.hp = 100; p.alive = true; p.timer = 0; p.wander = 0; p.shootT = Rnd(0.2f, 1.0f);
    p.phase = Rnd(0, 6); p.amp = 0; p.type = type;
    if (type == 0)      { p.shirt = shirts[RandInt(0, 5)]; p.pants = pantsC[RandInt(0, 3)]; p.hair = hairs[RandInt(0, 3)]; }
    else if (type == 1) { p.shirt = Rgba(150, 45, 35);      p.pants = Rgba(70, 60, 50);      p.hair = hairs[3]; }
    else if (type == 2) { p.shirt = Rgba(35, 60, 130);     p.pants = Rgba(25, 35, 70);      p.hair = hairs[1]; }
    else                { p.shirt = Rgba(32, 34, 38);      p.pants = Rgba(28, 30, 34);      p.hair = hairs[3]; }
    return p;
}
static void DrawNPC(const Person& p) {
    int hat = p.type == 0 ? 0 : (p.type == 1 ? 2 : (p.type == 3 ? 3 : 1));
    if (!p.alive) {                                                   // убитый падает на спину и лежит
        float k = Clampf((12.0f - p.timer) / 0.45f, 0.0f, 1.0f);
        k = k * k * (3.0f - 2.0f * k);
        glPushMatrix();
        glTranslatef(p.pos.x, 0.0f, p.pos.z);
        glRotatef(p.yaw * 180.0f / PI_F, 0, 1, 0);
        glTranslatef(0.0f, 0.2f * k, 0.0f);
        glRotatef(-90.0f * k, 1, 0, 0);
        DrawPerson({ 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, p.shirt, p.pants, p.hair, hat, -1, false);
        glPopMatrix();
        return;
    }
    bool hostile = p.type == 1 || ((p.type == 2 || p.type == 3) && gStars > 0);
    int weapon = hostile ? (p.type == 3 ? 2 : ((p.type == 1 || gStars >= 4) ? 1 : 0)) : -1;
    DrawPerson(p.pos, p.yaw, p.phase, p.amp, p.shirt, p.pants, p.hair, hat, weapon, hostile);
}

// ---------------------------- техника --------------------------------
static void Quad3(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 ctr) {   // грань с нормалью наружу (от центра ctr)
    Vec3 e1 = { b.x - a.x, b.y - a.y, b.z - a.z }, e2 = { d.x - a.x, d.y - a.y, d.z - a.z };
    Vec3 n = { e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x };
    Vec3 m = { (a.x + b.x + c.x + d.x) / 4 - ctr.x, (a.y + b.y + c.y + d.y) / 4 - ctr.y, (a.z + b.z + c.z + d.z) / 4 - ctr.z };
    if (n.x * m.x + n.y * m.y + n.z * m.z < 0) { n = { -n.x, -n.y, -n.z }; Vec3 t = b; b = d; d = t; }
    glNormal3f(n.x, n.y, n.z);
    glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z); glVertex3f(c.x, c.y, c.z); glVertex3f(d.x, d.y, d.z);
}
static void Hull(const Vec3 p[8], Col c) {   // скошенная коробка: 0-3 низ, 4-7 верх (для наклонных стёкол)
    static const int F[6][4] = { {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7} };
    Vec3 ctr = { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 8; i++) { ctr.x += p[i].x / 8; ctr.y += p[i].y / 8; ctr.z += p[i].z / 8; }
    SetCol(c);
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; f++) Quad3(p[F[f][0]], p[F[f][1]], p[F[f][2]], p[F[f][3]], ctr);
    glEnd();
}

// style: 0 обычная машина, 1 полицейская, 2 фургон спецназа; siren — время для мигалки
static void DrawCarAt(Vec3 pos, float yaw, Col body, int style, float siren, float wheel) {
    glPushMatrix();
    glTranslatef(pos.x, 0, pos.z);
    glRotatef(yaw * 180.0f / PI_F, 0, 1, 0);
    Col dark = Rgba(30, 30, 34), chrome = Rgba(200, 200, 208), trim = Darker(body, 0.8f);
    Box({ 0.0f, 0.62f, 0.0f }, 2.0f, 0.62f, 4.4f, body);                       // кузов
    Box({ 0.0f, 0.97f, 1.55f }, 1.9f, 0.08f, 1.35f, body);                     // капот
    Box({ 0.0f, 0.97f, -1.77f }, 1.9f, 0.08f, 0.95f, body);                    // багажник
    Box({ 0.0f, 0.30f, 0.0f }, 1.9f, 0.14f, 4.3f, dark);                       // днище
    const Vec3 cab[8] = { { -0.92f, 0.93f, -1.30f }, { 0.92f, 0.93f, -1.30f }, { 0.92f, 0.93f, 0.85f }, { -0.92f, 0.93f, 0.85f },
                          { -0.80f, 1.70f, -1.12f }, { 0.80f, 1.70f, -1.12f }, { 0.80f, 1.70f, 0.55f }, { -0.80f, 1.70f, 0.55f } };
    Hull(cab, C_GLASS);                                                        // салон с наклонными стёклами
    Box({ 0.0f, 1.72f, -0.285f }, 1.66f, 0.06f, 1.77f, trim);                  // крыша
    for (int sx = -1; sx <= 1; sx += 2) {
        Box({ sx * 0.88f, 1.30f, -0.22f }, 0.07f, 0.74f, 0.09f, trim);          // средняя стойка
        Box({ sx * 1.005f, 0.64f, -0.15f }, 0.02f, 0.46f, 1.55f, trim);         // дверь
        Box({ sx * 1.02f, 0.80f, 0.40f }, 0.03f, 0.04f, 0.22f, chrome);         // ручка
        Box({ sx * 1.07f, 1.12f, 0.70f }, 0.14f, 0.12f, 0.22f, body);           // зеркало
    }
    Box({ 0.0f, 0.62f, 2.21f }, 1.1f, 0.20f, 0.04f, dark);                      // решётка радиатора
    Box({ 0.0f, 0.42f, 2.25f }, 1.95f, 0.22f, 0.18f, C_GRAY);                   // бамперы
    Box({ 0.0f, 0.42f, -2.25f }, 1.95f, 0.22f, 0.18f, C_GRAY);
    Box({ 0.0f, 0.44f, 2.35f }, 0.52f, 0.14f, 0.02f, C_WHITE);                  // номера
    Box({ 0.0f, 0.44f, -2.35f }, 0.52f, 0.14f, 0.02f, C_WHITE);
    Box({ -0.68f, 0.78f, 2.22f }, 0.42f, 0.2f, 0.05f, Rgba(255, 250, 200));     // фары
    Box({  0.68f, 0.78f, 2.22f }, 0.42f, 0.2f, 0.05f, Rgba(255, 250, 200));
    Box({ -0.68f, 0.78f, -2.22f }, 0.42f, 0.2f, 0.05f, Rgba(170, 20, 20));
    Box({  0.68f, 0.78f, -2.22f }, 0.42f, 0.2f, 0.05f, Rgba(170, 20, 20));
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            glPushMatrix();
            glTranslatef(sx * 1.0f, 0.38f, sz * 1.40f);
            glRotatef(wheel * 180.0f / PI_F, 1, 0, 0);                              // колесо крутится
            CylX({ 0.0f, 0.0f, 0.0f }, 0.38f, 0.30f, 12, C_BLACK);                  // шина
            CylX({ 0.0f, 0.0f, 0.0f }, 0.25f, 0.31f, 12, Rgba(190, 190, 200));      // диск
            for (int k = 0; k < 5; k++) {                                          // спицы
                glPushMatrix();
                glRotatef(k * 72.0f, 1, 0, 0);
                Box({ sx * 0.162f, 0.13f, 0.0f }, 0.012f, 0.22f, 0.05f, Rgba(60, 60, 68));
                glPopMatrix();
            }
            glPopMatrix();
        }
    if (style > 0) {                                                            // полиция: полоса и мигалка
        Col stripe = style == 1 ? Rgba(25, 55, 140) : Rgba(240, 200, 20);
        for (int sx = -1; sx <= 1; sx += 2) Box({ sx * 1.012f, 0.66f, -0.15f }, 0.02f, style == 1 ? 0.30f : 0.12f, 3.6f, stripe);
        bool a = fmodf(siren, 0.4f) < 0.2f;
        Col rc = a ? Rgba(255, 30, 30) : Rgba(100, 15, 15), bc = a ? Rgba(25, 40, 100) : Rgba(70, 120, 255);
        Box({ 0.0f, 1.78f, -0.30f }, 1.4f, 0.06f, 0.34f, dark);
        bool lit = glIsEnabled(GL_LIGHTING) != 0;
        if (lit) glDisable(GL_LIGHTING);                                        // огни светятся сами
        Box({ -0.35f, 1.85f, -0.30f }, 0.55f, 0.10f, 0.28f, rc);
        Box({  0.35f, 1.85f, -0.30f }, 0.55f, 0.10f, 0.28f, bc);
        if (lit) glEnable(GL_LIGHTING);
    }
    glPopMatrix();
}
static void DrawCar(const Car& c) { DrawCarAt(c.pos, c.yaw, Rgba(200, 35, 40), 0, 0.0f, c.wheel); }
static void DrawPCar(const PCar& c) { DrawCarAt(c.pos, c.yaw, c.swat ? Rgba(30, 32, 38) : Rgba(235, 235, 240), c.swat ? 2 : 1, c.siren, c.wheel); }

static Vec3 LadderPos() {   // низ лестницы вертолёта в мире
    float s = sinf(gHeli.yaw), c = cosf(gHeli.yaw), lx = -1.66f, lz = 0.5f;
    return { gHeli.pos.x + lx * c + lz * s, 0.0f, gHeli.pos.z - lx * s + lz * c };
}

static void DrawHeli(const Heli& h) {
    glPushMatrix();
    glTranslatef(h.pos.x, h.pos.y, h.pos.z);
    glRotatef(h.yaw * 180.0f / PI_F, 0, 1, 0);
    Col body = Rgba(40, 95, 190), dk = Rgba(40, 40, 45);
    Box({ 0.0f, 1.45f, -0.1f }, 2.0f, 1.8f, 3.4f, body);                       // кабина
    Box({ 0.0f, 1.45f, 1.85f }, 1.8f, 1.5f, 1.1f, C_GLASS);                    // лобовое стекло
    Box({ 0.0f, 0.75f, 1.6f }, 1.7f, 0.2f, 1.6f, body);
    Box({ -1.01f, 1.75f, 0.6f }, 0.03f, 0.8f, 1.3f, C_GLASS);                  // боковые окна
    Box({  1.01f, 1.75f, 0.6f }, 0.03f, 0.8f, 1.3f, C_GLASS);
    Box({ -1.02f, 1.35f, -0.9f }, 0.03f, 0.9f, 0.9f, dk);                      // дверь
    Box({ 0.0f, 2.55f, -0.5f }, 1.4f, 0.5f, 1.6f, dk);                         // двигатель
    Box({ 0.0f, 1.85f, -3.4f }, 0.5f, 0.55f, 3.6f, body);                      // хвост
    Box({ 0.0f, 2.55f, -5.0f }, 0.12f, 1.3f, 0.9f, body);
    Box({ 0.0f, 2.05f, -4.8f }, 1.4f, 0.10f, 0.5f, body);
    for (int s = -1; s <= 1; s += 2) {                                         // лыжи
        Box({ s * 1.1f, 0.10f, 0.2f }, 0.12f, 0.10f, 4.0f, dk);
        Box({ s * 1.0f, 0.55f, 1.0f }, 0.08f, 0.95f, 0.08f, dk);
        Box({ s * 1.0f, 0.55f, -0.7f }, 0.08f, 0.95f, 0.08f, dk);
    }
    Box({ 0.0f, 3.0f, -0.5f }, 0.22f, 0.5f, 0.22f, dk);                        // втулка винта
    glPushMatrix();
    glTranslatef(0.0f, 3.3f, -0.5f);
    glRotatef(h.rotor * 180.0f / PI_F, 0, 1, 0);
    Box({ 0.0f, 0.0f, 0.0f }, 9.5f, 0.06f, 0.38f, dk);                         // лопасти
    Box({ 0.0f, 0.0f, 0.0f }, 0.38f, 0.06f, 9.5f, dk);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.32f, 2.55f, -5.0f);
    glRotatef(h.rotor * 3.0f * 180.0f / PI_F, 1, 0, 0);
    Box({ 0.0f, 0.0f, 0.0f }, 0.05f, 1.4f, 0.14f, dk);                         // хвостовой винт
    glPopMatrix();
    // лестница с левого борта: два поручня и перекладины
    glPushMatrix();
    glTranslatef(-1.15f, 1.3f, 0.5f);
    glRotatef(-20.0f, 0, 0, 1);
    glScalef(1.0f, gLadder < 0.02f ? 0.001f : gLadder, 1.0f);      // лестница складывается вверх
    for (int s = -1; s <= 1; s += 2) Box({ 0.0f, -0.75f, s * 0.25f }, 0.06f, 1.5f, 0.06f, Rgba(200, 200, 205));
    for (int k = 1; k <= 9; k++) Box({ 0.0f, -k * 0.16f, 0.0f }, 0.05f, 0.05f, 0.52f, Rgba(200, 200, 205));
    glPopMatrix();
    glPopMatrix();
}

// ---------------------------- бой ------------------------------------
static void KillPerson(Person& p) {
    p.alive = false; p.timer = 12.0f; gKills++;
    gStars = gStars < 5 ? gStars + 1 : 5;
    gWantedT = 30.0f;
    int n = p.type == 0 ? 1 : 2;
    for (int i = 0; i < n; i++) {   // деньги появляются ТОЛЬКО из убитых
        int v = p.type == 0 ? (RandInt(0, 1) ? 20 : 50) : (p.type == 1 ? (RandInt(0, 1) ? 50 : 100) : (p.type == 2 ? (RandInt(0, 1) ? 100 : 200) : (RandInt(0, 1) ? 200 : 300)));
        rubles.push_back({ { p.pos.x + Rnd(-1, 1), 0.0f, p.pos.z + Rnd(-1, 1) }, v, 60.0f });
    }
}
static void HurtPerson(Person& p, float dmg) {
    if (!p.alive) return;
    p.hp -= dmg;
    if (p.hp <= 0) KillPerson(p);
}
static void Explode(Vec3 p) {
    booms.push_back({ p, 0.0f });
    const float R = 8.0f;
    for (auto& e : people) {
        float d = Dist2D(e.pos, p);
        if (e.alive && d < R) HurtPerson(e, 240.0f * (1.0f - d / R));
    }
    for (auto& pc : pcars) { float d = Dist2D(pc.pos, p); if (d < R) pc.hp -= 300.0f * (1.0f - d / R); }
    float dp = Dist2D(gPlayer, p);
    if (!gInHeli && dp < R) {
        if (gInCar) gCar.hp -= 160.0f * (1.0f - dp / R);
        else { gHp -= 80.0f * (1.0f - dp / R); gHurtT = 0.3f; }
    }
}
static bool ProjStep(Proj& b, float dt) {   // true — снаряд исчез
    b.pos.x += b.vel.x * dt; b.pos.y += b.vel.y * dt; b.pos.z += b.vel.z * dt;
    if (b.pos.y < 0) { if (b.rocket) Explode(b.pos); return true; }
    for (auto& bl : buildings)
        if (fabsf(b.pos.x - bl.pos.x) < bl.size.x / 2 && fabsf(b.pos.z - bl.pos.z) < bl.size.z / 2 && b.pos.y < bl.size.y) {
            if (b.rocket) Explode(b.pos);
            return true;
        }
    for (auto& s : solids)
        if (fabsf(b.pos.x - s.pos.x) < s.size.x / 2 && fabsf(b.pos.z - s.pos.z) < s.size.z / 2 && b.pos.y < s.size.y) {
            if (b.rocket) Explode(b.pos);
            return true;
        }
    if (!b.enemy) {
        for (auto& e : people)
            if (e.alive && Dist2D(b.pos, e.pos) < 0.85f && b.pos.y < 2.0f) {
                if (b.rocket) Explode(b.pos); else HurtPerson(e, b.dmg);
                return true;
            }
        for (auto& pc : pcars)
            if (CarBoxHit(b.pos, pc.pos, pc.yaw)) {
                if (b.rocket) Explode(b.pos); else pc.hp -= b.dmg * 0.6f;
                return true;
            }
    } else {
        if (gCarAlive && CarBoxHit(b.pos, gCar.pos, gCar.yaw)) { gCar.hp -= b.dmg; return true; }   // машина — укрытие
        if (!gInHeli && !gInCar && Dist2D(b.pos, gPlayer) < 0.7f && b.pos.y < gPlayer.y + 2.0f && b.pos.y > gPlayer.y) {
            gHp -= b.dmg; gHurtT = 0.2f;
            return true;
        }
    }
    return false;
}
static void EnemyShoot(Person& e, Vec3 tgt, float dmg) {
    Vec3 m = { e.pos.x + sinf(e.yaw) * 0.9f, 1.4f, e.pos.z + cosf(e.yaw) * 0.9f };
    float ty = (gInCar || gInHeli) ? 0.0f : tgt.y;                 // целятся и в того, кто стоит на крыше
    Vec3 d = { tgt.x - m.x + Rnd(-1.2f, 1.2f), ty + 1.1f - m.y + Rnd(-0.4f, 0.4f), tgt.z - m.z + Rnd(-1.2f, 1.2f) };
    float l = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    if (l < 0.01f) return;
    projs.push_back({ m, { d.x / l * 60, d.y / l * 60, d.z / l * 60 }, 1.2f, dmg, true, false });
}

// ---------------------------- камера и 2D ----------------------------
static void SetCamera(Vec3 eye, Vec3 tgt, int W, int H, float fovDeg) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float nearP = 0.3f, farP = 450.0f, top = nearP * tanf(fovDeg * PI_F / 180.0f / 2), right = top * (float)W / (float)H;
    glFrustum(-right, right, -top, top, nearP, farP);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float fx = tgt.x - eye.x, fy = tgt.y - eye.y, fz = tgt.z - eye.z;
    float fl = sqrtf(fx * fx + fy * fy + fz * fz);
    fx /= fl; fy /= fl; fz /= fl;
    float sx = -fz, sy = 0.0f, sz = fx;                                  // s = f x up(0,1,0)
    float sl = sqrtf(sx * sx + sz * sz);
    sx /= sl; sz /= sl;
    float ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;   // u = s x f
    float m[16] = { sx, ux, -fx, 0,  sy, uy, -fy, 0,  sz, uz, -fz, 0,  0, 0, 0, 1 };
    glMultMatrixf(m);
    glTranslatef(-eye.x, -eye.y, -eye.z);
}
static void Quad2(float x, float y, float w, float h, Col c) {
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
static void Disc(float x, float y, float r, Col c) {
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= 16; i++) { float a = 2 * PI_F * i / 16; glVertex2f(x + cosf(a) * r, y + sinf(a) * r); }
    glEnd();
}
static void RingL(float x, float y, float r, Col c) {
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 24; i++) { float a = 2 * PI_F * i / 24; glVertex2f(x + cosf(a) * r, y + sinf(a) * r); }
    glEnd();
}
static void Line2(float x0, float y0, float x1, float y1, Col c) {
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_LINES); glVertex2f(x0, y0); glVertex2f(x1, y1); glEnd();
}
static void Star2(float x, float y, float r, Col c) {   // звезда розыска
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= 10; i++) {
        float a = -PI_F / 2 + i * PI_F / 5, rr = (i % 2 == 0) ? r : r * 0.45f;
        glVertex2f(x + cosf(a) * rr, y + sinf(a) * rr);
    }
    glEnd();
}
static void ScopeMask(float cx, float cy, float r, float outer) {
    glColor4f(0, 0, 0, 1);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 64; i++) {
        float a = 2 * PI_F * i / 64;
        glVertex2f(cx + cosf(a) * r, cy + sinf(a) * r);
        glVertex2f(cx + cosf(a) * outer, cy + sinf(a) * outer);
    }
    glEnd();
}

// ---------------------------- окно и ввод ----------------------------
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CLOSE: gQuit = true; return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEWHEEL: gWheel += (short)HIWORD(w) / 120; return 0;
    }
    return DefWindowProcA(h, m, w, l);
}
static bool gDown[256], gPrev[256], gPressed[256];
static bool gFocused = true;
static void PollKeys(HWND hwnd) {
    gFocused = GetForegroundWindow() == hwnd;
    for (int i = 0; i < 256; i++) {
        gPrev[i] = gDown[i];
        gDown[i] = gFocused && (GetAsyncKeyState(i) & 0x8000) != 0;
        gPressed[i] = gDown[i] && !gPrev[i];
    }
}
static bool Key(char c) { return gDown[(unsigned char)c]; }

// =====================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASSA wc = {};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = "GtaRub2Wnd";
    RegisterClassA(&wc);
    RECT wr = { 0, 0, 1280, 720 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowA("GtaRub2Wnd", "GTA-RUB 4", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                              wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;

    HDC dc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;            // нужен для теней
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(dc, &pfd);
    if (!pf || !SetPixelFormat(dc, pf, &pfd)) { MessageBoxA(hwnd, "OpenGL pixel format failed", "GTA-RUB 4", MB_OK); return 1; }
    HGLRC glrc = wglCreateContext(dc);
    if (!glrc || !wglMakeCurrent(dc, glrc)) { MessageBoxA(hwnd, "OpenGL context failed", "GTA-RUB 4", MB_OK); return 1; }

    typedef BOOL (WINAPI *SwapIntervalFn)(int);
    SwapIntervalFn swapInterval = (SwapIntervalFn)(void*)wglGetProcAddress("wglSwapIntervalEXT");
    if (swapInterval) swapInterval(1);

    // --- общие настройки графики: свет, туман ---
    float sl0 = sqrtf(gSun.x * gSun.x + gSun.y * gSun.y + gSun.z * gSun.z);
    gSun = { gSun.x / sl0, gSun.y / sl0, gSun.z / sl0 };
    glEnable(GL_LIGHT0);
    float amb[4] = { 0.36f, 0.39f, 0.45f, 1.0f }, dif[4] = { 0.95f, 0.92f, 0.82f, 1.0f }, lma[4] = { 0.18f, 0.18f, 0.20f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lma);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    float fogColor[4] = { 0.76f, 0.86f, 0.95f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, 110.0f);
    glFogf(GL_FOG_END, 340.0f);

    GLFont fSmall = MakeFont(dc, 18, false), fMid = MakeFont(dc, 24, true), fBig = MakeFont(dc, 34, true), fSign = MakeFont(dc, 22, true);
    ShowCursor(FALSE);

    BuildCity();
    {   // банк: ближайшее обычное здание к точке (75, -25)
        int bi = -1; float bd = 1e9f;
        for (size_t i = 0; i < buildings.size(); i++)
            if (buildings[i].kind == 0) { float d = Dist2D(buildings[i].pos, { 75.0f, 0.0f, -25.0f }); if (d < bd) { bd = d; bi = (int)i; } }
        if (bi >= 0) { buildings[bi].kind = 5; buildings[bi].name = L"БАНК"; buildings[bi].accent = Rgba(200, 160, 30); buildings[bi].color = Rgba(205, 200, 185); }
    }
    {   // лавочки у дорог и стулья у кафе — на них можно запрыгнуть
        for (int i = 0; i < 60 && seats.size() < 14; i++) {
            Vec3 p = { RandInt(-2, 2) * 50.0f + (RandInt(0, 1) ? 9.2f : -9.2f), 0.0f, Rnd(-WORLD + 12, WORLD - 12) };
            if (OnRoad(p.x, p.z) || Blocked(p, 1.5f) || Obstructed(p, 1.0f)) continue;
            seats.push_back({ p, 0, 1 });
        }
        for (auto& b : buildings) if (b.kind == 4) {
            float nx, nz, tx, tz;
            FaceDirs(b.facing, nx, nz, tx, tz);
            for (int k = 0; k < 4; k++) {
                float t = (k < 2 ? -1.0f : 1.0f) * (2.2f + (k % 2) * 2.0f);
                seats.push_back({ { b.door.x + tx * t - nx * 0.5f, 0.0f, b.door.z + tz * t - nz * 0.5f }, 1, 0 });
            }
        }
        for (auto& s : seats) {
            if (s.kind == 0) solids.push_back({ { s.pos.x, 0.0f, s.pos.z }, { 0.7f, 0.5f, 1.9f } });
            else             solids.push_back({ { s.pos.x, 0.0f, s.pos.z }, { 0.55f, 0.5f, 0.55f } });
        }
    }
    BuildLists(dc, fSign);
    srand((unsigned)time(NULL));
    wpicks.push_back({ { 0.0f, 0.0f, 18.0f }, 1 });         // автомат — рядом со стартом
    wpicks.push_back({ { -18.0f, 0.0f, 0.0f }, 2 });        // дробовик
    wpicks.push_back({ { 50.0f, 0.0f, -30.0f }, 3 });       // снайперка
    wpicks.push_back({ { -50.0f, 0.0f, -50.0f }, 4 });      // РПГ — на дальнем перекрёстке

    for (int i = 0; i < 16; i++) people.push_back(MakePerson(0, RandomFreePoint()));
    for (int i = 0; i < 4; i++) people.push_back(MakePerson(1, RandomFreePoint(gPlayer, 60.0f)));

    bool mouseInit = false, climbing = false;
    int rcB = -1;           // номер здания, на крышу которого лезем (-1 — не лезем)
    float rcH = 0.0f;       // высота лазания
    float camYaw = 0, camPitch = 0.2f, vy = 0, climbT = 0, polTimer = 0, pcTimer = 0, gAmp = 0, gPhase = 0;
    Vec3 climbFrom = { 0.0f, 0.0f, 0.0f };

    LARGE_INTEGER qf, q0, q1;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&q0);
    float gTime = 0;

    while (!gQuit) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) gQuit = true;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (gQuit) break;

        QueryPerformanceCounter(&q1);
        float dt = (float)((double)(q1.QuadPart - q0.QuadPart) / (double)qf.QuadPart);
        q0 = q1;
        if (dt > 0.05f) dt = 0.05f;
        gTime += dt;
        float t = gTime;

        PollKeys(hwnd);
        if (gPressed[VK_ESCAPE]) break;
        RECT cr;
        GetClientRect(hwnd, &cr);
        int W = cr.right, H = cr.bottom;
        if (W < 1 || H < 1) { Sleep(50); continue; }

        // ---- мышь ----
        float mdx = 0, mdy = 0;
        if (gFocused) {
            POINT ctr = { W / 2, H / 2 };
            ClientToScreen(hwnd, &ctr);
            POINT p;
            GetCursorPos(&p);
            if (mouseInit) { mdx = (float)(p.x - ctr.x); mdy = (float)(p.y - ctr.y); }
            mouseInit = true;
            SetCursorPos(ctr.x, ctr.y);
        } else mouseInit = false;

        bool inVeh = gInCar || gInHeli;
        bool zoom = gDown[VK_RBUTTON] && !inVeh && !climbing;
        float fov = zoom ? (gWeapon == 3 ? 14.0f : 48.0f) : 65.0f;
        float sens = zoom ? (gWeapon == 3 ? 0.0008f : 0.002f) : 0.003f;
        camYaw -= mdx * sens;
        camPitch = Clampf(camPitch + mdy * sens, -0.15f, 1.3f);
        if (gInCar) camYaw += AngleDiff(gCar.yaw, camYaw) * 1.2f * dt;
        if (gInHeli) camYaw += AngleDiff(gHeli.yaw, camYaw) * 1.2f * dt;

        // ---- выбор оружия: цифры 1-5 и колесо мыши ----
        for (int k = 0; k < 5; k++) if (gPressed['1' + k]) {
            if (gOwned[k]) gWeapon = k;
            else Popup(std::wstring(L"Нет оружия: ") + WEAPONS[k].name + L" (найди или купи)");
        }
        if (gWheel != 0) {
            int dir = gWheel > 0 ? -1 : 1;
            for (int n = 0; n < 5; n++) { gWeapon = (gWeapon + dir + 5) % 5; if (gOwned[gWeapon]) break; }
            gWheel = 0;
        }

        // ---- магазины: оружие (цифры) и аптека (E) ----
        const Building* shop = nullptr;
        if (!inVeh && !climbing)
            for (auto& b : buildings) if ((b.kind == 1 || b.kind == 2) && Dist2D(gPlayer, b.door) < 5.0f) shop = &b;
        if (shop && shop->kind == 1) {
            for (int k = 1; k < 5; k++)
                if (gPressed['1' + k] && !gOwned[k]) {
                    if (gMoney >= WEAPONS[k].price) {
                        gMoney -= WEAPONS[k].price; gOwned[k] = true; gWeapon = k;
                        Popup(std::wstring(L"Куплено: ") + WEAPONS[k].name);
                    } else Popup(L"Не хватает рублей");
                }
        }
        if (shop && shop->kind == 2 && gPressed['E']) {
            if (gHp >= 99.0f) Popup(L"Ты и так здоров");
            else if (gMoney >= 100) { gMoney -= 100; gHp = 100; Popup(L"Вылечен!"); }
            else Popup(L"Не хватает рублей");
        }

        // ---- банк: E у двери — ограбление (6 секунд у двери) ----
        const Building* bank = nullptr;
        if (!inVeh && !climbing && rcB < 0)
            for (auto& b : buildings) if (b.kind == 5 && Dist2D(gPlayer, b.door) < 6.0f) bank = &b;
        if (gBankCD > 0) gBankCD -= dt;
        if (gRobT >= 0) {
            if (!bank) { gRobT = -1.0f; Popup(L"Ограбление сорвано"); }
            else {
                gRobT += dt; gWantedT = 40.0f;
                if (gRobT >= 6.0f) {
                    int loot = RandInt(1500, 2500);
                    gMoney += loot; gBankCD = 180.0f; gRobT = -1.0f;
                    Popup(L"Банк ограблен! +" + I2W(loot) + L" \u20BD");
                }
            }
        } else if (bank && gPressed['E']) {
            if (gBankCD > 0) Popup(L"Банк пуст - загляни позже");
            else { gRobT = 0.0f; if (gStars < 3) gStars = 3; gWantedT = 40.0f; Popup(L"Грабим банк! Стой у двери 6 секунд"); }
        }

        // ---- F: сесть/выйти (машина, вертолёт по лестнице), R: вызвать машину ----
        if (gPressed['F']) {
            if (gInCar) {
                gInCar = false;
                Vec3 r = Right(gCar.yaw);
                gPlayer = { gCar.pos.x - r.x * 3.2f, 0.0f, gCar.pos.z - r.z * 3.2f };
                vy = 0;
            } else if (gInHeli) {
                if (gHeli.pos.y < 2.5f) { gInHeli = false; Vec3 lp = LadderPos(); gPlayer = { lp.x, 0.0f, lp.z }; gHeli.speed = 0; vy = 0; }
                else Popup(L"Опустись ниже, чтобы выйти");
            } else if (!climbing && rcB < 0) {
                float dc = gCarAlive ? Dist2D(gPlayer, gCar.pos) : 1e9f;
                float dl = Dist2D(gPlayer, LadderPos());
                if (dc < 4.5f && dc <= dl) gInCar = true;
                else if (dl < 3.4f && gHeli.pos.y < 0.5f) { climbing = true; climbT = 0; climbFrom = gPlayer; }
                else {                                                       // лестница на крышу здания
                    for (size_t bi = 0; bi < buildings.size(); bi++)
                        if (rcB < 0 && gPlayer.y < 1.0f && Dist2D(gPlayer, RoofLadderBase(buildings[bi], 1.0f)) < 2.6f) { rcB = (int)bi; rcH = 0.0f; }
                }
            }
        }
        if (gPressed['R'] && !inVeh && gCarAlive) {
            Vec3 f = Fwd(camYaw);
            gCar.pos = { gPlayer.x + f.x * 7, 0.0f, gPlayer.z + f.z * 7 };
            gCar.yaw = camYaw; gCar.speed = 0; gStandCar = false;
        }

        // ---- машина ----
        Car prevCar = gCar;
        if (gCarAlive) {
            float acc = 0, steer = 0;
            if (gInCar) {
                acc   = (Key('W') ? 1.0f : 0.0f) - (Key('S') ? 1.0f : 0.0f);
                steer = (Key('A') ? 1.0f : 0.0f) - (Key('D') ? 1.0f : 0.0f);
                if (gDown[VK_SPACE]) gCar.speed -= gCar.speed * 4.0f * dt;          // ручник
            }
            if (acc != 0) gCar.speed += acc * 24.0f * dt;
            else gCar.speed -= gCar.speed * (gInCar ? 0.8f : 3.0f) * dt;
            gCar.speed = Clampf(gCar.speed, -10.0f, 34.0f);
            gCar.wheel += gCar.speed * dt / 0.38f;
            gCar.yaw += steer * 1.9f * dt * Clampf(gCar.speed / 8.0f, -1.0f, 1.0f);
            Vec3 cf = Fwd(gCar.yaw);
            gCar.pos.x = Clampf(gCar.pos.x + cf.x * gCar.speed * dt, -WORLD + 2, WORLD - 2);
            gCar.pos.z = Clampf(gCar.pos.z + cf.z * gCar.speed * dt, -WORLD + 2, WORLD - 2);
            for (auto& b : buildings) if (Collide(gCar.pos, 2.0f, b)) gCar.speed *= 0.6f;
            for (auto& s : solids) if (CollideAABB(gCar.pos, 1.7f, s.pos.x, s.pos.z, s.size.x, s.size.z)) gCar.speed *= 0.6f;
            if (gCar.hp <= 0) {                                                       // машина взорвалась
                gCarAlive = false; gCarT = 10.0f;
                if (gInCar) { gInCar = false; Vec3 r = Right(gCar.yaw); gPlayer = { gCar.pos.x - r.x * 3.2f, 0.0f, gCar.pos.z - r.z * 3.2f }; }
                Explode(gCar.pos);
                Popup(L"Машина взорвалась!");
            }
        } else {
            gCarT -= dt;
            if (gCarT <= 0) { gCarAlive = true; gCar = { { 4.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 100.0f }; }
        }

        // ---- стоим на машине — едем вместе с ней ----
        if (gStandCar && gCarAlive && !gInCar && !gInHeli && !climbing) {
            float s0 = sinf(prevCar.yaw), c0 = cosf(prevCar.yaw);
            float dx = gPlayer.x - prevCar.pos.x, dz = gPlayer.z - prevCar.pos.z;
            float lx = dx * c0 - dz * s0, lz = dx * s0 + dz * c0;
            float s1 = sinf(gCar.yaw), c1 = cosf(gCar.yaw);
            gPlayer.x = gCar.pos.x + lx * c1 + lz * s1;
            gPlayer.z = gCar.pos.z - lx * s1 + lz * c1;
        }

        // ---- лестница вертолёта: в полёте убрана, на земле выдвинута ----
        {
            float tgtL = gInHeli ? 0.0f : 1.0f;
            if (gLadder < tgtL) gLadder = fminf(tgtL, gLadder + dt * 1.2f); else gLadder = fmaxf(tgtL, gLadder - dt * 1.2f);
        }
        // ---- вертолёт ----
        if (gInHeli) {
            float acc = (Key('W') ? 1.0f : 0.0f) - (Key('S') ? 1.0f : 0.0f);
            float steer = (Key('A') ? 1.0f : 0.0f) - (Key('D') ? 1.0f : 0.0f);
            if (acc != 0) gHeli.speed = Clampf(gHeli.speed + acc * 18.0f * dt, -12.0f, 24.0f);
            else gHeli.speed -= gHeli.speed * 1.2f * dt;
            gHeli.yaw += steer * 1.5f * dt;
            if (gDown[VK_SPACE]) gHeli.vy = fminf(gHeli.vy + 16.0f * dt, 11.0f);
            else if (gDown[VK_SHIFT]) gHeli.vy = fmaxf(gHeli.vy - 16.0f * dt, -11.0f);
            else gHeli.vy -= gHeli.vy * 3.0f * dt;
            gHeli.rotor += 30.0f * dt;
        } else {
            gHeli.speed -= gHeli.speed * 3.0f * dt;
            gHeli.vy = gHeli.pos.y > 0.0f ? -3.0f : 0.0f;                  // без пилота вертолёт плавно садится
            gHeli.rotor += 2.0f * dt;                                       // винт лениво крутится на земле
        }
        {
            Vec3 hf = Fwd(gHeli.yaw);
            gHeli.pos.x = Clampf(gHeli.pos.x + hf.x * gHeli.speed * dt, -WORLD + 3, WORLD - 3);
            gHeli.pos.z = Clampf(gHeli.pos.z + hf.z * gHeli.speed * dt, -WORLD + 3, WORLD - 3);
            gHeli.pos.y = Clampf(gHeli.pos.y + gHeli.vy * dt, 0.0f, 90.0f);
            for (auto& b : buildings) if (gHeli.pos.y < b.size.y + 1.5f && Collide(gHeli.pos, 3.2f, b)) gHeli.speed *= 0.5f;
            for (auto& s : solids) if (gHeli.pos.y < s.size.y + 1.5f && CollideAABB(gHeli.pos, 3.0f, s.pos.x, s.pos.z, s.size.x, s.size.z)) gHeli.speed *= 0.5f;
        }

        // ---- игрок ----
        float walkSpeed = 0;
        if (climbing) {                                           // лезем по лестнице в кабину
            climbT += dt;
            float u = Clampf(climbT / 1.3f, 0.0f, 1.0f);
            gPlayer = { climbFrom.x + (gHeli.pos.x - climbFrom.x) * u, 1.3f * u, climbFrom.z + (gHeli.pos.z - climbFrom.z) * u };
            if (u >= 1.0f) { climbing = false; gInHeli = true; }
        } else if (rcB >= 0) {                                    // лезем по лестнице на крышу
            const Building& rb = buildings[rcB];
            float nx, nz, tx, tz;
            FaceDirs(rb.facing, nx, nz, tx, tz);
            float half = rb.facing < 2 ? rb.size.z / 2 : rb.size.x / 2;
            rcH += 5.0f * dt;
            vy = 0; gStandCar = false;
            if (rcH >= rb.size.y + 0.6f) {                         // долезли — выходим на крышу
                gPlayer = { rb.pos.x - nx * (half - 1.8f), rb.size.y, rb.pos.z - nz * (half - 1.8f) };
                rcB = -1; rcH = 0.0f;
            } else gPlayer = { rb.pos.x - nx * (half + 0.55f), Clampf(rcH, 0.0f, rb.size.y), rb.pos.z - nz * (half + 0.55f) };
        } else if (gInCar) { gPlayer = { gCar.pos.x, 0.0f, gCar.pos.z }; gStandCar = false; }
        else if (gInHeli)  { gPlayer = { gHeli.pos.x, 0.0f, gHeli.pos.z }; gStandCar = false; }
        else {
            float mx = (Key('D') ? 1.0f : 0.0f) - (Key('A') ? 1.0f : 0.0f);
            float mz = (Key('W') ? 1.0f : 0.0f) - (Key('S') ? 1.0f : 0.0f);
            Vec3 f = Fwd(camYaw), r = Right(camYaw);
            float vx = f.x * mz + r.x * mx, vz = f.z * mz + r.z * mx;
            float len = sqrtf(vx * vx + vz * vz);
            float spd = gDown[VK_SHIFT] ? 11.0f : 6.5f;
            if (len > 0) { gPlayer.x += vx / len * spd * dt; gPlayer.z += vz / len * spd * dt; walkSpeed = spd; }
            gPlayer.x = Clampf(gPlayer.x, -WORLD + 1, WORLD - 1);
            gPlayer.z = Clampf(gPlayer.z, -WORLD + 1, WORLD - 1);
            float footPrev = gPlayer.y;
            CollideWorld(gPlayer, 0.5f, footPrev, true);                   // стены, деревья, фонари, машины, вертолёт — насквозь не пройти
            for (auto& e : people) if (e.alive && gPlayer.y < 1.8f) {     // сквозь людей не пройти
                float dx = gPlayer.x - e.pos.x, dz = gPlayer.z - e.pos.z, dd = sqrtf(dx * dx + dz * dz);
                if (dd < 0.95f) {
                    if (dd < 0.01f) { dx = 1.0f; dz = 0.0f; dd = 1.0f; }
                    float k = (0.95f - dd) / dd;
                    gPlayer.x += dx * k; gPlayer.z += dz * k;
                }
            }
            bool onCar = false;
            float gh = GroundHeight(gPlayer, footPrev, onCar);             // что под ногами: земля, капот, крыша машины
            if (gPressed[VK_SPACE] && footPrev <= gh + 0.02f) vy = 7.5f;
            vy -= 20.0f * dt;
            gPlayer.y += vy * dt;
            if (gPlayer.y <= gh) { gPlayer.y = gh; if (vy < 0) vy = 0; }
            gStandCar = onCar && gPlayer.y <= gh + 0.01f;
        }
        gAmp += ((walkSpeed > 0 ? 1.0f : 0.0f) - gAmp) * fminf(1.0f, 10.0f * dt);
        gPhase += walkSpeed * dt * 1.7f;
        Vec3 focus = gInCar ? gCar.pos : (gInHeli ? gHeli.pos : gPlayer);

        // ---- полицейские машины: по дорогам к игроку, таранят, высаживают копов или спецназ ----
        {
            pcTimer -= dt;
            int wantCars = gStars >= 2 ? gStars - 1 : 0;                   // 2 звезды - 1 машина ... 5 звёзд - 4
            int wantVans = gStars >= 4 ? gStars - 3 : 0;                   // 4 звезды - 1 фургон спецназа, 5 - 2
            int haveVans = 0;
            for (auto& pc : pcars) if (pc.swat) haveVans++;
            if ((int)pcars.size() < wantCars && pcTimer <= 0) {
                PCar nc;
                Vec3 sp = SpawnOnRoad(focus, 70.0f, 100.0f);
                nc.wheel = 0.0f; nc.pos = sp; nc.yaw = atan2f(focus.x - sp.x, focus.z - sp.z); nc.speed = 0.0f;
                nc.swat = haveVans < wantVans;
                nc.hp = nc.swat ? 220.0f : 150.0f;
                nc.siren = Rnd(0.0f, 1.0f); nc.hitT = 0.0f; nc.stuck = 0.0f; nc.rev = 0.0f; nc.revDir = 1.0f; nc.unloaded = false;
                pcars.push_back(nc);
                pcTimer = 3.0f;
            }
            for (size_t i = 0; i < pcars.size();) {
                PCar& pc = pcars[i];
                pc.siren += dt;
                if (pc.hitT > 0) pc.hitT -= dt;
                if (pc.hp <= 0) {                                          // машину разбили
                    Vec3 where = pc.pos;
                    pcars.erase(pcars.begin() + i);
                    gStars = gStars < 5 ? gStars + 1 : 5; gWantedT = 30.0f;
                    Popup(L"Полицейская машина уничтожена!");
                    Explode(where);
                    continue;
                }
                float d = Dist2D(pc.pos, focus);
                if (gStars == 0) {
                    pc.speed -= pc.speed * 2.5f * dt;
                    if (d > 55.0f) { pcars.erase(pcars.begin() + i); continue; }
                } else {
                    float want = atan2f(focus.x - pc.pos.x, focus.z - pc.pos.z);
                    float da = AngleDiff(want, pc.yaw);
                    bool stopped = !gInCar && d < 14.0f;                   // пешего игрока окружают — машина встаёт
                    float steer, targetSp;
                    if (pc.rev > 0) { pc.rev -= dt; targetSp = -8.0f; steer = pc.revDir; }
                    else {
                        steer = Clampf(da * 2.0f, -1.0f, 1.0f);
                        targetSp = stopped ? 0.0f : (fabsf(da) > 1.0f ? 9.0f : 22.0f + gStars * 1.5f);
                        if (!stopped && fabsf(pc.speed) < 2.5f) pc.stuck += dt; else pc.stuck = 0.0f;
                        if (pc.stuck > 0.9f) { pc.rev = 1.1f; pc.revDir = Rnd(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f; pc.stuck = 0.0f; }   // застряла — задний ход
                    }
                    pc.speed += Clampf(targetSp - pc.speed, -30.0f * dt, 18.0f * dt);
                    pc.yaw += steer * 1.9f * dt * Clampf(pc.speed / 8.0f, -1.0f, 1.0f);
                    if (!pc.unloaded && !gInCar && d < 16.0f && pc.rev <= 0) {   // приехали: выходят полицейские (или спецназ)
                        pc.unloaded = true;
                        Vec3 rr = Right(pc.yaw), ff = Fwd(pc.yaw);
                        int n = pc.swat ? 3 : 2;
                        for (int k = 0; k < n; k++) {
                            float side = (k % 2 == 0) ? 2.8f : -2.8f, back = -0.8f - (k / 2) * 1.4f;
                            Person cp = MakePerson(pc.swat ? 3 : 2, { pc.pos.x + rr.x * side + ff.x * back, 0.0f, pc.pos.z + rr.z * side + ff.z * back });
                            cp.hp = pc.swat ? 220.0f : 100.0f + gStars * 10.0f;
                            people.push_back(cp);
                        }
                    }
                }
                Vec3 pf = Fwd(pc.yaw);
                pc.wheel += pc.speed * dt / 0.38f;
                pc.pos.x = Clampf(pc.pos.x + pf.x * pc.speed * dt, -WORLD + 2, WORLD - 2);
                pc.pos.z = Clampf(pc.pos.z + pf.z * pc.speed * dt, -WORLD + 2, WORLD - 2);
                for (auto& b : buildings) if (Collide(pc.pos, 2.0f, b)) pc.speed *= 0.6f;
                for (auto& s : solids) if (CollideAABB(pc.pos, 1.7f, s.pos.x, s.pos.z, s.size.x, s.size.z)) pc.speed *= 0.6f;
                if (gCarAlive) {                                           // таран машины игрока
                    float dd = Dist2D(pc.pos, gCar.pos);
                    if (dd < 3.8f && dd > 0.01f) {
                        float nx = (gCar.pos.x - pc.pos.x) / dd, nz = (gCar.pos.z - pc.pos.z) / dd, push = (3.8f - dd) * 0.5f;
                        gCar.pos.x += nx * push; gCar.pos.z += nz * push;
                        pc.pos.x -= nx * push; pc.pos.z -= nz * push;
                        float rel = fabsf(pc.speed) + fabsf(gCar.speed);
                        if (pc.hitT <= 0 && rel > 8.0f) {
                            gCar.hp -= rel * 0.5f; pc.hp -= fabsf(gCar.speed) * 0.9f + 5.0f; pc.hitT = 0.4f;
                            gCar.speed *= 0.6f; pc.speed *= 0.6f;
                            if (gInCar) gHurtT = 0.15f;
                        }
                    }
                }
                i++;
            }
            for (size_t a = 0; a < pcars.size(); a++)                      // полицейские машины не едут друг сквозь друга
                for (size_t c = a + 1; c < pcars.size(); c++) {
                    float dd = Dist2D(pcars[a].pos, pcars[c].pos);
                    if (dd < 3.8f && dd > 0.01f) {
                        float nx = (pcars[c].pos.x - pcars[a].pos.x) / dd, nz = (pcars[c].pos.z - pcars[a].pos.z) / dd, push = (3.8f - dd) * 0.5f;
                        pcars[a].pos.x -= nx * push; pcars[a].pos.z -= nz * push;
                        pcars[c].pos.x += nx * push; pcars[c].pos.z += nz * push;
                    }
                }
        }


        // ---- камера от третьего лица ----
        Vec3 rt = Right(camYaw);
        float shoulder = (gInCar || gInHeli) ? 0.0f : 0.9f;
        float baseH = gInHeli ? gHeli.pos.y + 2.5f : (gInCar ? 1.6f : gPlayer.y + 1.6f);
        Vec3 tgt = { focus.x + rt.x * shoulder, baseH, focus.z + rt.z * shoulder };
        float dist = gInHeli ? 17.0f : (gInCar ? 10.5f : 5.5f);
        Vec3 eye = { tgt.x - sinf(camYaw) * cosf(camPitch) * dist,
                     tgt.y + sinf(camPitch) * dist,
                     tgt.z - cosf(camYaw) * cosf(camPitch) * dist };
        if (eye.y < 0.5f) eye.y = 0.5f;

        // ---- стрельба: из машины и вертолёта стрелять нельзя ----
        const Weapon& wp = WEAPONS[gWeapon];
        gShootT -= dt;
        if (gFlashT > 0) gFlashT -= dt;
        bool fireIn = wp.automatic ? gDown[VK_LBUTTON] : gPressed[VK_LBUTTON];
        if (!inVeh && !climbing && rcB < 0 && fireIn && gShootT <= 0) {
            gShootT = wp.rate;
            Vec3 hf = Fwd(camYaw);
            Vec3 m = { gPlayer.x + hf.x * wp.muzzle, gPlayer.y + 1.42f, gPlayer.z + hf.z * wp.muzzle };
            Vec3 n = { tgt.x - eye.x, tgt.y - eye.y, tgt.z - eye.z };
            float nl = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
            n = { n.x / nl, n.y / nl, n.z / nl };
            float s = 90.0f;
            if (n.y < -0.01f) s = fminf(s, (eye.y - 1.3f) / -n.y);               // целимся на уровень груди
            Vec3 aim = { eye.x + n.x * s, eye.y + n.y * s, eye.z + n.z * s };
            Vec3 d0 = { aim.x - m.x, aim.y - m.y, aim.z - m.z };
            float dl0 = sqrtf(d0.x * d0.x + d0.y * d0.y + d0.z * d0.z);
            if (dl0 > 0.01f) {
                d0 = { d0.x / dl0, d0.y / dl0, d0.z / dl0 };
                for (int k = 0; k < wp.pellets; k++) {
                    Vec3 d = { d0.x + Rnd(-1, 1) * wp.spread, d0.y + Rnd(-1, 1) * wp.spread, d0.z + Rnd(-1, 1) * wp.spread };
                    float dl = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
                    projs.push_back({ m, { d.x / dl * wp.speed, d.y / dl * wp.speed, d.z / dl * wp.speed }, wp.life, wp.dmg, false, gWeapon == 4 });
                }
                gFlashPos = m; gFlashT = 0.05f;
            }
        }

        // ---- люди: прохожие, бандиты, полиция ----
        int pol = 0;
        int swatN = 0;
        for (auto& e : people) if (e.alive) { if (e.type == 2) pol++; else if (e.type == 3) swatN++; }
        polTimer -= dt;
        if (gStars > 0 && polTimer <= 0) {
            int wantCops = gStars >= 3 ? 3 : gStars * 2;                   // с 3 звёзд к полиции добавляется спецназ
            int wantSwat = gStars >= 3 ? (gStars - 2) * 2 : 0;             // 3 звезды - 2, 4 - 4, 5 - 6
            if (swatN < wantSwat) {
                Person sw = MakePerson(3, SpawnNear(focus, 40.0f, 70.0f));
                sw.hp = 220.0f;
                people.push_back(sw);
                polTimer = 1.5f;
            } else if (pol < wantCops) {
                Person cop = MakePerson(2, SpawnNear(focus, 40.0f, 70.0f));
                cop.hp = 100.0f + gStars * 10.0f;
                people.push_back(cop);
                polTimer = 1.2f;
            }
        }
        for (size_t i = 0; i < people.size();) {           // убрать лишних полицейских
            Person& e = people[i];
            if ((e.type == 2 || e.type == 3) && ((!e.alive && e.timer <= 6.0f) || (gStars == 0 && Dist2D(e.pos, focus) > 55.0f))) people.erase(people.begin() + i);
            else i++;
        }
        for (auto& e : people) {
            if (!e.alive) {
                e.timer -= dt;
                if (e.timer <= 0) { e.pos = RandomFreePoint(focus, 45.0f); e.hp = 100; e.alive = true; }
                continue;
            }
            float d = Dist2D(e.pos, focus);
            bool hostile = e.type == 1 || ((e.type == 2 || e.type == 3) && gStars > 0);
            Vec3 dir = { 0.0f, 0.0f, 0.0f };
            float sp = 0;
            if (hostile && d < (e.type == 1 ? 40.0f : 60.0f)) {
                float range = e.type == 1 ? 10.0f : (e.type == 3 ? 16.0f : 12.0f);
                if (d > 0.01f) e.yaw = atan2f(focus.x - e.pos.x, focus.z - e.pos.z);
                if (d > range) { dir = Fwd(WallAhead(e.pos, e.yaw, 2.2f) ? FreeYaw(e.pos, e.yaw, (((int)floorf(e.pos.x / 8.0f) + (int)floorf(e.pos.z / 8.0f)) & 1) ? 1.0f : -1.0f) : e.yaw); sp = e.type == 1 ? 4.8f : (e.type == 3 ? 6.0f : 5.4f) + gStars * 0.3f; }
                e.shootT -= dt;
                if (d < 34.0f && e.shootT <= 0 && LineClear(e.pos, focus)) {
                    if (e.type == 3) { EnemyShoot(e, focus, 7.0f); EnemyShoot(e, focus, 7.0f); }
                    else EnemyShoot(e, focus, e.type == 1 ? 6.0f : 8.0f);
                    e.shootT = e.type == 1 ? Rnd(0.5f, 1.0f) : (e.type == 3 ? Rnd(0.45f, 0.8f) : fmaxf(0.3f, 0.9f - gStars * 0.1f));
                }
            } else {
                e.wander -= dt;
                if (e.wander <= 0) { e.yaw = Rnd(0, 2 * PI_F); e.wander = Rnd(1, 4); }
                if (WallAhead(e.pos, e.yaw, 2.2f)) { e.yaw = FreeYaw(e.pos, e.yaw, Rnd(0, 1) < 0.5f ? 1.0f : -1.0f); e.wander = Rnd(1.5f, 3.5f); }   // стена впереди — свернуть
                dir = Fwd(e.yaw); sp = 1.5f;
            }
            e.pos.x = Clampf(e.pos.x + dir.x * sp * dt, -WORLD + 1, WORLD - 1);
            e.pos.z = Clampf(e.pos.z + dir.z * sp * dt, -WORLD + 1, WORLD - 1);
            CollideWorld(e.pos, 0.6f, 0.0f, !(gInCar && fabsf(gCar.speed) > 3.0f));
            e.amp += ((sp > 0 ? 1.0f : 0.0f) - e.amp) * fminf(1.0f, 8.0f * dt);
            e.phase += sp * dt * 1.7f;
            if (gInCar && gCarAlive && fabsf(gCar.speed) > 6.0f && Dist2D(e.pos, gCar.pos) < 2.6f) {   // сбили машиной
                HurtPerson(e, 120.0f);
                gCar.speed *= 0.93f;
            }
        }

        for (size_t a = 0; a < people.size(); a++) if (people[a].alive)          // люди не слипаются друг с другом
            for (size_t c = a + 1; c < people.size(); c++) if (people[c].alive) {
                float dx = people[c].pos.x - people[a].pos.x, dz = people[c].pos.z - people[a].pos.z, dd = sqrtf(dx * dx + dz * dz);
                if (dd < 0.8f && dd > 0.001f) {
                    float k = (0.8f - dd) / dd * 0.5f;
                    people[a].pos.x -= dx * k; people[a].pos.z -= dz * k;
                    people[c].pos.x += dx * k; people[c].pos.z += dz * k;
                }
            }

        // ---- снаряды ----
        for (size_t i = 0; i < projs.size();) {
            Proj& b = projs[i];
            b.life -= dt;
            bool dead = false;
            if (b.life <= 0) { if (b.rocket) Explode(b.pos); dead = true; }
            else {
                float sp = sqrtf(b.vel.x * b.vel.x + b.vel.y * b.vel.y + b.vel.z * b.vel.z);
                int n = (int)(sp * dt / 0.5f) + 1;
                for (int s = 0; s < n && !dead; s++) dead = ProjStep(b, dt / n);
            }
            if (dead) projs.erase(projs.begin() + i); else i++;
        }
        for (size_t i = 0; i < booms.size();) {
            booms[i].t += dt;
            if (booms[i].t > 0.7f) booms.erase(booms.begin() + i); else i++;
        }

        // ---- рубли: лежат только там, где кого-то убили ----
        float pickR = gInCar ? 3.2f : (gInHeli ? 0.0f : 1.9f);
        for (size_t i = 0; i < rubles.size();) {
            Ruble& r = rubles[i];
            r.life -= dt;
            bool remove = r.life <= 0;
            if (!remove && pickR > 0 && Dist2D(r.pos, focus) < pickR) {
                gMoney += r.value;
                Popup(L"+" + I2W(r.value) + L" \u20BD");
                remove = true;
            }
            if (remove) rubles.erase(rubles.begin() + i); else i++;
        }

        // ---- оружие, лежащее на земле ----
        if (!inVeh && !climbing && rcB < 0)
            for (size_t i = 0; i < wpicks.size();) {
                if (Dist2D(wpicks[i].pos, gPlayer) < 1.8f && gPlayer.y < 2.0f) {
                    int w = wpicks[i].w;
                    gOwned[w] = true; gWeapon = w;
                    Popup(std::wstring(L"Нашёл: ") + WEAPONS[w].name);
                    wpicks.erase(wpicks.begin() + i);
                } else i++;
            }

        // ---- розыск, здоровье ----
        if (gStars > 0) { gWantedT -= dt; if (gWantedT <= 0) { gStars--; gWantedT = 20.0f; } }
        if (gHp < 100) gHp = fminf(100.0f, gHp + 1.0f * dt);
        if (gHp <= 0) {
            gMoney /= 2; gHp = 100; vy = 0; gInCar = false; gInHeli = false; climbing = false; gStars = 0; pcars.clear(); gStandCar = false; gRobT = -1.0f; rcB = -1;
            gPlayer = { -4.0f, 0.0f, 0.0f };
            Popup(L"Тебя вырубили! Потеряна половина рублей");
        }
        if (gHurtT > 0) gHurtT -= dt;
        if (gPopupT > 0) gPopupT -= dt;

        // =================== РИСОВАНИЕ ===================
        glViewport(0, 0, W, H);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG);   // небо
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, 1, 0, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
        glBegin(GL_QUADS);
        glColor4f(0.76f, 0.86f, 0.95f, 1); glVertex2f(0, 0); glVertex2f(1, 0);
        glColor4f(0.28f, 0.52f, 0.90f, 1); glVertex2f(1, 1); glVertex2f(0, 1);
        glEnd();

        glEnable(GL_DEPTH_TEST); glEnable(GL_FOG); glEnable(GL_LIGHTING);
        SetCamera(eye, tgt, W, H, fov);
        float lp[4] = { gSun.x, gSun.y, gSun.z, 0.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, lp);

        glCallList(gCityList);
        for (auto& r : rubles) {                                                  // купюры, упавшие с убитых
            glPushMatrix();
            glTranslatef(r.pos.x, 1.0f + sinf(t * 3 + r.pos.x) * 0.15f, r.pos.z);
            glRotatef(t * 120.0f, 0, 1, 0);
            Box({ 0.0f, 0.0f, 0.0f }, 1.2f, 0.06f, 0.6f, RubleColor(r.value));
            Box({ 0.0f, 0.035f, 0.0f }, 0.5f, 0.02f, 0.36f, Rgba(240, 240, 225));
            glPopMatrix();
        }
        for (auto& e : people) if (e.alive || e.timer > 6.0f) DrawNPC(e);
        if (!gInCar && !gInHeli) {
            DrawPerson(gPlayer, camYaw, gPhase, gAmp, Rgba(30, 90, 200), Rgba(50, 50, 70), Rgba(90, 60, 35), 0, (climbing || rcB >= 0) ? -1 : gWeapon, !(climbing || rcB >= 0));
        }
        if (gCarAlive) DrawCar(gCar);
        for (auto& pc : pcars) DrawPCar(pc);
        DrawHeli(gHeli);

        for (auto& wp : wpicks) {                                                 // оружие на земле: вращается над золотой площадкой
            glPushMatrix();
            glTranslatef(wp.pos.x, 1.1f + sinf(t * 2.0f + wp.pos.x) * 0.12f, wp.pos.z);
            glRotatef(t * 90.0f, 0, 1, 0);
            glScalef(1.5f, 1.5f, 1.5f);
            glTranslatef(0.0f, 0.0f, -0.3f);
            DrawWeaponModel(wp.w);
            glPopMatrix();
            Box({ wp.pos.x, 0.08f, wp.pos.z }, 1.2f, 0.06f, 1.2f, C_GOLD);
        }

        // --- тени: плоская проекция на землю, трафарет не даёт теням темнеть дважды ---
        {
            float k1 = gSun.x / gSun.y, k2 = gSun.z / gSun.y, hh = 0.075f;
            float sm[16] = { 1, 0, 0, 0,  -k1, 0, -k2, 0,  0, 0, 1, 0,  k1 * hh, hh, k2 * hh, 1 };
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_EQUAL, 0, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
            glColor4f(0, 0, 0, 0.40f);
            gShadowPass = true;
            glPushMatrix();
            glMultMatrixf(sm);
            glCallList(gCityShadow);
            for (auto& e : people) if (e.alive || e.timer > 6.0f) DrawNPC(e);
            if (!gInCar && !gInHeli) DrawPerson(gPlayer, camYaw, gPhase, gAmp, C_BLACK, C_BLACK, C_BLACK, 0, gWeapon, true);
            if (gCarAlive) DrawCar(gCar);
            for (auto& pc : pcars) DrawPCar(pc);
            DrawHeli(gHeli);
            glPopMatrix();
            gShadowPass = false;
            glDisable(GL_STENCIL_TEST);
            glDepthMask(GL_TRUE);
        }

        // --- светящееся: пули, ракеты, вспышки, взрывы ---
        glDisable(GL_LIGHTING);
        for (auto& b : projs) {
            if (b.rocket) {
                Sphere(b.pos, 0.22f, 5, Rgba(120, 130, 110));
                Sphere({ b.pos.x - b.vel.x * 0.012f, b.pos.y - b.vel.y * 0.012f, b.pos.z - b.vel.z * 0.012f }, 0.3f, 5, Rgba(255, 150, 40));
            } else Box(b.pos, 0.16f, 0.16f, 0.16f, b.enemy ? Rgba(255, 90, 60) : C_YELLOW);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        if (gFlashT > 0) Sphere(gFlashPos, 0.22f, 5, Rgba(255, 220, 120, 220));
        for (auto& bm : booms) {
            float u = bm.t / 0.7f;
            Sphere(bm.pos, 1.0f + 7.0f * sqrtf(u), 10, Rgba(255, 140, 30, (int)(200 * (1 - u))));
            Sphere(bm.pos, 0.6f + 4.0f * sqrtf(u), 8, Rgba(255, 240, 160, (int)(230 * (1 - u))));
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // --- надписи на вывесках ---
        glDisable(GL_FOG);
        for (auto& s : signs)
            if (Dist2D(s.pos, eye) < 60.0f) SignText(s, fSign);

        // =================== ИНТЕРФЕЙС ===================
        glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, W, H, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float cx = W / 2.0f, cy = H / 2.0f;
        if (gHurtT > 0) Quad2(0, 0, (float)W, (float)H, Rgba(255, 0, 0, 70));
        if (zoom && gWeapon == 3) {                                          // прицел снайперки
            ScopeMask(cx, cy, H * 0.42f, (float)(W > H ? W : H) * 1.2f);
            Line2(cx - H * 0.42f, cy, cx + H * 0.42f, cy, C_BLACK);
            Line2(cx, cy - H * 0.42f, cx, cy + H * 0.42f, C_BLACK);
            Disc(cx, cy, 2, C_RED);
        } else if (!inVeh && !climbing) { RingL(cx, cy, 9, C_WHITE); Disc(cx, cy, 2, C_RED); }

        Txt(L"Рубли: " + I2W(gMoney) + L" \u20BD", 20, 12, fBig, C_GOLD);
        Quad2(20, 58, 204, 18, Rgba(0, 0, 0, 150));
        Quad2(22, 60, 2 * gHp, 14, Rgba(220, 50, 50));
        if (gInCar) {
            Quad2(20, 80, 204, 12, Rgba(0, 0, 0, 150));
            Quad2(22, 82, 2 * Clampf(gCar.hp, 0, 100), 8, C_SKY);
            Txt(I2W((int)(fabsf(gCar.speed) * 3.6f)) + L" км/ч", 20, 96, fMid, C_SKY);
        }
        if (gInHeli) Txt(L"Высота: " + I2W((int)gHeli.pos.y) + L" м   (Пробел - вверх, Shift - вниз)", 20, 96, fMid, C_SKY);
        Txt(L"Убито: " + I2W(gKills), 20, 128, fSmall, C_WHITE);

        for (int k = 0; k < 5; k++) Star2(cx + (k - 2) * 36.0f, 36.0f, 15.0f, k < gStars ? C_GOLD : Rgba(60, 60, 70, 170));   // звёзды розыска

        for (int k = 0; k < 5; k++) {                                        // список оружия
            float x = W - 230.0f, y = H - 190.0f + k * 30.0f;
            Quad2(x, y, 210, 26, k == gWeapon ? Rgba(255, 200, 0, 170) : Rgba(0, 0, 0, 130));
            Txt(I2W(k + 1) + L"  " + WEAPONS[k].name, x + 8, y + 3, fSmall, gOwned[k] ? C_WHITE : Rgba(130, 130, 130));
        }

        if (shop && shop->kind == 1) {
            std::wstring s = L"ОРУЖЕЙНЫЙ МАГАЗИН:";
            for (int k = 1; k < 5; k++) s += L"   " + I2W(k + 1) + L"-" + WEAPONS[k].name + L" " + (gOwned[k] ? std::wstring(L"(есть)") : I2W(WEAPONS[k].price) + L"\u20BD");
            Quad2(cx - 400, cy + 70, 800, 34, Rgba(0, 0, 0, 160));
            Txt(s, cx - 392, cy + 76, fSmall, C_WHITE);
        } else if (shop && shop->kind == 2) {
            Quad2(cx - 200, cy + 70, 400, 34, Rgba(0, 0, 0, 160));
            Txt(L"АПТЕКА: E - вылечиться за 100 \u20BD", cx - 190, cy + 76, fSmall, C_WHITE);
        } else if (!inVeh && !climbing) {
            if (gCarAlive && Dist2D(gPlayer, gCar.pos) < 4.5f) Txt(L"F - сесть в машину", cx - 90, cy + 60, fMid, C_WHITE);
            else if (Dist2D(gPlayer, LadderPos()) < 3.4f && gHeli.pos.y < 0.5f) Txt(L"F - залезть в вертолёт по лестнице", cx - 150, cy + 60, fMid, C_WHITE);
            else for (auto& rb : buildings) if (rcB < 0 && gPlayer.y < 1.0f && Dist2D(gPlayer, RoofLadderBase(rb, 1.0f)) < 2.6f) { Txt(L"F - залезть на крышу по лестнице", cx - 150, cy + 60, fMid, C_WHITE); break; }
        }
        if (gPopupT > 0) Txt(gPopup, cx - 120, cy - 130, fBig, C_GREEN);
        if (gRobT >= 0) {
            Txt(L"Грабим банк... не отходи от двери!", cx - 190, cy + 40, fMid, C_RED);
            Quad2(cx - 150, cy + 76, 300, 22, Rgba(0, 0, 0, 170));
            Quad2(cx - 148, cy + 78, 296 * (gRobT / 6.0f), 18, C_RED);
        } else if (bank && !inVeh) Txt(gBankCD > 0 ? L"Банк пуст - загляни позже" : L"E - ограбить банк (получишь 3 звезды)", cx - 190, cy + 60, fMid, C_WHITE);
        Txt(L"WASD - ходьба/руль/полёт | Мышь - камера | ЛКМ - огонь | ПКМ - прицел | 1-5 / колесо мыши - оружие",
            20, H - 52.0f, fSmall, C_WHITE);
        Txt(L"F - сесть/выйти (машина, вертолёт) | R - вызвать машину | Shift - бег | Пробел - прыжок (можно на капот и крышу) | Из машины стрелять нельзя | Esc - выход",
            20, H - 28.0f, fSmall, C_WHITE);

        // ---- миникарта ----
        const float S = 170.0f;
        float mx0 = W - S - 20, my0 = 20;
        auto mpx = [&](Vec3 p) { return mx0 + (0.5f - p.x / WORLD * 0.5f) * S; };
        auto mpy = [&](Vec3 p) { return my0 + (0.5f - p.z / WORLD * 0.5f) * S; };
        Quad2(mx0 - 2, my0 - 2, S + 4, S + 4, C_BLACK);
        Quad2(mx0, my0, S, S, Rgba(60, 100, 60));
        for (auto& b : buildings) {
            float w = b.size.x / (2 * WORLD) * S, h = b.size.z / (2 * WORLD) * S;
            Col bc = b.kind == 1 ? Rgba(220, 60, 60) : (b.kind == 2 ? Rgba(60, 200, 110) : (b.kind == 3 ? Rgba(70, 100, 230) : (b.kind == 5 ? C_GOLD : C_GRAY)));
            Quad2(mpx(b.pos) - w / 2, mpy(b.pos) - h / 2, w, h, bc);
        }
        for (auto& r : rubles) Quad2(mpx(r.pos) - 1, mpy(r.pos) - 1, 3, 3, C_LIME);
        for (auto& e : people)
            if (e.alive && (e.type != 0)) Disc(mpx(e.pos), mpy(e.pos), 2.5f, e.type == 1 ? C_RED : (e.type == 3 ? Rgba(0, 220, 255) : Rgba(80, 120, 255)));
        Quad2(mpx(gHeli.pos) - 3, mpy(gHeli.pos) - 3, 6, 6, C_YELLOW);
        if (gCarAlive) Quad2(mpx(gCar.pos) - 3, mpy(gCar.pos) - 3, 6, 6, C_SKY);
        for (auto& pc : pcars) Quad2(mpx(pc.pos) - 3, mpy(pc.pos) - 3, 6, 6, pc.swat ? Rgba(0, 220, 255) : Rgba(80, 120, 255));
        for (auto& wp : wpicks) Disc(mpx(wp.pos), mpy(wp.pos), 2.5f, C_GOLD);
        {
            Vec3 f = Fwd(camYaw);
            Disc(mpx(focus), mpy(focus), 3.5f, C_WHITE);
            Line2(mpx(focus), mpy(focus), mpx(focus) - f.x * 10, mpy(focus) - f.z * 10, C_WHITE);
        }

        glDisable(GL_BLEND);
        SwapBuffers(dc);
        if (!swapInterval) Sleep(2);
    }

    ShowCursor(TRUE);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(glrc);
    ReleaseDC(hwnd, dc);
    DestroyWindow(hwnd);
    return 0;
}
