/* src/native/handasm/runtime_test.c — unit tests for the hand-asm runtime
 * (runtime.c, handasm.h) and the register-only hand-asm leaves (make
 * native-test).
 *
 * The GTE is a fake register file in place of PsyZ's, so the tests see what
 * the runtime's own GTE layer does. Hand-asm memory access needs PS1
 * addresses to be host addresses (wasm32), so only clusters that touch no
 * memory run here. */

#include <stdint.h>

#include "handasm.h"
#include "test.h"

/* --- fake PsyZ GTE ------------------------------------------------------- */

static uint32_t gte_data[32], gte_ctrl[32], gte_last_cmd;

unsigned int Psyz_GteDataRead(unsigned reg) { return gte_data[reg]; }
void Psyz_GteDataWrite(unsigned reg, unsigned int v) { gte_data[reg] = v; }
unsigned int Psyz_GteCtrlRead(unsigned reg) { return gte_ctrl[reg]; }
void Psyz_GteCtrlWrite(unsigned reg, unsigned int v) { gte_ctrl[reg] = v; }
void Psyz_GteCommand(unsigned int cmd) { gte_last_cmd = cmd; }

static void gte_reset(void) {
  for (int i = 0; i < 32; i++) {
    gte_data[i] = gte_ctrl[i] = 0;
  }
  gte_last_cmd = 0;
}

/* The generated C-ABI wrappers (ha_<Name>.c). */
uint32_t AbsAngleDelta8(uint32_t a, uint32_t b);
uint32_t AbsAngleDelta12(uint32_t a, uint32_t b);
uint32_t SignedAngleDelta8(uint32_t a, uint32_t b);
uint32_t SignedAngleDelta12(uint32_t a, uint32_t b);
uint32_t ApproachAngle8(uint32_t target, uint32_t cur, uint32_t step,
                        uint32_t lim);
uint32_t CountLeadingZeros(uint32_t x);

/* --- ha_enter ------------------------------------------------------------ */

TEST(handasm_enter_sets_abi_registers) {
  HaRegs R;

  for (int i = 0; i < 32; i++) {
    R.r[i] = 0xDEADBEEF;
  }
  R.hi = R.lo = 0xDEADBEEF;
  ha_enter(&R);
  CHECK_INT(R.r[0], 0);
  CHECK_INT(R.r[4], 0);
  CHECK_INT(R.hi, 0);
  CHECK_INT(R.lo, 0);
  CHECK_INT(R.r[28], 0x80075264u); /* $gp */
  CHECK_INT(R.r[31], HA_RET_MAGIC);
  CHECK(ha_mips_sp != 0);
  CHECK_INT(R.r[29], ha_mips_sp - 0x20); /* argument home space */
}

/* --- GTE side effects ---------------------------------------------------- */

TEST(handasm_gte_sxyp_pushes_fifo) {
  gte_reset();
  ha_gte_write(15, 0x11);
  ha_gte_write(15, 0x22);
  ha_gte_write(15, 0x33);
  CHECK_INT(gte_data[12], 0x11); /* SXY0 */
  CHECK_INT(gte_data[13], 0x22); /* SXY1 */
  CHECK_INT(gte_data[14], 0x33); /* SXY2 */
  CHECK_INT(ha_gte_read(15), 0x33);
  gte_data[14] = 0x44;
  CHECK_INT(ha_gte_read(15), 0x44); /* SXYP reads SXY2 */
}

TEST(handasm_gte_irgb_sets_ir) {
  gte_reset();
  ha_gte_write(28, 0x1F | 0x10 << 5 | 0x01 << 10);
  CHECK_INT(gte_data[9], 0x1F << 7);
  CHECK_INT(gte_data[10], 0x10 << 7);
  CHECK_INT(gte_data[11], 0x01 << 7);
  CHECK_INT(gte_data[28], 0); /* not stored as a register */
}

TEST(handasm_gte_orgb_saturates_ir) {
  gte_reset();
  gte_data[9] = 0x0A << 7;
  gte_data[10] = (uint32_t)-0x100; /* negative: 0 */
  gte_data[11] = 0x7FFF;           /* above 0x1F << 7: 0x1F */
  CHECK_INT(ha_gte_read(29), 0x0A | 0x00 << 5 | 0x1F << 10);
  CHECK_INT(ha_gte_read(28), ha_gte_read(29)); /* IRGB reads as ORGB */
  ha_gte_write(28, 0x0C63);
  CHECK_INT(ha_gte_read(29), 0x0C63); /* round trip */
}

TEST(handasm_gte_lzcr) {
  static const struct {
    uint32_t lzcs, lzcr;
  } cases[] = {
      {0, 32},          {1, 31},         {0x0000FFFF, 16}, {0x7FFFFFFF, 1},
      {0xFFFFFFFF, 32}, {0x80000000, 1}, {0xFFFF0000, 16}, {0xFFFFFFFE, 31},
  };

  gte_reset();
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    ha_gte_write(30, cases[i].lzcs);
    CHECK_INT(ha_gte_read(30), cases[i].lzcs);
    CHECK_INT(ha_gte_read(31), cases[i].lzcr);
  }
}

