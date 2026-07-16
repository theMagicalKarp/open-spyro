/* func_credits_8007BFD0 (0x8007BFD0, credits overlay).
 *
 * Credits-sequence frame draw. While the sequence gate (D_80075704) is -1,
 * only scrolls the double-height framebuffer: DrawSync/VSync, then MoveImage
 * (func_8005FAF0) of the 512x224 strip at y=8 (front env active) or y=0xF8
 * to the opposite half, and a final DrawSync. Otherwise draws a full frame:
 * copies the fade color bytes (D_80078A50..52) into both frame DRAWENVs,
 * builds one 0x58-byte sprite prim per credits line (records at D_8007589C,
 * 0x1C stride; prim cursor D_80075710 grows downward; record type 0x115 gets
 * a +0x29 glyph offset and depth 0x1C00), submits the pending sprite queue,
 * clears the OT bins, emits the static world, then runs the standard
 * 2-vblank pace loop and PutDispEnv/PutDrawEnv/DrawOTag submit.
 */

extern void func_8005F764();  /* DrawSync */
extern int func_8005DBC4();   /* VSync */
extern void func_8005FAF0();  /* MoveImage */
extern void func_80016914();  /* word fill */
extern void func_800190D4();
extern void func_80018880();
extern void func_80022A2C(void);
extern void func_80016930();
extern void func_800258F0();
extern void func_8004EBA8();
extern void func_80060030(); /* PutDispEnv */
extern void func_8005FDD8(); /* PutDrawEnv */
extern void *func_80016784(); /* LinkOTPrimitives */
extern void func_8005FD64();  /* DrawOTag */

extern int D_80075704;
extern unsigned char *D_80075888;
extern unsigned char D_80076EE0[];
extern unsigned char D_80078A50;
extern unsigned char D_80078A51;
extern unsigned char D_80078A52;
extern unsigned char D_80076EF9;
extern unsigned char D_80076EFA;
extern unsigned char D_80076EFB;
extern unsigned char D_80076F7D;
extern unsigned char D_80076F7E;
extern unsigned char D_80076F7F;
extern int D_800756E4;
extern char *D_80075710;
extern char *D_8007589C;
extern int D_80075918;
extern int D_800720F4[];
extern int D_800785D0;
extern int D_80076DE4[];
extern int D_80075784;
extern int D_80075950;
extern int D_80075954[];

void func_credits_8007BFD0(void) {
  int i;

  if (D_80075704 == -1) {
    struct {
      short x, y, w, h;
    } rect;
    int y;
    func_8005F764(0);
    func_8005DBC4(0);
    y = 8;
    rect.x = 0;
    if (D_80075888 != D_80076EE0) {
      y = 0xF8;
    }
    rect.w = 0x200;
    rect.y = y;
    rect.h = 0xE0;
    func_8005FAF0(&rect, 0, 0x100 - y);
    func_8005F764(0);
    return;
  }
  {
    int r = D_80078A50;
    int g = D_80078A51;
    int b = D_80078A52;
    D_80076EF9 = r;
    D_80076EFA = g;
    D_80076EFB = b;
    D_80076F7D = r;
    D_80076F7E = g;
    D_80076F7F = b;
  }
  for (i = 0; i < D_800756E4; i++) {
    {
      char *rec;
      char *prim = D_80075710 - 0x58;
      D_80075710 = prim;
      rec = D_8007589C + i * 0x1C;
      func_80016914(prim, 0, 0x58);
      *(int *)(D_80075710 + 0xC) = *(short *)(rec + 0x12);
      *(int *)(D_80075710 + 0x10) = *(short *)(rec + 0x14);
      *(int *)(D_80075710 + 0x14) = *(short *)(rec + 0x16);
      *(unsigned char *)(D_80075710 + 0x44) = rec[5];
      if (*(short *)(rec + 0x1A) == 0x115) {
        *(unsigned char *)(D_80075710 + 0x44) += 0x29;
        *(int *)(D_80075710 + 0x14) = 0x1C00;
      }
      *(unsigned char *)(D_80075710 + 0x45) = rec[6];
      *(unsigned char *)(D_80075710 + 0x46) = rec[7];
      *(unsigned short *)(D_80075710 + 0x36) = *(unsigned short *)(rec + 0x1A);
      *(unsigned char *)(D_80075710 + 0x4F) = 0xB;
      *(unsigned char *)(D_80075710 + 0x50) = 0xFF;
      *(unsigned char *)(D_80075710 + 0x47) = 0x7F;
    }
  }
  {
    int n = D_80075918;
    if (n != 0) {
      int s = n << 4;
      func_800190D4(2, s, s, s);
    }
  }
  {
    int *q = D_800720F4;
    char *ot;
    q[0] = 0;
    func_80018880();
    ot = (char *)q - 0x2400;
    func_80016914(ot, 0, 0x900);
    func_80022A2C();
    func_80016930(ot, 0, 0x1C00);
    D_800785D0 = 0x28000;
    func_800258F0(-1);
    func_8004EBA8(-1, D_80076DE4, (char *)D_80076DE4 - 0x14);
  }
  func_8005F764(0);
  if (D_80075784 != 0) {
    func_8005DBC4(0);
  }
  {
    int *end;
    *(end = D_80075954) = func_8005DBC4(-1);
    if (end[0] - D_80075950 < 2) {
      int *p = end;
      do {
        func_8005DBC4(0);
        p[0] = func_8005DBC4(-1);
      } while (p[0] - p[-1] < 2);
    }
  }
  D_80075950 = func_8005DBC4(-1);
  func_80060030(D_80075888 + 0x5C);
  func_8005FDD8(D_80075888);
  func_8005FD64(func_80016784(0x800));
}
