/* Differential oracle for the 68000 core.
 *
 * Executes every one of the 65536 opcode words from an identical
 * starting state against a bus that answers the same way forever, and
 * folds everything the instruction did - each read, each write, each
 * interrupt acknowledgement, the register file afterwards and the
 * cycles it billed - into one digest per opcode. Those digests are
 * then folded into one digest for the run, which is what golden holds.
 *
 * The bus is stateless on purpose: reads outside the vector table and
 * the code window are a fixed function of the address, and writes are
 * recorded rather than stored. Nothing an opcode does can reach the
 * next opcode, so the run has no order dependence and no accumulated
 * state to explain away.
 *
 * A change that is meant to preserve behaviour reproduces the digest.
 * A change that is meant to alter it - one encoding, say - will not,
 * and --dump writes the per-opcode lines so two builds can be diffed
 * to see exactly which encodings moved.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "m68k.h"

#define CODE_BASE  0x001000u
#define VEC_TOP    0x000400u

static uint8_t vectors[VEC_TOP];
static uint8_t code[0x20];
static uint64_t trace;

static uint64_t mix(uint64_t h, uint64_t v)
{
   return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

static void hash_step(uint64_t v)
{
   trace = mix(trace, v);
}

/* Fixed filler so every address outside the vector table and the code
   window reads back the same value on every run of every build. */
static uint8_t filler(uint32_t a)
{
   uint32_t h = a * 2654435761u;
   h ^= h >> 15;
   return (uint8_t)(h >> 7);
}

static uint8_t rd8(uint32_t a)
{
   a &= 0x00ffffffu;
   if (a < VEC_TOP)
      return vectors[a];
   if (a >= CODE_BASE && a < CODE_BASE + sizeof(code))
      return code[a - CODE_BASE];
   return filler(a);
}

uint32_t m68k_read_memory_8(uint32_t a)
{
   hash_step(0x11u); hash_step(a);
   return rd8(a);
}

uint32_t m68k_read_memory_16(uint32_t a)
{
   hash_step(0x22u); hash_step(a);
   return ((uint32_t)rd8(a) << 8) | rd8(a + 1);
}

uint32_t m68k_read_memory_32(uint32_t a)
{
   hash_step(0x33u); hash_step(a);
   return ((uint32_t)rd8(a) << 24) | ((uint32_t)rd8(a + 1) << 16)
        | ((uint32_t)rd8(a + 2) << 8) | rd8(a + 3);
}

uint32_t m68k_read_disassembler_8(uint32_t a)
{
   return rd8(a);
}

uint32_t m68k_read_disassembler_16(uint32_t a)
{
   return ((uint32_t)rd8(a) << 8) | rd8(a + 1);
}

uint32_t m68k_read_disassembler_32(uint32_t a)
{
   return ((uint32_t)rd8(a) << 24) | ((uint32_t)rd8(a + 1) << 16)
        | ((uint32_t)rd8(a + 2) << 8) | rd8(a + 3);
}

/* Writes are recorded, never stored: the trace is the observable. */
void m68k_write_memory_8(uint32_t a, uint32_t v)
{
   hash_step(0x81u); hash_step(a); hash_step(v & 0xffu);
}

void m68k_write_memory_16(uint32_t a, uint32_t v)
{
   hash_step(0x82u); hash_step(a); hash_step(v & 0xffffu);
}

void m68k_write_memory_32(uint32_t a, uint32_t v)
{
   hash_step(0x84u); hash_step(a); hash_step(v);
}

void m68k_write_memory_32_pd(uint32_t a, uint32_t v)
{
   hash_step(0x88u); hash_step(a); hash_step(v);
}

/* The two hooks m68kconf.h names. */
int neocd_get_vector(int level)
{
   hash_step(0xacu); hash_step((uint32_t)level);
   return M68K_INT_ACK_AUTOVECTOR;
}

int neocd_illegal_handler(int opcode)
{
   hash_step(0x1cu); hash_step((uint32_t)opcode);
   return 0;
}

static const m68k_register_t dumped[] = {
   M68K_REG_D0, M68K_REG_D1, M68K_REG_D2, M68K_REG_D3,
   M68K_REG_D4, M68K_REG_D5, M68K_REG_D6, M68K_REG_D7,
   M68K_REG_A0, M68K_REG_A1, M68K_REG_A2, M68K_REG_A3,
   M68K_REG_A4, M68K_REG_A5, M68K_REG_A6, M68K_REG_A7,
   M68K_REG_PC, M68K_REG_SR, M68K_REG_USP, M68K_REG_ISP,
   M68K_REG_PPC
};

#define NUM_DUMPED (sizeof(dumped) / sizeof(dumped[0]))

int main(int argc, char** argv)
{
   uint64_t run  = 0xcbf29ce484222325ULL;
   int      dump = (argc > 1 && !strcmp(argv[1], "--dump"));
   unsigned op;
   unsigned i;

   /* Every vector lands somewhere harmless and even. */
   for (i = 0; i < VEC_TOP; i += 4)
   {
      vectors[i + 0] = 0x00;
      vectors[i + 1] = 0x00;
      vectors[i + 2] = 0x30;
      vectors[i + 3] = 0x00;
   }
   /* Reset stack pointer and reset program counter. */
   vectors[0] = 0x00; vectors[1] = 0x00; vectors[2] = 0x80; vectors[3] = 0x00;
   vectors[4] = 0x00; vectors[5] = 0x00; vectors[6] = 0x10; vectors[7] = 0x00;

   m68k_init();
   m68k_set_cpu_type(M68K_CPU_TYPE_68000);

   for (op = 0; op < 0x10000u; op++)
   {
      int cycles;

      /* The opcode word, then a fixed pattern, so any encoding that
         takes extension words takes the same ones every time. */
      code[0] = (uint8_t)(op >> 8);
      code[1] = (uint8_t)(op & 0xff);
      for (i = 2; i < sizeof(code); i++)
         code[i] = (uint8_t)(0x40 + i * 7);

      m68k_pulse_reset();
      /* m68k_execute() will not run while the reset cycles are still
         owed - it returns them instead - so spend them first. */
      m68k_execute(0);

      for (i = 0; i < 8; i++)
         m68k_set_reg((m68k_register_t)(M68K_REG_D0 + i),
                      0x01020304u * (i + 1) + 0x5a5au);
      for (i = 0; i < 7; i++)
         m68k_set_reg((m68k_register_t)(M68K_REG_A0 + i),
                      0x00004000u + i * 0x40u);
      m68k_set_reg(M68K_REG_SR, 0x2000u);
      m68k_set_reg(M68K_REG_SP, 0x00008000u);
      m68k_set_reg(M68K_REG_PC, CODE_BASE);

      trace  = 0xcbf29ce484222325ULL;
      cycles = m68k_execute(1);

      for (i = 0; i < NUM_DUMPED; i++)
         hash_step(m68k_get_reg(NULL, dumped[i]));

      if (dump)
         printf("%04x %5d %016llx\n", op, cycles,
                (unsigned long long)trace);

      run = mix(run, trace);
      run = mix(run, (uint64_t)(int64_t)cycles);
   }

   if (!dump)
      printf("%016llx\n", (unsigned long long)run);
   return 0;
}