TEST(handasm_gte_read_only_registers) {
  gte_reset();
  ha_gte_write(30, 1);
  ha_gte_write(31, 0);  /* LZCR */
  ha_gte_write(29, 99); /* ORGB */
  CHECK_INT(ha_gte_read(31), 31);
  CHECK_INT(gte_data[29], 0);
  CHECK_INT(gte_data[31], 0);
}

TEST(handasm_gte_passthrough) {
  gte_reset();
  ha_gte_write(0, 0x12345678);
  CHECK_INT(gte_data[0], 0x12345678);
  CHECK_INT(ha_gte_read(0), 0x12345678);
  ha_gte_ctrl_write(26, 0x200); /* H */
  CHECK_INT(gte_ctrl[26], 0x200);
  CHECK_INT(ha_gte_ctrl_read(26), 0x200);
  ha_gte_command(0x00180001); /* RTPS */
  CHECK_INT(gte_last_cmd, 0x00180001);
}

/* --- R3000 multiply / divide (handasm.h) --------------------------------- */

/* Runs an HA_ mult/div macro on (a, b); returns hi:lo. */
#define HILO(op, a, b)                                                         \
  ({                                                                           \
    uint32_t hi, lo;                                                           \
    op(a, b);                                                                  \
    (uint64_t)hi << 32 | lo;                                                   \
  })

TEST(handasm_mult) {
  CHECK_INT(HILO(HA_MULT, 3u, (uint32_t)-4), (uint64_t)-12);
  CHECK_INT(HILO(HA_MULT, 0x10000u, 0x10000u), 1ull << 32);
  CHECK_INT(HILO(HA_MULTU, 0xFFFFFFFFu, 2u), 0x1FFFFFFFEull);
  CHECK_INT(HILO(HA_MULT, 0xFFFFFFFFu, 2u), (uint64_t)-2);
}

TEST(handasm_div) {
  CHECK_INT(HILO(HA_DIV, (uint32_t)-7, 2u),
            (uint64_t)(uint32_t)-1 << 32 | (uint32_t)-3);
  CHECK_INT(HILO(HA_DIVU, 7u, 2u), 1ull << 32 | 3);
}

TEST(handasm_div_r3000_edge_cases) {
  /* x / 0: lo = -1 (x >= 0) or 1 (x < 0), hi = x */
  CHECK_INT(HILO(HA_DIV, 5u, 0u), 5ull << 32 | 0xFFFFFFFFu);
  CHECK_INT(HILO(HA_DIV, (uint32_t)-5, 0u), (uint64_t)(uint32_t)-5 << 32 | 1);
  CHECK_INT(HILO(HA_DIVU, 5u, 0u), 5ull << 32 | 0xFFFFFFFFu);
  /* INT_MIN / -1 */
  CHECK_INT(HILO(HA_DIV, 0x80000000u, 0xFFFFFFFFu), 0x80000000u);
}

/* --- register-only leaves ------------------------------------------------ */

TEST(handasm_abs_angle_delta) {
  CHECK_INT(AbsAngleDelta8(0x10, 0xF0), 0x20);
  CHECK_INT(AbsAngleDelta8(0xF0, 0x10), 0x20);
  CHECK_INT(AbsAngleDelta8(0x00, 0x80), 0x80);
  CHECK_INT(AbsAngleDelta8(0x40, 0x40), 0);
  CHECK_INT(AbsAngleDelta12(0x100, 0xF00), 0x200);
  CHECK_INT(AbsAngleDelta12(0xF00, 0x100), 0x200);
  CHECK_INT(AbsAngleDelta12(0, 0x800), 0x800);
}

TEST(handasm_signed_angle_delta) {
  CHECK_INT((int32_t)SignedAngleDelta8(0x10, 0xF0), 0x20);
  CHECK_INT((int32_t)SignedAngleDelta8(0xF0, 0x10), -0x20);
  CHECK_INT((int32_t)SignedAngleDelta8(0x30, 0x10), 0x20);
  CHECK_INT((int32_t)SignedAngleDelta12(0x100, 0xF00), 0x200);
  CHECK_INT((int32_t)SignedAngleDelta12(0xF00, 0x100), -0x200);
  CHECK_INT((int32_t)SignedAngleDelta12(0x123, 0x123), 0);
}

TEST(handasm_approach_angle8) {
  /* steps cur toward target the short way, wrapping */
  CHECK_INT(ApproachAngle8(0x40, 0x10, 4, 8), 0x14);
  CHECK_INT(ApproachAngle8(0x10, 0x40, 4, 8), 0x3C);
  CHECK_INT(ApproachAngle8(0x02, 0xFE, 4, 2), 0x02);
  CHECK_INT(ApproachAngle8(0xFE, 0x02, 4, 2), 0xFE);
  /* within lim of target: cur is returned unchanged */
  CHECK_INT(ApproachAngle8(0x40, 0x3D, 4, 8), 0x3D);
  CHECK_INT(ApproachAngle8(0x3D, 0x40, 4, 8), 0x40);
}

TEST(handasm_count_leading_zeros) {
  gte_reset();
  CHECK_INT(CountLeadingZeros(0), 0);
  CHECK_INT(CountLeadingZeros(1), 0);
  CHECK_INT(CountLeadingZeros(0x10), 4);
  CHECK_INT(CountLeadingZeros(0x7FFFFFFF), 30);
}
