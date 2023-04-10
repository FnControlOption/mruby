#pragma clang diagnostic ignored "-Wundefined-internal"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-label"

/*
** codegen.c - mruby code generator
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/proc.h>
#include <mruby/dump.h>
#include <mruby/numeric.h>
#include <mruby/string.h>
#include <mruby/debug.h>
#include <mruby/presym.h>
#if 0
#include "node.h"
#endif
#include <mruby/opcode.h>
#include <mruby/re.h>
#include <mruby/throw.h>
#include <ctype.h>
#include <string.h>
#include <mruby/internal.h>
#include <yarp.h>
#include <yarp/ast.h>
MRB_INLINE mrb_sym yarp_sym(mrb_state *mrb, yp_token_t token)
{ return mrb_intern(mrb, token.start, token.end - token.start); }
MRB_INLINE mrb_sym yarp_sym2(mrb_state *mrb, yp_location_t location)
{ return mrb_intern(mrb, location.start, location.end - location.start); }
MRB_INLINE mrb_sym yarp_sym3(mrb_state *mrb, const yp_string_t *string)
{ return mrb_intern(mrb, yp_string_source(string), yp_string_length(string)); }
MRB_INLINE mrb_bool yarp_safe_call_p(yp_call_node_t *node)
{ return node->call_operator.type == YP_TOKEN_AMPERSAND_DOT; }
MRB_INLINE yp_token_t yarp_keyword_parameter_name(yp_keyword_parameter_node_t *kwd)
{ return (yp_token_t){.type = kwd->name.type, .start = kwd->name.start, .end = kwd->name.end - 1}; }

#ifndef MRB_CODEGEN_LEVEL_MAX
#define MRB_CODEGEN_LEVEL_MAX 256
#endif

#define MAXARG_S (1<<16)

typedef mrb_ast_node node;
typedef struct mrb_parser_state parser_state;

enum looptype {
  LOOP_NORMAL,
  LOOP_BLOCK,
  LOOP_FOR,
  LOOP_BEGIN,
  LOOP_RESCUE,
};

struct loopinfo {
  enum looptype type;
  uint32_t pc0;                 /* `next` destination */
  uint32_t pc1;                 /* `redo` destination */
  uint32_t pc2;                 /* `break` destination */
  int reg;                      /* destination register */
  struct loopinfo *prev;
};

typedef struct scope {
  mrb_state *mrb;
  mrbc_context *cxt;
  mrb_pool *mpool;

  struct scope *prev;

  mrb_sym *lv;
  size_t lvsize;

  uint16_t sp;
  uint32_t pc;
  uint32_t lastpc;
  uint32_t lastlabel;
  uint16_t ainfo:15;
  mrb_bool mscope:1;

  struct loopinfo *loop;
#if 0
  mrb_sym filename_sym;
  uint16_t lineno;
#endif

  mrb_code *iseq;
#if 0
  uint16_t *lines;
#endif
  uint32_t icapa;

  mrb_irep *irep;
  mrb_pool_value *pool;
  mrb_sym *syms;
  mrb_irep **reps;
  struct mrb_irep_catch_handler *catch_table;
  uint32_t pcapa, scapa, rcapa;

  uint16_t nlocals;
  uint16_t nregs;
  int ai;

#if 0
  int debug_start_pos;
  uint16_t filename_index;
#endif
  yp_parser_t* parser;

  int rlev;                     /* recursion levels */
} codegen_scope;

static codegen_scope* scope_new(mrb_state *mrb, mrbc_context *cxt, codegen_scope *prev, mrb_sym *lv, size_t lvsize);
static void scope_finish(codegen_scope *s);
static struct loopinfo *loop_push(codegen_scope *s, enum looptype t);
static void loop_break(codegen_scope *s, node *tree);
static void loop_pop(codegen_scope *s, int val);

/*
 * The search for catch handlers starts at the end of the table in mrb_vm_run().
 * Therefore, the next handler to be added must meet one of the following conditions.
 * - Larger start position
 * - Same start position but smaller end position
 */
static int catch_handler_new(codegen_scope *s);
static void catch_handler_set(codegen_scope *s, int ent, enum mrb_catch_type type, uint32_t begin, uint32_t end, uint32_t target);

static void gen_assignment(codegen_scope *s, yp_node_t *node, yp_node_t *rhs, int sp, int val);
static void gen_massignment(codegen_scope *s, yp_node_list_t targets, int sp, int val);

static void codegen(codegen_scope *s, yp_node_t *node, int val);
static void raise_error(codegen_scope *s, const char *msg);

static void
codegen_error(codegen_scope *s, const char *message)
{
  if (!s) return;
#ifndef MRB_NO_STDIO
#if 0
  if (s->filename_sym && s->lineno) {
    const char *filename = mrb_sym_name_len(s->mrb, s->filename_sym, NULL);
    fprintf(stderr, "%s:%d: %s\n", filename, s->lineno, message);
  }
  else {
#endif
    fprintf(stderr, "%s\n", message);
#if 0
  }
#endif
#endif
  while (s->prev) {
    codegen_scope *tmp = s->prev;
    if (s->irep) {
      mrb_free(s->mrb, s->iseq);
      for (int i=0; i<s->irep->plen; i++) {
        mrb_pool_value *pv = &s->pool[i];
        if ((pv->tt & 0x3) == IREP_TT_STR || pv->tt == IREP_TT_BIGINT) {
          mrb_free(s->mrb, (void*)pv->u.str);
        }
      }
      mrb_free(s->mrb, s->pool);
      mrb_free(s->mrb, s->syms);
      mrb_free(s->mrb, s->catch_table);
      if (s->reps) {
        /* copied from mrb_irep_free() in state.c */
        for (int i=0; i<s->irep->rlen; i++) {
          if (s->reps[i])
            mrb_irep_decref(s->mrb, (mrb_irep*)s->reps[i]);
        }
        mrb_free(s->mrb, s->reps);
      }
#if 0
      mrb_free(s->mrb, s->lines);
#endif
    }
    mrb_pool_close(s->mpool);
    s = tmp;
  }
  MRB_THROW(s->mrb->jmp);
}

static void*
codegen_palloc(codegen_scope *s, size_t len)
{
  void *p = mrb_pool_alloc(s->mpool, len);

  if (!p) codegen_error(s, "pool memory allocation");
  return p;
}

static void*
codegen_realloc(codegen_scope *s, void *p, size_t len)
{
  p = mrb_realloc_simple(s->mrb, p, len);

  if (!p && len > 0) codegen_error(s, "mrb_realloc");
  return p;
}

static void
check_no_ext_ops(codegen_scope *s, uint16_t a, uint16_t b)
{
  if (s->cxt->no_ext_ops && (a | b) > 0xff) {
    codegen_error(s, "need OP_EXTs instruction (currently OP_EXTs are prohibited)");
  }
}

static int
new_label(codegen_scope *s)
{
  return s->lastlabel = s->pc;
}

static void
emit_B(codegen_scope *s, uint32_t pc, uint8_t i)
{
  if (pc >= s->icapa) {
    if (pc == UINT32_MAX) {
      codegen_error(s, "too big code block");
    }
    if (pc >= UINT32_MAX / 2) {
      pc = UINT32_MAX;
    }
    else {
      s->icapa *= 2;
    }
    s->iseq = (mrb_code*)codegen_realloc(s, s->iseq, sizeof(mrb_code)*s->icapa);
#if 0
    if (s->lines) {
      s->lines = (uint16_t*)codegen_realloc(s, s->lines, sizeof(uint16_t)*s->icapa);
    }
#endif
  }
#if 0
  if (s->lines) {
    if (s->lineno > 0 || pc == 0)
      s->lines[pc] = s->lineno;
    else
      s->lines[pc] = s->lines[pc-1];
  }
#endif
  s->iseq[pc] = i;
}

static void
emit_S(codegen_scope *s, int pc, uint16_t i)
{
  uint8_t hi = i>>8;
  uint8_t lo = i&0xff;

  emit_B(s, pc,   hi);
  emit_B(s, pc+1, lo);
}

static void
gen_B(codegen_scope *s, uint8_t i)
{
  emit_B(s, s->pc, i);
  s->pc++;
}

static void
gen_S(codegen_scope *s, uint16_t i)
{
  emit_S(s, s->pc, i);
  s->pc += 2;
}

static void
genop_0(codegen_scope *s, mrb_code i)
{
  s->lastpc = s->pc;
  gen_B(s, i);
}

static void
genop_1(codegen_scope *s, mrb_code i, uint16_t a)
{
  s->lastpc = s->pc;
  check_no_ext_ops(s, a, 0);
  if (a > 0xff) {
    gen_B(s, OP_EXT1);
    gen_B(s, i);
    gen_S(s, a);
  }
  else {
    gen_B(s, i);
    gen_B(s, (uint8_t)a);
  }
}

static void
genop_2(codegen_scope *s, mrb_code i, uint16_t a, uint16_t b)
{
  s->lastpc = s->pc;
  check_no_ext_ops(s, a, b);
  if (a > 0xff && b > 0xff) {
    gen_B(s, OP_EXT3);
    gen_B(s, i);
    gen_S(s, a);
    gen_S(s, b);
  }
  else if (b > 0xff) {
    gen_B(s, OP_EXT2);
    gen_B(s, i);
    gen_B(s, (uint8_t)a);
    gen_S(s, b);
  }
  else if (a > 0xff) {
    gen_B(s, OP_EXT1);
    gen_B(s, i);
    gen_S(s, a);
    gen_B(s, (uint8_t)b);
  }
  else {
    gen_B(s, i);
    gen_B(s, (uint8_t)a);
    gen_B(s, (uint8_t)b);
  }
}

static void
genop_3(codegen_scope *s, mrb_code i, uint16_t a, uint16_t b, uint16_t c)
{
  genop_2(s, i, a, b);
  gen_B(s, (uint8_t)c);
}

static void
genop_2S(codegen_scope *s, mrb_code i, uint16_t a, uint16_t b)
{
  genop_1(s, i, a);
  gen_S(s, b);
}

static void
genop_2SS(codegen_scope *s, mrb_code i, uint16_t a, uint32_t b)
{
  genop_1(s, i, a);
  gen_S(s, b>>16);
  gen_S(s, b&0xffff);
}

static void
genop_W(codegen_scope *s, mrb_code i, uint32_t a)
{
  uint8_t a1 = (a>>16) & 0xff;
  uint8_t a2 = (a>>8) & 0xff;
  uint8_t a3 = a & 0xff;

  s->lastpc = s->pc;
  gen_B(s, i);
  gen_B(s, a1);
  gen_B(s, a2);
  gen_B(s, a3);
}

#define NOVAL  0
#define VAL    1

static mrb_bool
no_optimize(codegen_scope *s)
{
  if (s && s->cxt && s->cxt->no_optimize)
    return TRUE;
  return FALSE;
}

#if 0
struct mrb_insn_data
mrb_decode_insn(const mrb_code *pc)
{
  struct mrb_insn_data data = { 0 };
  if (pc == 0) return data;
  data.addr = pc;
  mrb_code insn = READ_B();
  uint16_t a = 0;
  uint16_t b = 0;
  uint16_t c = 0;

  switch (insn) {
#define FETCH_Z() /* empty */
#define OPCODE(i,x) case OP_ ## i: FETCH_ ## x (); break;
#include "mruby/ops.h"
#undef OPCODE
  }
  switch (insn) {
  case OP_EXT1:
    insn = READ_B();
    switch (insn) {
#define OPCODE(i,x) case OP_ ## i: FETCH_ ## x ## _1 (); break;
#include "mruby/ops.h"
#undef OPCODE
    }
    break;
  case OP_EXT2:
    insn = READ_B();
    switch (insn) {
#define OPCODE(i,x) case OP_ ## i: FETCH_ ## x ## _2 (); break;
#include "mruby/ops.h"
#undef OPCODE
    }
    break;
  case OP_EXT3:
    insn = READ_B();
    switch (insn) {
#define OPCODE(i,x) case OP_ ## i: FETCH_ ## x ## _3 (); break;
#include "mruby/ops.h"
#undef OPCODE
    }
    break;
  default:
    break;
  }
  data.insn = insn;
  data.a = a;
  data.b = b;
  data.c = c;
  return data;
}
#endif

#undef OPCODE
#define Z 1
#define S 3
#define W 4
#define OPCODE(_,x) x,
/* instruction sizes */
static uint8_t mrb_insn_size[] = {
#define B 2
#define BB 3
#define BBB 4
#define BS 4
#define BSS 6
#include "mruby/ops.h"
#undef B
#undef BB
#undef BBB
#undef BS
#undef BSS
};
/* EXT1 instruction sizes */
static uint8_t mrb_insn_size1[] = {
#define B 3
#define BB 4
#define BBB 5
#define BS 5
#define BSS 7
#include "mruby/ops.h"
#undef B
#undef BS
#undef BSS
};
/* EXT2 instruction sizes */
static uint8_t mrb_insn_size2[] = {
#define B 2
#define BS 4
#define BSS 6
#include "mruby/ops.h"
#undef B
#undef BB
#undef BBB
#undef BS
#undef BSS
};
/* EXT3 instruction sizes */
#define B 3
#define BB 5
#define BBB 6
#define BS 5
#define BSS 7
static uint8_t mrb_insn_size3[] = {
#include "mruby/ops.h"
};
#undef B
#undef BB
#undef BBB
#undef BS
#undef BSS
#undef OPCODE

static const mrb_code*
mrb_prev_pc(codegen_scope *s, const mrb_code *pc)
{
  const mrb_code *prev_pc = NULL;
  const mrb_code *i = s->iseq;

  while (i<pc) {
    uint8_t insn = i[0];
    prev_pc = i;
    switch (insn) {
    case OP_EXT1:
      i += mrb_insn_size1[i[1]] + 1;
      break;
    case OP_EXT2:
      i += mrb_insn_size2[i[1]] + 1;
      break;
    case OP_EXT3:
      i += mrb_insn_size3[i[1]] + 1;
      break;
    default:
      i += mrb_insn_size[insn];
      break;
    }
  }
  return prev_pc;
}

#define pc_addr(s) &((s)->iseq[(s)->pc])
#define addr_pc(s, addr) (uint32_t)((addr) - s->iseq)
#define rewind_pc(s) s->pc = s->lastpc

static struct mrb_insn_data
mrb_last_insn(codegen_scope *s)
{
  if (s->pc == 0) {
    struct mrb_insn_data data = { OP_NOP, 0 };
    return data;
  }
  return mrb_decode_insn(&s->iseq[s->lastpc]);
}

static mrb_bool
no_peephole(codegen_scope *s)
{
  return no_optimize(s) || s->lastlabel == s->pc || s->pc == 0 || s->pc == s->lastpc;
}

#define JMPLINK_START UINT32_MAX

static void
gen_jmpdst(codegen_scope *s, uint32_t pc)
{

  if (pc == JMPLINK_START) {
    pc = 0;
  }
  uint32_t pos2 = s->pc+2;
  int32_t off = pc - pos2;

  if (off > INT16_MAX || INT16_MIN > off) {
    codegen_error(s, "too big jump offset");
  }
  gen_S(s, (uint16_t)off);
}

static uint32_t
genjmp(codegen_scope *s, mrb_code i, uint32_t pc)
{
  uint32_t pos;

  genop_0(s, i);
  pos = s->pc;
  gen_jmpdst(s, pc);
  return pos;
}

#define genjmp_0(s,i) genjmp(s,i,JMPLINK_START)

static uint32_t
genjmp2(codegen_scope *s, mrb_code i, uint16_t a, uint32_t pc, int val)
{
  uint32_t pos;

  if (!no_peephole(s) && !val) {
    struct mrb_insn_data data = mrb_last_insn(s);

    switch (data.insn) {
    case OP_MOVE:
      if (data.a == a && data.a > s->nlocals) {
        rewind_pc(s);
        a = data.b;
      }
      break;
    case OP_LOADNIL:
    case OP_LOADF:
      if (data.a == a || data.a > s->nlocals) {
        s->pc = addr_pc(s, data.addr);
        if (i == OP_JMPNOT || (i == OP_JMPNIL && data.insn == OP_LOADNIL)) {
          return genjmp(s, OP_JMP, pc);
        }
        else {                  /* OP_JMPIF */
          return JMPLINK_START;
        }
      }
      break;
    case OP_LOADT: case OP_LOADI: case OP_LOADINEG: case OP_LOADI__1:
    case OP_LOADI_0: case OP_LOADI_1: case OP_LOADI_2: case OP_LOADI_3:
    case OP_LOADI_4: case OP_LOADI_5: case OP_LOADI_6: case OP_LOADI_7:
      if (data.a == a || data.a > s->nlocals) {
        s->pc = addr_pc(s, data.addr);
        if (i == OP_JMPIF) {
          return genjmp(s, OP_JMP, pc);
        }
        else {                  /* OP_JMPNOT and OP_JMPNIL */
          return JMPLINK_START;
        }
      }
      break;
    }
  }

  if (a > 0xff) {
    check_no_ext_ops(s, a, 0);
    gen_B(s, OP_EXT1);
    genop_0(s, i);
    gen_S(s, a);
  }
  else {
    genop_0(s, i);
    gen_B(s, (uint8_t)a);
  }
  pos = s->pc;
  gen_jmpdst(s, pc);
  return pos;
}

#define genjmp2_0(s,i,a,val) genjmp2(s,i,a,JMPLINK_START,val)

static mrb_bool get_int_operand(codegen_scope *s, struct mrb_insn_data *data, mrb_int *ns);
static void gen_int(codegen_scope *s, uint16_t dst, mrb_int i);

static void
gen_move(codegen_scope *s, uint16_t dst, uint16_t src, int nopeep)
{
  if (nopeep || no_peephole(s)) goto normal;
  else if (dst == src) return;
  else {
    struct mrb_insn_data data = mrb_last_insn(s);

    switch (data.insn) {
    case OP_MOVE:
      if (dst == src) return;   /* remove useless MOVE */
      if (data.a == src) {
        if (data.b == dst)      /* skip swapping MOVE */
          return;
        if (data.a < s->nlocals) goto normal;
        rewind_pc(s);
        s->lastpc = addr_pc(s, mrb_prev_pc(s, data.addr));
        gen_move(s, dst, data.b, FALSE);
        return;
      }
      if (dst == data.a) {      /* skip overwritten move */
        rewind_pc(s);
        s->lastpc = addr_pc(s, mrb_prev_pc(s, data.addr));
        gen_move(s, dst, src, FALSE);
        return;
      }
      goto normal;
    case OP_LOADNIL: case OP_LOADSELF: case OP_LOADT: case OP_LOADF:
    case OP_LOADI__1:
    case OP_LOADI_0: case OP_LOADI_1: case OP_LOADI_2: case OP_LOADI_3:
    case OP_LOADI_4: case OP_LOADI_5: case OP_LOADI_6: case OP_LOADI_7:
      if (data.a != src || data.a < s->nlocals) goto normal;
      rewind_pc(s);
      genop_1(s, data.insn, dst);
      return;
    case OP_HASH:
      if (data.b != 0) goto normal;
      /* fall through */
    case OP_LOADI: case OP_LOADINEG:
    case OP_LOADL: case OP_LOADSYM:
    case OP_GETGV: case OP_GETSV: case OP_GETIV: case OP_GETCV:
    case OP_GETCONST: case OP_STRING:
    case OP_LAMBDA: case OP_BLOCK: case OP_METHOD: case OP_BLKPUSH:
      if (data.a != src || data.a < s->nlocals) goto normal;
      rewind_pc(s);
      genop_2(s, data.insn, dst, data.b);
      return;
    case OP_LOADI16:
      if (data.a != src || data.a < s->nlocals) goto normal;
      rewind_pc(s);
      genop_2S(s, data.insn, dst, data.b);
      return;
    case OP_LOADI32:
      if (data.a != src || data.a < s->nlocals) goto normal;
      else {
        uint32_t i = (uint32_t)data.b<<16|data.c;
        rewind_pc(s);
        genop_2SS(s, data.insn, dst, i);
      }
      return;
    case OP_ARRAY:
      if (data.a != src || data.a < s->nlocals || data.a < dst) goto normal;
      rewind_pc(s);
      if (data.b == 0 || dst == data.a)
        genop_2(s, OP_ARRAY, dst, 0);
      else
        genop_3(s, OP_ARRAY2, dst, data.a, data.b);
      return;
    case OP_ARRAY2:
      if (data.a != src || data.a < s->nlocals || data.a < dst) goto normal;
      rewind_pc(s);
      genop_3(s, OP_ARRAY2, dst, data.b, data.c);
      return;
    case OP_AREF:
    case OP_GETUPVAR:
      if (data.a != src || data.a < s->nlocals) goto normal;
      rewind_pc(s);
      genop_3(s, data.insn, dst, data.b, data.c);
      return;
    case OP_ADDI: case OP_SUBI:
      if (addr_pc(s, data.addr) == s->lastlabel || data.a != src || data.a < s->nlocals) goto normal;
      else {
        struct mrb_insn_data data0 = mrb_decode_insn(mrb_prev_pc(s, data.addr));
        if (data0.insn != OP_MOVE || data0.a != data.a || data0.b != dst) goto normal;
        s->pc = addr_pc(s, data0.addr);
        if (addr_pc(s, data0.addr) != s->lastlabel) {
          /* constant folding */
          data0 = mrb_decode_insn(mrb_prev_pc(s, data0.addr));
          mrb_int n;
          if (data0.a == dst && get_int_operand(s, &data0, &n)) {
            if ((data.insn == OP_ADDI && !mrb_int_add_overflow(n, data.b, &n)) ||
                (data.insn == OP_SUBI && !mrb_int_sub_overflow(n, data.b, &n))) {
              s->pc = addr_pc(s, data0.addr);
              gen_int(s, dst, n);
              return;
            }
          }
        }
      }
      genop_2(s, data.insn, dst, data.b);
      return;
    default:
      break;
    }
  }
 normal:
  genop_2(s, OP_MOVE, dst, src);
  return;
}

static int search_upvar(codegen_scope *s, mrb_sym id, int *idx);

static void
gen_getupvar(codegen_scope *s, uint16_t dst, mrb_sym id)
{
  int idx;
  int lv = search_upvar(s, id, &idx);

  if (!no_peephole(s)) {
    struct mrb_insn_data data = mrb_last_insn(s);
    if (data.insn == OP_SETUPVAR && data.a == dst && data.b == idx && data.c == lv) {
      /* skip GETUPVAR right after SETUPVAR */
      return;
    }
  }
  genop_3(s, OP_GETUPVAR, dst, idx, lv);
}

static void
gen_setupvar(codegen_scope *s, uint16_t dst, mrb_sym id)
{
  int idx;
  int lv = search_upvar(s, id, &idx);

  if (!no_peephole(s)) {
    struct mrb_insn_data data = mrb_last_insn(s);
    if (data.insn == OP_MOVE && data.a == dst) {
      dst = data.b;
      rewind_pc(s);
    }
  }
  genop_3(s, OP_SETUPVAR, dst, idx, lv);
}

static void
gen_return(codegen_scope *s, uint8_t op, uint16_t src)
{
  if (no_peephole(s)) {
    genop_1(s, op, src);
  }
  else {
    struct mrb_insn_data data = mrb_last_insn(s);

    if (data.insn == OP_MOVE && src == data.a) {
      rewind_pc(s);
      genop_1(s, op, data.b);
    }
    else if (data.insn != OP_RETURN) {
      genop_1(s, op, src);
    }
  }
}

static mrb_bool
get_int_operand(codegen_scope *s, struct mrb_insn_data *data, mrb_int *n)
{
  switch (data->insn) {
  case OP_LOADI__1:
    *n = -1;
    return TRUE;

  case OP_LOADINEG:
    *n = -data->b;
    return TRUE;

  case OP_LOADI_0: case OP_LOADI_1: case OP_LOADI_2: case OP_LOADI_3:
  case OP_LOADI_4: case OP_LOADI_5: case OP_LOADI_6: case OP_LOADI_7:
    *n = data->insn - OP_LOADI_0;
    return TRUE;

  case OP_LOADI:
  case OP_LOADI16:
    *n = (int16_t)data->b;
    return TRUE;

  case OP_LOADI32:
    *n = (mrb_int)((uint32_t)data->b<<16)+data->c;
    return TRUE;

  case OP_LOADL:
    {
      mrb_pool_value *pv = &s->pool[data->b];

      if (pv->tt == IREP_TT_INT32) {
        *n = (mrb_int)pv->u.i32;
      }
#ifdef MRB_INT64
      else if (pv->tt == IREP_TT_INT64) {
        *n = (mrb_int)pv->u.i64;
      }
#endif
      else {
        return FALSE;
      }
    }
    return TRUE;

  default:
    return FALSE;
  }
}

static void
gen_addsub(codegen_scope *s, uint8_t op, uint16_t dst)
{
  if (no_peephole(s)) {
  normal:
    genop_1(s, op, dst);
    return;
  }
  else {
    struct mrb_insn_data data = mrb_last_insn(s);
    mrb_int n;

    if (!get_int_operand(s, &data, &n)) {
      /* not integer immediate */
      goto normal;
    }
    struct mrb_insn_data data0 = mrb_decode_insn(mrb_prev_pc(s, data.addr));
    mrb_int n0;
    if (addr_pc(s, data.addr) == s->lastlabel || !get_int_operand(s, &data0, &n0)) {
      /* OP_ADDI/OP_SUBI takes upto 8bits */
      if (n > INT8_MAX || n < INT8_MIN) goto normal;
      rewind_pc(s);
      if (n == 0) return;
      if (n > 0) {
        if (op == OP_ADD) genop_2(s, OP_ADDI, dst, (uint16_t)n);
        else genop_2(s, OP_SUBI, dst, (uint16_t)n);
      }
      else {                    /* n < 0 */
        n = -n;
        if (op == OP_ADD) genop_2(s, OP_SUBI, dst, (uint16_t)n);
        else genop_2(s, OP_ADDI, dst, (uint16_t)n);
      }
      return;
    }
    if (op == OP_ADD) {
      if (mrb_int_add_overflow(n0, n, &n)) goto normal;
    }
    else { /* OP_SUB */
      if (mrb_int_sub_overflow(n0, n, &n)) goto normal;
    }
    s->pc = addr_pc(s, data0.addr);
    gen_int(s, dst, n);
  }
}

static void
gen_muldiv(codegen_scope *s, uint8_t op, uint16_t dst)
{
  if (no_peephole(s)) {
  normal:
    genop_1(s, op, dst);
    return;
  }
  else {
    struct mrb_insn_data data = mrb_last_insn(s);
    mrb_int n, n0;
    if (addr_pc(s, data.addr) == s->lastlabel || !get_int_operand(s, &data, &n)) {
      /* not integer immediate */
      goto normal;
    }
    struct mrb_insn_data data0 = mrb_decode_insn(mrb_prev_pc(s, data.addr));
    if (!get_int_operand(s, &data0, &n0)) {
      goto normal;
    }
    if (op == OP_MUL) {
      if (mrb_int_mul_overflow(n0, n, &n)) goto normal;
    }
    else { /* OP_DIV */
      if (n == 0) goto normal;
      if (n0 == MRB_INT_MIN && n == -1) goto normal;
      n = mrb_div_int(n0, n);
    }
    s->pc = addr_pc(s, data0.addr);
    gen_int(s, dst, n);
  }
}

mrb_bool mrb_num_shift(mrb_state *mrb, mrb_int val, mrb_int width, mrb_int *num);

static mrb_bool
gen_binop(codegen_scope *s, mrb_sym op, uint16_t dst)
{
  if (no_peephole(s)) return FALSE;
  else if (op == MRB_OPSYM_2(s->mrb, aref)) {
    genop_1(s, OP_GETIDX, dst);
    return TRUE;
  }
  else {
    struct mrb_insn_data data = mrb_last_insn(s);
    mrb_int n, n0;
    if (addr_pc(s, data.addr) == s->lastlabel || !get_int_operand(s, &data, &n)) {
      /* not integer immediate */
      return FALSE;
    }
    struct mrb_insn_data data0 = mrb_decode_insn(mrb_prev_pc(s, data.addr));
    if (!get_int_operand(s, &data0, &n0)) {
      return FALSE;
    }
    if (op == MRB_OPSYM_2(s->mrb, lshift)) {
      if (!mrb_num_shift(s->mrb, n0, n, &n)) return FALSE;
    }
    else if (op == MRB_OPSYM_2(s->mrb, rshift)) {
      if (n == MRB_INT_MIN) return FALSE;
      if (!mrb_num_shift(s->mrb, n0, -n, &n)) return FALSE;
    }
    else if (op == MRB_OPSYM_2(s->mrb, mod) && n != 0) {
      if (n0 == MRB_INT_MIN && n == -1) {
        n = 0;
      }
      else {
        mrb_int n1 = n0 % n;
        if ((n0 < 0) != (n < 0) && n1 != 0) {
          n1 += n;
        }
        n = n1;
      }
    }
    else if (op == MRB_OPSYM_2(s->mrb, and)) {
      n = n0 & n;
    }
    else if (op == MRB_OPSYM_2(s->mrb, or)) {
      n = n0 | n;
    }
    else if (op == MRB_OPSYM_2(s->mrb, xor)) {
      n = n0 ^ n;
    }
    else {
      return FALSE;
    }
    s->pc = addr_pc(s, data0.addr);
    gen_int(s, dst, n);
    return TRUE;
  }
}

static uint32_t
dispatch(codegen_scope *s, uint32_t pos0)
{
  int32_t pos1;
  int32_t offset;
  int16_t newpos;

  if (pos0 == JMPLINK_START) return 0;

  pos1 = pos0 + 2;
  offset = s->pc - pos1;
  if (offset > INT16_MAX) {
    codegen_error(s, "too big jmp offset");
  }
  s->lastlabel = s->pc;
  newpos = (int16_t)PEEK_S(s->iseq+pos0);
  emit_S(s, pos0, (uint16_t)offset);
  if (newpos == 0) return 0;
  return pos1+newpos;
}

static void
dispatch_linked(codegen_scope *s, uint32_t pos)
{
  if (pos==JMPLINK_START) return;
  for (;;) {
    pos = dispatch(s, pos);
    if (pos==0) break;
  }
}

#define nregs_update do {if (s->sp > s->nregs) s->nregs = s->sp;} while (0)
static void
push_n_(codegen_scope *s, int n)
{
  if (s->sp+n >= 0xffff) {
    codegen_error(s, "too complex expression");
  }
  s->sp+=n;
  nregs_update;
}

static void
pop_n_(codegen_scope *s, int n)
{
  if ((int)s->sp-n < 0) {
    codegen_error(s, "stack pointer underflow");
  }
  s->sp-=n;
}

#define push() push_n_(s,1)
#define push_n(n) push_n_(s,n)
#define pop() pop_n_(s,1)
#define pop_n(n) pop_n_(s,n)
#define cursp() (s->sp)

static mrb_pool_value*
lit_pool_extend(codegen_scope *s)
{
  if (s->irep->plen == s->pcapa) {
    s->pcapa *= 2;
    s->pool = (mrb_pool_value*)codegen_realloc(s, s->pool, sizeof(mrb_pool_value)*s->pcapa);
  }

  return &s->pool[s->irep->plen++];
}

static int
new_litbint(codegen_scope *s, const char *p, int base, mrb_bool neg)
{
  int i;
  size_t plen;
  mrb_pool_value *pv;

  plen = strlen(p);
  if (plen > 255) {
    codegen_error(s, "integer too big");
  }
  for (i=0; i<s->irep->plen; i++) {
    size_t len;
    pv = &s->pool[i];
    if (pv->tt != IREP_TT_BIGINT) continue;
    len = pv->u.str[0];
    if (len == plen && pv->u.str[1] == base && memcmp(pv->u.str+2, p, len) == 0)
      return i;
  }

  pv = lit_pool_extend(s);

  char *buf;
  pv->tt = IREP_TT_BIGINT;
  buf = (char*)codegen_realloc(s, NULL, plen+3);
  buf[0] = (char)plen;
  if (neg) buf[1] = -base;
  else buf[1] = base;
  memcpy(buf+2, p, plen);
  buf[plen+2] = '\0';
  pv->u.str = buf;

  return i;
}

static int
new_lit_str(codegen_scope *s, const char *str, mrb_int len)
{
  int i;
  mrb_pool_value *pv;

  for (i=0; i<s->irep->plen; i++) {
    pv = &s->pool[i];
    if (pv->tt & IREP_TT_NFLAG) continue;
    mrb_int plen = pv->tt>>2;
    if (len != plen) continue;
    if (memcmp(pv->u.str, str, plen) == 0)
      return i;
  }

  pv = lit_pool_extend(s);

  if (mrb_ro_data_p(str)) {
    pv->tt = (uint32_t)(len<<2) | IREP_TT_SSTR;
    pv->u.str = str;
  }
  else {
    char *p;
    pv->tt = (uint32_t)(len<<2) | IREP_TT_STR;
    p = (char*)codegen_realloc(s, NULL, len+1);
    memcpy(p, str, len);
    p[len] = '\0';
    pv->u.str = p;
  }

  return i;
}

#if 0
static int
new_lit_cstr(codegen_scope *s, const char *str)
{
  return new_lit_str(s, str, (mrb_int)strlen(str));
}
#endif

static int
new_lit_int(codegen_scope *s, mrb_int num)
{
  int i;
  mrb_pool_value *pv;

  for (i=0; i<s->irep->plen; i++) {
    pv = &s->pool[i];
    if (pv->tt == IREP_TT_INT32) {
      if (num == pv->u.i32) return i;
    }
#ifdef MRB_64BIT
    else if (pv->tt == IREP_TT_INT64) {
      if (num == pv->u.i64) return i;
    }
    continue;
#endif
  }

  pv = lit_pool_extend(s);

#ifdef MRB_INT64
  pv->tt = IREP_TT_INT64;
  pv->u.i64 = num;
#else
  pv->tt = IREP_TT_INT32;
  pv->u.i32 = num;
#endif

  return i;
}

#ifndef MRB_NO_FLOAT
static int
new_lit_float(codegen_scope *s, mrb_float num)
{
  int i;
  mrb_pool_value *pv;

  for (i=0; i<s->irep->plen; i++) {
    mrb_float f;
    pv = &s->pool[i];
    if (pv->tt != IREP_TT_FLOAT) continue;
    f = pv->u.f;
    if (f == num && !signbit(f) == !signbit(num)) return i;
  }

  pv = lit_pool_extend(s);

  pv->tt = IREP_TT_FLOAT;
  pv->u.f = num;

  return i;
}
#endif

static int
new_sym(codegen_scope *s, mrb_sym sym)
{
  int i, len;

  mrb_assert(s->irep);

  len = s->irep->slen;
  for (i=0; i<len; i++) {
    if (s->syms[i] == sym) return i;
  }
  if (s->irep->slen >= s->scapa) {
    s->scapa *= 2;
    if (s->scapa > 0xffff) {
      codegen_error(s, "too many symbols");
    }
    s->syms = (mrb_sym*)codegen_realloc(s, s->syms, sizeof(mrb_sym)*s->scapa);
  }
  s->syms[s->irep->slen] = sym;
  return s->irep->slen++;
}

static void
gen_setxv(codegen_scope *s, uint8_t op, uint16_t dst, mrb_sym sym, int val)
{
  int idx = new_sym(s, sym);
  if (!val && !no_peephole(s)) {
    struct mrb_insn_data data = mrb_last_insn(s);
    if (data.insn == OP_MOVE && data.a == dst) {
      dst = data.b;
      rewind_pc(s);
    }
  }
  genop_2(s, op, dst, idx);
}

static void
gen_int(codegen_scope *s, uint16_t dst, mrb_int i)
{
  if (i < 0) {
    if (i == -1) genop_1(s, OP_LOADI__1, dst);
    else if (i >= -0xff) genop_2(s, OP_LOADINEG, dst, (uint16_t)-i);
    else if (i >= INT16_MIN) genop_2S(s, OP_LOADI16, dst, (uint16_t)i);
    else if (i >= INT32_MIN) genop_2SS(s, OP_LOADI32, dst, (uint32_t)i);
    else goto int_lit;
  }
  else if (i < 8) genop_1(s, OP_LOADI_0 + (uint8_t)i, dst);
  else if (i <= 0xff) genop_2(s, OP_LOADI, dst, (uint16_t)i);
  else if (i <= INT16_MAX) genop_2S(s, OP_LOADI16, dst, (uint16_t)i);
  else if (i <= INT32_MAX) genop_2SS(s, OP_LOADI32, dst, (uint32_t)i);
  else {
  int_lit:
    genop_2(s, OP_LOADL, dst, new_lit_int(s, i));
  }
}

static mrb_bool
gen_uniop(codegen_scope *s, mrb_sym sym, uint16_t dst)
{
  if (no_peephole(s)) return FALSE;
  struct mrb_insn_data data = mrb_last_insn(s);
  mrb_int n;

  if (!get_int_operand(s, &data, &n)) return FALSE;
  if (sym == MRB_OPSYM_2(s->mrb, plus)) {
    /* unary plus does nothing */
  }
  else if (sym == MRB_OPSYM_2(s->mrb, minus)) {
    if (n == MRB_INT_MIN) return FALSE;
    n = -n;
  }
  else if (sym == MRB_OPSYM_2(s->mrb, neg)) {
    n = ~n;
  }
  else {
    return FALSE;
  }
  s->pc = addr_pc(s, data.addr);
  gen_int(s, dst, n);
  return TRUE;
}

#if 0
static int
node_len(node *tree)
{
  int n = 0;

  while (tree) {
    n++;
    tree = tree->cdr;
  }
  return n;
}

#define nint(x) ((int)(intptr_t)(x))
#define nchar(x) ((char)(intptr_t)(x))
#define nsym(x) ((mrb_sym)(intptr_t)(x))

#define lv_name(lv) nsym((lv)->car)
#endif

static int
lv_idx(codegen_scope *s, mrb_sym id)
{
  mrb_sym *lv = s->lv;
  int n = 1;

  for (size_t i = 0; i < s->lvsize; i++) {
    if (lv[i] == id) return n;
    n++;
  }
  return 0;
}

static int
search_upvar(codegen_scope *s, mrb_sym id, int *idx)
{
  const struct RProc *u;
  int lv = 0;
  codegen_scope *up = s->prev;

  while (up) {
    *idx = lv_idx(up, id);
    if (*idx > 0) {
      return lv;
    }
    lv++;
    up = up->prev;
  }

  if (lv < 1) lv = 1;
  u = s->cxt->upper;
  while (u && !MRB_PROC_CFUNC_P(u)) {
    const struct mrb_irep *ir = u->body.irep;
    uint_fast16_t n = ir->nlocals;
    int i;

    const mrb_sym *v = ir->lv;
    if (v) {
      for (i=1; n > 1; n--, v++, i++) {
        if (*v == id) {
          *idx = i;
          return lv - 1;
        }
      }
    }
    if (MRB_PROC_SCOPE_P(u)) break;
    u = u->upper;
    lv++;
  }

  if (id == MRB_OPSYM_2(s->mrb, and)) {
    codegen_error(s, "No anonymous block parameter");
  }
  else if (id == MRB_OPSYM_2(s->mrb, mul)) {
    codegen_error(s, "No anonymous rest parameter");
  }
  else if (id == MRB_OPSYM_2(s->mrb, pow)) {
    codegen_error(s, "No anonymous keyword rest parameter");
  }
  else {
    // mrb_p(s->mrb, mrb_nil_value());
    // mrb_p(s->mrb, mrb_symbol_value(id));
    codegen_error(s, "Can't find local variables");
  }
  return -1; /* not reached */
}

static void
for_body(codegen_scope *s, yp_for_node_t *node)
{
  codegen_scope *prev = s;
  int idx;
  struct loopinfo *lp;

  /* generate receiver */
  codegen(s, node->collection, VAL);
  /* generate loop-block */
  s = scope_new(s->mrb, s->cxt, s, NULL, 0);

  push();                       /* push for a block parameter */

  /* generate loop variable */
  genop_W(s, OP_ENTER, 0x40000);
  mrb_assert(node->index->type == YP_NODE_MULTI_WRITE_NODE);
  yp_multi_write_node_t *write = (yp_multi_write_node_t*)node->index;
  if (write->targets.size == 1) {
    gen_assignment(s, write->targets.nodes[0], NULL, 1, NOVAL);
  }
  else {
    gen_massignment(s, write->targets, 1, VAL);
  }
  /* construct loop */
  lp = loop_push(s, LOOP_FOR);
  lp->pc1 = new_label(s);

  /* loop body */
  codegen(s, (yp_node_t*)node->statements, VAL);
  pop();
  gen_return(s, OP_RETURN, cursp());
  loop_pop(s, NOVAL);
  scope_finish(s);
  s = prev;
  genop_2(s, OP_BLOCK, cursp(), s->irep->rlen-1);
  push();pop(); /* space for a block */
  pop();
  idx = new_sym(s, MRB_SYM_2(s->mrb, each));
  genop_3(s, OP_SENDB, cursp(), idx, 0);
}

static void
count_destructured_parameters(yp_node_list_t parameters, size_t *lvsize)
{
  for (size_t i=0; i < parameters.size; i++) {
    if (parameters.nodes[i]->type == YP_NODE_REQUIRED_DESTRUCTURED_PARAMETER_NODE) {
      (*lvsize)++;
      count_destructured_parameters(((yp_required_destructured_parameter_node_t*)parameters.nodes[i])->parameters, lvsize);
    }
  }
}

static void
init_required_parameters(codegen_scope *s, yp_node_list_t parameters, mrb_sym *lv, size_t *lvidx, size_t *ypidx)
{
  for (size_t i=0; i < parameters.size; i++) {
    switch (parameters.nodes[i]->type) {
    case YP_NODE_REQUIRED_PARAMETER_NODE:
      lv[(*lvidx)++] = yarp_sym2(s->mrb, parameters.nodes[i]->location);
      (*ypidx)++;
      break;

    case YP_NODE_REQUIRED_DESTRUCTURED_PARAMETER_NODE:
      lv[(*lvidx)++] = 0;
      break;

    default:
      mrb_assert(FALSE);
    }
  }
}

static void
init_destructured_parameters(codegen_scope *s, yp_node_list_t parameters, mrb_sym *lv, size_t *lvidx, size_t *ypidx)
{
    for (size_t i=0; i < parameters.size; i++) {
      if (parameters.nodes[i]->type == YP_NODE_REQUIRED_DESTRUCTURED_PARAMETER_NODE) {
        yp_required_destructured_parameter_node_t *destructured = (yp_required_destructured_parameter_node_t*)parameters.nodes[i];

        for (size_t j=0; j < destructured->parameters.size; j++) {
          yp_node_t *node = destructured->parameters.nodes[j];
          switch (node->type) {
          case YP_NODE_REQUIRED_PARAMETER_NODE:
            lv[(*lvidx)++] = yarp_sym2(s->mrb, node->location);
            (*ypidx)++;
            break;

          case YP_NODE_SPLAT_NODE:
            {
              yp_splat_node_t *splat = (yp_splat_node_t*)node;
              if (splat->expression) {
                mrb_assert(splat->expression->type == YP_NODE_REQUIRED_PARAMETER_NODE);
                lv[(*lvidx)++] = yarp_sym2(s->mrb, splat->expression->location);
                (*ypidx)++;
              }
              break;
            }

          case YP_NODE_REQUIRED_DESTRUCTURED_PARAMETER_NODE:
            lv[(*lvidx)++] = 0;
            break;

          default:
            mrb_assert(FALSE);
          }
        }

        init_destructured_parameters(s, destructured->parameters, lv, lvidx, ypidx);
      }
    }
}

static int
lambda_body(codegen_scope *s, yp_scope_node_t *scope, yp_parameters_node_t *parameters, yp_node_t *statements, int blk)
{
  codegen_scope *parent = s;
  size_t lvsize = 0;
  mrb_sym *lv = NULL;
  if (parameters != NULL) {
    mrb_assert(scope != NULL);
    lvsize = scope->locals.size;
    if (parameters->keyword_rest == NULL && parameters->keywords.size > 0)
      lvsize++;
    if (parameters->block == NULL)
      lvsize++;
    count_destructured_parameters(parameters->requireds, &lvsize);
    count_destructured_parameters(parameters->posts, &lvsize);
    lv = (mrb_sym*)mrb_malloc(s->mrb, sizeof(mrb_sym)*lvsize);
    size_t lvidx = 0, ypidx = 0;
    init_required_parameters(s, parameters->requireds, lv, &lvidx, &ypidx);
    for (size_t i=0; i < parameters->optionals.size; i++) {
      lv[lvidx++] = yarp_sym(s->mrb, ((yp_optional_parameter_node_t*)parameters->optionals.nodes[i])->name);
      ypidx++;
    }
    if (parameters->rest != NULL) {
      yp_token_t name = ((yp_rest_parameter_node_t*)parameters->rest)->name;
      if (name.type != YP_TOKEN_NOT_PROVIDED) {
        lv[lvidx++] = yarp_sym(s->mrb, name);
      } else {
        lv[lvidx++] = MRB_OPSYM_2(s->mrb, mul);
      }
      ypidx++;
    }
    init_required_parameters(s, parameters->posts, lv, &lvidx, &ypidx);
    if (parameters->keyword_rest != NULL) {
      yp_token_t name = ((yp_keyword_rest_parameter_node_t*)parameters->keyword_rest)->name;
      if (name.type != YP_TOKEN_NOT_PROVIDED) {
        lv[lvidx++] = yarp_sym(s->mrb, name);
      } else {
        lv[lvidx++] = MRB_OPSYM_2(s->mrb, pow);
      }
      ypidx++;
    } else if (parameters->keywords.size > 0) {
      lv[lvidx++] = MRB_OPSYM_2(s->mrb, pow);
    }
    if (parameters->block != NULL) {
      yp_token_t name = ((yp_block_parameter_node_t*)parameters->block)->name;
      if (name.type != YP_TOKEN_NOT_PROVIDED) {
        lv[lvidx++] = yarp_sym(s->mrb, name);
      } else {
        lv[lvidx++] = MRB_OPSYM_2(s->mrb, and);
      }
      ypidx++;
    } else {
      lv[lvidx++] = 0;
    }
    for (size_t i=0; i < parameters->keywords.size; i++) {
      lv[lvidx++] = yarp_sym(s->mrb, yarp_keyword_parameter_name((yp_keyword_parameter_node_t*)parameters->keywords.nodes[i]));
      ypidx++;
    }
    init_destructured_parameters(s, parameters->requireds, lv, &lvidx, &ypidx);
    init_destructured_parameters(s, parameters->posts, lv, &lvidx, &ypidx);
    mrb_assert(ypidx <= scope->locals.size);
    while (ypidx < scope->locals.size) {
      lv[lvidx++] = yarp_sym(s->mrb, scope->locals.tokens[ypidx]);
      ypidx++;
    }
  }
  s = scope_new(s->mrb, s->cxt, s, lv, lvsize);

  s->mscope = !blk;

  if (blk) {
    struct loopinfo *lp = loop_push(s, LOOP_BLOCK);
    lp->pc0 = new_label(s);
  }
  if (parameters == NULL) {
    genop_W(s, OP_ENTER, 0);
    s->ainfo = 0;
  }
  else {
    mrb_aspec a;
    int ma, oa, ra, pa, ka, kd, ba, i;
    uint32_t pos;
    yp_node_t **margs, **oargs, **pargs, **kargs;

    /* mandatory arguments */
    ma = parameters->requireds.size;
    margs = parameters->requireds.nodes;

    /* optional arguments */
    oa = parameters->optionals.size;
    oargs = parameters->optionals.nodes;
    /* rest argument? */
    ra = parameters->rest ? 1 : 0;
    /* mandatory arguments after rest argument */
    pa = parameters->posts.size;
    pargs = parameters->posts.nodes;
    /* keyword arguments */
    ka = parameters->keywords.size;
    kargs = parameters->keywords.nodes;
    /* keyword dictionary? */
    kd = parameters->keyword_rest ? 1 : 0;
    /* block argument? */
    ba = parameters->block ? 1 : 0;

    if (ma > 0x1f || oa > 0x1f || pa > 0x1f || ka > 0x1f) {
      codegen_error(s, "too many formal arguments");
    }
    /* (23bits = 5:5:1:5:5:1:1) */
    a = MRB_ARGS_REQ(ma)
      | MRB_ARGS_OPT(oa)
      | (ra? MRB_ARGS_REST() : 0)
      | MRB_ARGS_POST(pa)
      | MRB_ARGS_KEY(ka, kd)
      | (ba? MRB_ARGS_BLOCK() : 0);
    genop_W(s, OP_ENTER, a);
    /* (12bits = 5:1:5:1) */
    s->ainfo = (((ma+oa) & 0x3f) << 7)
      | ((ra & 0x1) << 6)
      | ((pa & 0x1f) << 1)
      | ((ka | kd) ? 1 : 0);
    /* generate jump table for optional arguments initializer */
    pos = new_label(s);
    for (i=0; i<oa; i++) {
      new_label(s);
      genjmp_0(s, OP_JMP);
    }
    if (oa > 0) {
      genjmp_0(s, OP_JMP);
    }
    for (i=0; i<oa; i++) {
      int idx;
      mrb_assert(oargs[i]->type == YP_NODE_OPTIONAL_PARAMETER_NODE);
      yp_optional_parameter_node_t *parameter = (yp_optional_parameter_node_t*)oargs[i];
      mrb_sym id = yarp_sym(s->mrb, parameter->name);

      dispatch(s, pos+i*3+1);
      codegen(s, parameter->value, VAL);
      pop();
      idx = lv_idx(s, id);
      if (idx > 0) {
        gen_move(s, idx, cursp(), 0);
      }
      else {
        gen_getupvar(s, cursp(), id);
      }
    }
    if (oa > 0) {
      dispatch(s, pos+i*3+1);
    }

    /* keyword arguments */
    if (ka > 0 || kd) {
      for (int j=0; j<ka; j++) {
        int jmpif_key_p, jmp_def_set = -1;
        mrb_assert(kargs[j]->type == YP_NODE_KEYWORD_PARAMETER_NODE);
        yp_keyword_parameter_node_t *kwd = (yp_keyword_parameter_node_t*)kargs[j];
        yp_node_t *def_arg = kwd->value;
        mrb_sym kwd_sym = yarp_sym(s->mrb, yarp_keyword_parameter_name(kwd));

        if (def_arg) {
          int idx;
          genop_2(s, OP_KEY_P, lv_idx(s, kwd_sym), new_sym(s, kwd_sym));
          jmpif_key_p = genjmp2_0(s, OP_JMPIF, lv_idx(s, kwd_sym), NOVAL);
          codegen(s, def_arg, VAL);
          pop();
          idx = lv_idx(s, kwd_sym);
          if (idx > 0) {
            gen_move(s, idx, cursp(), 0);
          }
          else {
            gen_getupvar(s, cursp(), kwd_sym);
          }
          jmp_def_set = genjmp_0(s, OP_JMP);
          dispatch(s, jmpif_key_p);
        }
        genop_2(s, OP_KARG, lv_idx(s, kwd_sym), new_sym(s, kwd_sym));
        if (jmp_def_set != -1) {
          dispatch(s, jmp_def_set);
        }
        i++;
      }
      if (ka > 0 && !kd) {
        genop_0(s, OP_KEYEND);
      }
    }

    /* argument destructuring */
    if (ma > 0) {
      pos = 1;
      for (int j=0; j<ma; j++) {
        if (margs[j]->type == YP_NODE_REQUIRED_DESTRUCTURED_PARAMETER_NODE) {
          gen_massignment(s, ((yp_required_destructured_parameter_node_t*)margs[j])->parameters, pos, NOVAL);
        }
        pos++;
      }
    }
    if (pa > 0) {
      pos = ma+oa+ra+1;
      for (int j=0; j<pa; j++) {
        if (pargs[j]->type == YP_NODE_REQUIRED_DESTRUCTURED_PARAMETER_NODE) {
          gen_massignment(s, ((yp_required_destructured_parameter_node_t*)pargs[j])->parameters, pos, NOVAL);
        }
        pos++;
      }
    }
  }

  codegen(s, statements, VAL);
  pop();
  if (s->pc > 0) {
    gen_return(s, OP_RETURN, cursp());
  }
  if (blk) {
    loop_pop(s, NOVAL);
  }
  scope_finish(s);
  return parent->irep->rlen - 1;
}

static int
scope_body(codegen_scope *s, yp_scope_node_t *node, yp_node_t *statements, int val)
{
  mrb_sym *lv = NULL;
  size_t lvsize = 0;
  if (node) {
    lvsize = node->locals.size;
    lv = (mrb_sym*)mrb_malloc(s->mrb, sizeof(mrb_sym)*lvsize);
    for (size_t i=0; i < lvsize; i++) {
      lv[i] = yarp_sym(s->mrb, node->locals.tokens[i]);
    }
  }
  codegen_scope *scope = scope_new(s->mrb, s->cxt, s, lv, lvsize);

  codegen(scope, statements, VAL);
  gen_return(scope, OP_RETURN, scope->sp-1);
  if (!s->iseq) {
    genop_0(scope, OP_STOP);
  }
  scope_finish(scope);
  if (!s->irep) {
    /* should not happen */
    return 0;
  }
  return s->irep->rlen - 1;
}

static mrb_bool
nosplat(yp_array_node_t *array)
{
  for (size_t i = 0; i < array->elements.size; i++) {
    if (array->elements.nodes[i]->type == YP_NODE_SPLAT_NODE) return FALSE;
  }
  return TRUE;
}

#if 0
static mrb_sym
attrsym(codegen_scope *s, mrb_sym a)
{
  const char *name;
  mrb_int len;
  char *name2;

  name = mrb_sym_name_len(s->mrb, a, &len);
  name2 = (char*)codegen_palloc(s,
                                (size_t)len
                                + 1 /* '=' */
                                + 1 /* '\0' */
                                );
  mrb_assert_int_fit(mrb_int, len, size_t, SIZE_MAX);
  memcpy(name2, name, (size_t)len);
  name2[len] = '=';
  name2[len+1] = '\0';

  return mrb_intern(s->mrb, name2, len+1);
}
#endif

#define CALL_MAXARGS 15
#define GEN_LIT_ARY_MAX 64
#define GEN_VAL_STACK_MAX 99

static int
gen_values(codegen_scope *s, yp_node_t **nodes, size_t node_count, int val, int limit)
{
  int n = 0;
  int first = 1;
  int slimit = GEN_VAL_STACK_MAX;

  if (limit == 0) limit = GEN_LIT_ARY_MAX;
  if (cursp() >= slimit) slimit = INT16_MAX;

  if (!val) {
    for (size_t i = 0; i < node_count; i++) {
      codegen(s, nodes[i], NOVAL);
      n++;
    }
    return n;
  }

  for (size_t i = 0; i < node_count; i++) {
    int is_splat = nodes[i]->type == YP_NODE_SPLAT_NODE;

    if (is_splat || cursp() >= slimit) { /* flush stack */
      pop_n(n);
      if (first) {
        if (n == 0) {
          genop_1(s, OP_LOADNIL, cursp());
        }
        else {
          genop_2(s, OP_ARRAY, cursp(), n);
        }
        push();
        first = 0;
        limit = GEN_LIT_ARY_MAX;
      }
      else if (n > 0) {
        pop();
        genop_2(s, OP_ARYPUSH, cursp(), n);
        push();
      }
      n = 0;
    }
    codegen(s, nodes[i], val);
    if (is_splat) {
      pop(); pop();
      genop_1(s, OP_ARYCAT, cursp());
      push();
    }
    else {
      n++;
    }
  }
  if (!first) {
    pop();
    if (n > 0) {
      pop_n(n);
      genop_2(s, OP_ARYPUSH, cursp(), n);
    }
    return -1;                  /* variable length */
  }
  else if (n > limit) {
    pop_n(n);
    genop_2(s, OP_ARRAY, cursp(), n);
    return -1;
  }
  return n;
}

static int
gen_hash(codegen_scope *s, yp_node_t **nodes, size_t node_count, int val, int limit)
{
  int slimit = GEN_VAL_STACK_MAX;
  if (cursp() >= GEN_LIT_ARY_MAX) slimit = INT16_MAX;
  int len = 0;
  mrb_bool update = FALSE;
  mrb_bool first = TRUE;

  for (size_t i = 0; i < node_count; i++) {
    if (nodes[i]->type == YP_NODE_ASSOC_SPLAT_NODE) {
      yp_assoc_splat_node_t *splat = (yp_assoc_splat_node_t*)nodes[i];
      if (val && first) {
        genop_2(s, OP_HASH, cursp(), 0);
        push();
        update = TRUE;
      }
      else if (val && len > 0) {
        pop_n(len*2);
        if (!update) {
          genop_2(s, OP_HASH, cursp(), len);
        }
        else {
          pop();
          genop_2(s, OP_HASHADD, cursp(), len);
        }
        push();
      }
      codegen(s, splat->value, val);
      if (val && (len > 0 || update)) {
        pop(); pop();
        genop_1(s, OP_HASHCAT, cursp());
        push();
      }
      update = TRUE;
      len = 0;
    }
    else {
      mrb_assert(nodes[i]->type == YP_NODE_ASSOC_NODE);
      yp_assoc_node_t *assoc = (yp_assoc_node_t*)nodes[i];
      codegen(s, assoc->key, val);
      codegen(s, assoc->value, val);
      len++;
    }
    if (val && cursp() >= slimit) {
      pop_n(len*2);
      if (!update) {
        genop_2(s, OP_HASH, cursp(), len);
      }
      else {
        pop();
        genop_2(s, OP_HASHADD, cursp(), len);
      }
      push();
      update = TRUE;
      len = 0;
    }
    first = FALSE;
  }
  if (val && len > limit) {
    pop_n(len*2);
    genop_2(s, OP_HASH, cursp(), len);
    push();
    return -1;
  }
  if (update) {
    if (val && len > 0) {
      pop_n(len*2+1);
      genop_2(s, OP_HASHADD, cursp(), len);
      push();
    }
    return -1;                  /* variable length */
  }
  return len;
}

static void
gen_call(codegen_scope *s, yp_call_node_t *node, int val)
{
  mrb_sym sym = yarp_sym3(s->mrb, &node->name);
  int skip = 0, n = 0, nk = 0, noop = no_optimize(s), noself = 0, blk = 0, sp_save = cursp();

  if (!node->receiver) {
    noself = noop = 1;
    push();
  }
  else {
    codegen(s, node->receiver, VAL); /* receiver */
  }
  if (yarp_safe_call_p(node)) {
    int recv = cursp()-1;
    gen_move(s, cursp(), recv, 1);
    skip = genjmp2_0(s, OP_JMPNIL, cursp(), val);
  }
  yp_node_t *block = (yp_node_t*)node->block;
  if (node->arguments) {
    yp_node_list_t arguments = node->arguments->arguments;
    size_t argc = arguments.size;
    mrb_assert(argc > 0);
    yp_hash_node_t *kwargs = NULL;
    if (arguments.nodes[argc - 1]->type == YP_NODE_BLOCK_ARGUMENT_NODE) {
      if (block != NULL)
        codegen_error(s, "both block arg and actual block given");
      block = arguments.nodes[argc - 1];
      argc -= 1;
    }
    if (argc > 0 && arguments.nodes[argc - 1]->type == YP_NODE_HASH_NODE) {
      yp_hash_node_t *hash = (yp_hash_node_t*)arguments.nodes[argc - 1];
      if (hash->opening.type == YP_TOKEN_NOT_PROVIDED) {
        argc -= 1;
        kwargs = hash;
      }
    }
    if (argc > 0) {             /* positional arguments */
      n = gen_values(s, arguments.nodes, argc, VAL, 14);
      if (n < 0) {              /* variable length */
        noop = 1;               /* not operator */
        n = 15;
        push();
      }
    }
    if (kwargs) {               /* keyword arguments */
      noop = 1;
      nk = gen_hash(s, kwargs->elements.nodes, kwargs->elements.size, VAL, 14);
      if (nk < 0) nk = 15;
    }
  }
  if (block) {
    codegen(s, block, VAL);
    pop();
    noop = 1;
    blk = 1;
  }
  push();pop();
  s->sp = sp_save;
  if (!noop && sym == MRB_OPSYM_2(s->mrb, add) && n == 1)  {
    gen_addsub(s, OP_ADD, cursp());
  }
  else if (!noop && sym == MRB_OPSYM_2(s->mrb, sub) && n == 1)  {
    gen_addsub(s, OP_SUB, cursp());
  }
  else if (!noop && sym == MRB_OPSYM_2(s->mrb, mul) && n == 1)  {
    gen_muldiv(s, OP_MUL, cursp());
  }
  else if (!noop && sym == MRB_OPSYM_2(s->mrb, div) && n == 1)  {
    gen_muldiv(s, OP_DIV, cursp());
  }
  else if (!noop && sym == MRB_OPSYM_2(s->mrb, lt) && n == 1)  {
    genop_1(s, OP_LT, cursp());
  }
  else if (!noop && sym == MRB_OPSYM_2(s->mrb, le) && n == 1)  {
    genop_1(s, OP_LE, cursp());
  }
  else if (!noop && sym == MRB_OPSYM_2(s->mrb, gt) && n == 1)  {
    genop_1(s, OP_GT, cursp());
  }
  else if (!noop && sym == MRB_OPSYM_2(s->mrb, ge) && n == 1)  {
    genop_1(s, OP_GE, cursp());
  }
  else if (!noop && sym == MRB_OPSYM_2(s->mrb, eq) && n == 1)  {
    genop_1(s, OP_EQ, cursp());
  }
  else if (!noop && sym == MRB_OPSYM_2(s->mrb, aset) && n == 2)  {
    genop_1(s, OP_SETIDX, cursp());
  }
  else if (!noop && n == 0 && gen_uniop(s, sym, cursp())) {
    /* constant folding succeeded */
  }
  else if (!noop && n == 1 && gen_binop(s, sym, cursp())) {
    /* constant folding succeeded */
  }
  else if (noself){
    genop_3(s, blk ? OP_SSENDB : OP_SSEND, cursp(), new_sym(s, sym), n|(nk<<4));
  }
  else {
    genop_3(s, blk ? OP_SENDB : OP_SEND, cursp(), new_sym(s, sym), n|(nk<<4));
  }
  if (yarp_safe_call_p(node)) {
    dispatch(s, skip);
  }
  if (val) {
    push();
  }
}

static void
gen_assignment(codegen_scope *s, yp_node_t *node, yp_node_t *rhs, int sp, int val)
{
  mrb_sym name;
  int idx;

  switch (node->type) {
  case YP_NODE_GLOBAL_VARIABLE_WRITE_NODE:
  case YP_NODE_REQUIRED_PARAMETER_NODE:
  case YP_NODE_LOCAL_VARIABLE_WRITE_NODE:
  case YP_NODE_INSTANCE_VARIABLE_WRITE_NODE:
  case YP_NODE_CLASS_VARIABLE_WRITE_NODE:
#if 0
  case NODE_NIL:
  case NODE_MASGN:
#endif
  case YP_NODE_REQUIRED_DESTRUCTURED_PARAMETER_NODE:
    if (rhs) {
      codegen(s, rhs, VAL);
      pop();
      sp = cursp();
    }
    break;

  case YP_NODE_CONSTANT_PATH_WRITE_NODE:
#if 0
  case NODE_CALL:
  case NODE_SCALL:
#endif
    /* keep evaluation order */
    break;

#if 0
  case NODE_NVAR:
    /* never happens; should have already checked in the parser */
    codegen_error(s, "Can't assign to numbered parameter");
    break;
#endif

  case YP_NODE_SPLAT_NODE:
    {
      mrb_assert(rhs == NULL);
      mrb_assert(val == NOVAL);
      yp_splat_node_t *splat = (yp_splat_node_t*)node;
      if (splat->expression)
        gen_assignment(s, splat->expression, NULL, sp, NOVAL);
      return;
    }

  default:
    codegen_error(s, "unknown lhs");
    break;
  }

  switch (node->type) {
  case YP_NODE_GLOBAL_VARIABLE_WRITE_NODE:
    name = yarp_sym(s->mrb, ((yp_global_variable_write_node_t*)node)->name);
    gen_setxv(s, OP_SETGV, sp, name, val);
    break;
  case YP_NODE_REQUIRED_PARAMETER_NODE:
  case YP_NODE_LOCAL_VARIABLE_WRITE_NODE:
    if (node->type == YP_NODE_REQUIRED_PARAMETER_NODE) {
      name = yarp_sym2(s->mrb, node->location);
    } else {
      name = yarp_sym2(s->mrb, ((yp_local_variable_write_node_t*)node)->name_loc);
    }
    idx = lv_idx(s, name);
    if (idx > 0) {
      if (idx != sp) {
        gen_move(s, idx, sp, val);
      }
      break;
    }
    else {                      /* upvar */
      gen_setupvar(s, sp, name);
    }
    break;
  case YP_NODE_INSTANCE_VARIABLE_WRITE_NODE:
    name = yarp_sym2(s->mrb, ((yp_instance_variable_write_node_t*)node)->name_loc);
    gen_setxv(s, OP_SETIV, sp, name, val);
    break;
  case YP_NODE_CLASS_VARIABLE_WRITE_NODE:
    name = yarp_sym2(s->mrb, ((yp_class_variable_write_node_t*)node)->name_loc);
    gen_setxv(s, OP_SETCV, sp, name, val);
    break;
  case YP_NODE_CONSTANT_PATH_WRITE_NODE: {
    yp_constant_path_write_node_t *write = (yp_constant_path_write_node_t*)node;
    yp_node_t *target = write->target;
    if (target->type == YP_NODE_CONSTANT_READ_NODE) {
      if (rhs) {
        codegen(s, rhs, VAL);
        pop();
        sp = cursp();
      }
      name = yarp_sym2(s->mrb, target->location);
      gen_setxv(s, OP_SETCONST, sp, name, val);
      break;
    }
    mrb_assert(target->type == YP_NODE_CONSTANT_PATH_NODE);
    yp_constant_path_node_t *path = (yp_constant_path_node_t*)target;
    mrb_assert(path->child->type == YP_NODE_CONSTANT_READ_NODE);
    name = yarp_sym2(s->mrb, path->child->location);
    if (sp) {
      gen_move(s, cursp(), sp, 0);
    }
    sp = cursp();
    push();
    if (path->parent) {
      codegen(s, path->parent, VAL);
      idx = new_sym(s, name);
    }
    else {
      genop_1(s, OP_OCLASS, cursp());
      push();
      idx = new_sym(s, name);
    }
    if (rhs) {
      codegen(s, rhs, VAL); pop();
      gen_move(s, sp, cursp(), 0);
    }
    pop_n(2);
    genop_2(s, OP_SETMCNST, sp, idx);
    break;
  }

#if 0
  case NODE_CALL:
  case NODE_SCALL:
    {
      int noself = 0, safe = (type == NODE_SCALL), skip = 0, top, call, n = 0;
      mrb_sym mid = nsym(tree->cdr->car);

      top = cursp();
      if (val || sp == cursp()) {
        push();                   /* room for retval */
      }
      call = cursp();
      if (!tree->car) {
        noself = 1;
        push();
      }
      else {
        codegen(s, tree->car, VAL); /* receiver */
      }
      if (safe) {
        int recv = cursp()-1;
        gen_move(s, cursp(), recv, 1);
        skip = genjmp2_0(s, OP_JMPNIL, cursp(), val);
      }
      tree = tree->cdr->cdr->car;
      if (tree) {
        if (tree->car) {            /* positional arguments */
          n = gen_values(s, tree->car, VAL, (tree->cdr->car)?13:14);
          if (n < 0) {              /* variable length */
            n = 15;
            push();
          }
        }
        if (tree->cdr->car) {       /* keyword arguments */
          if (n == 13 || n == 14) {
            pop_n(n);
            genop_2(s, OP_ARRAY, cursp(), n);
            push();
            n = 15;
          }
          gen_hash(s, tree->cdr->car->cdr, VAL, 0);
          if (n < 14) {
            n++;
          }
          else {
            pop_n(2);
            genop_2(s, OP_ARYPUSH, cursp(), 1);
          }
          push();
        }
      }
      if (rhs) {
        codegen(s, rhs, VAL);
        pop();
      }
      else {
        gen_move(s, cursp(), sp, 0);
      }
      if (val) {
        gen_move(s, top, cursp(), 1);
      }
      if (n < 15) {
        n++;
        if (n == 15) {
          pop_n(14);
          genop_2(s, OP_ARRAY, cursp(), 15);
        }
      }
      else {
        pop();
        genop_2(s, OP_ARYPUSH, cursp(), 1);
      }
      push(); pop();
      s->sp = call;
      if (mid == MRB_OPSYM_2(s->mrb, aref) && n == 2) {
        push_n(4); pop_n(4); /* self + idx + value + (invisible block for OP_SEND) */
        genop_1(s, OP_SETIDX, cursp());
      }
      else {
        int st = 2 /* self + block */ +
                 (((n >> 0) & 0x0f) < 15 ? ((n >> 0) & 0x0f)     : 1) +
                 (((n >> 4) & 0x0f) < 15 ? ((n >> 4) & 0x0f) * 2 : 1);
        push_n(st); pop_n(st);
        genop_3(s, noself ? OP_SSEND : OP_SEND, cursp(), new_sym(s, attrsym(s, mid)), n);
      }
      if (safe) {
        dispatch(s, skip);
      }
      s->sp = top;
    }
    break;

  case NODE_MASGN:
    gen_massignment(s, tree->car, sp, val);
    break;
#endif

  case YP_NODE_REQUIRED_DESTRUCTURED_PARAMETER_NODE:
    gen_massignment(s, ((yp_required_destructured_parameter_node_t*)node)->parameters, sp, val);
    break;

#if 0
  /* splat without assignment */
  case NODE_NIL:
    break;
#endif

  default:
    codegen_error(s, "unknown lhs");
    break;
  }
  if (val) push();
}

static void
count_multi_write(yp_node_list_t targets, int *pre, yp_splat_node_t **splat, int *post) {
  for (size_t i = 0; i < targets.size; i++) {
    yp_node_t *node = targets.nodes[i];
    if (node->type == YP_NODE_SPLAT_NODE) {
      *splat = (yp_splat_node_t*)node;
      continue;
    }
    if (*splat) {
      (*post)++;
    } else {
      (*pre)++;
    }
  }
}

static void
gen_massignment(codegen_scope *s, yp_node_list_t targets, int rhs, int val)
{
  int n = 0, post = 0;

  int idx = 0, pre = 0;
  yp_splat_node_t *splat = NULL;
  count_multi_write(targets, &pre, &splat, &post);
  if (pre > 0) {
    n = 0;
    while (idx < pre) {
      int sp = cursp();

      genop_3(s, OP_AREF, sp, rhs, n);
      push();
      gen_assignment(s, targets.nodes[idx], NULL, sp, NOVAL);
      pop();
      n++;
      idx++;
    }
  }
  if (true) {
    gen_move(s, cursp(), rhs, val);
    push_n(post+1);
    pop_n(post+1);
    genop_3(s, OP_APOST, cursp(), n, post);
    n = 1;
    if (splat && splat->expression) {
      gen_assignment(s, splat->expression, NULL, cursp(), NOVAL);
    }
    if (post > 0) {
      idx++;
      while (idx < targets.size) {
        gen_assignment(s, targets.nodes[idx], NULL, cursp()+n, NOVAL);
        idx++;
        n++;
      }
    }
    if (val) {
      gen_move(s, cursp(), rhs, 0);
    }
  }
}

static void
gen_intern(codegen_scope *s)
{
  pop();
  if (!no_peephole(s)) {
    struct mrb_insn_data data = mrb_last_insn(s);

    if (data.insn == OP_STRING && data.a == cursp()) {
      rewind_pc(s);
      genop_2(s, OP_SYMBOL, data.a, data.b);
      push();
      return;
    }
  }
  genop_1(s, OP_INTERN, cursp());
  push();
}

#if 0
static void
gen_literal_array(codegen_scope *s, node *tree, mrb_bool sym, int val)
{
  if (val) {
    int i = 0, j = 0, gen = 0;

    while (tree) {
      switch (nint(tree->car->car)) {
      case NODE_STR:
        if ((tree->cdr == NULL) && (nint(tree->car->cdr->cdr) == 0))
          break;
        /* fall through */
      case NODE_BEGIN:
        codegen(s, tree->car, VAL);
        j++;
        break;

      case NODE_LITERAL_DELIM:
        if (j > 0) {
          j = 0;
          i++;
          if (sym)
            gen_intern(s);
        }
        break;
      }
      while (j >= 2) {
        pop(); pop();
        genop_1(s, OP_STRCAT, cursp());
        push();
        j--;
      }
      if (i > GEN_LIT_ARY_MAX) {
        pop_n(i);
        if (gen) {
          pop();
          genop_2(s, OP_ARYPUSH, cursp(), i);
        }
        else {
          genop_2(s, OP_ARRAY, cursp(), i);
          gen = 1;
        }
        push();
        i = 0;
      }
      tree = tree->cdr;
    }
    if (j > 0) {
      i++;
      if (sym)
        gen_intern(s);
    }
    pop_n(i);
    if (gen) {
      pop();
      genop_2(s, OP_ARYPUSH, cursp(), i);
    }
    else {
      genop_2(s, OP_ARRAY, cursp(), i);
    }
    push();
  }
  else {
    while (tree) {
      switch (nint(tree->car->car)) {
      case NODE_BEGIN: case NODE_BLOCK:
        codegen(s, tree->car, NOVAL);
      }
      tree = tree->cdr;
    }
  }
}

static void
raise_error(codegen_scope *s, const char *msg)
{
  int idx = new_lit_cstr(s, msg);

  genop_1(s, OP_ERR, idx);
}
#endif

static mrb_int
readint(codegen_scope *s, const char *p, const char *e, int base, mrb_bool neg, mrb_bool *overflow)
{
  mrb_int result = 0;

  mrb_assert(base >= 2 && base <= 16);
  if (*p == '+') p++;
  while (p < e) {
    int n;
    char c = *p;
    switch (c) {
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
      n = c - '0'; break;
    case '8': case '9':
      n = c - '0'; break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      n = c - 'a' + 10; break;
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
      n = c - 'A' + 10; break;
    default:
      codegen_error(s, "malformed readint input");
      *overflow = TRUE;
      /* not reached */
      return result;
    }
    if (mrb_int_mul_overflow(result, base, &result)) {
    overflow:
      *overflow = TRUE;
      return 0;
    }
    mrb_uint tmp = ((mrb_uint)result)+n;
    if (neg && tmp == (mrb_uint)MRB_INT_MAX+1) {
      *overflow = FALSE;
      return MRB_INT_MIN;
    }
    if (tmp > MRB_INT_MAX) goto overflow;
    result = (mrb_int)tmp;
    p++;
  }
  *overflow = FALSE;
  if (neg) return -result;
  return result;
}

#if 0
static void
gen_retval(codegen_scope *s, node *tree)
{
  if (nint(tree->car) == NODE_SPLAT) {
    codegen(s, tree, VAL);
    pop();
    genop_1(s, OP_ARYSPLAT, cursp());
  }
  else {
    codegen(s, tree, VAL);
    pop();
  }
}
#endif

static mrb_bool
true_always(yp_node_t *node)
{
  switch (node->type) {
  case YP_NODE_TRUE_NODE:
  case YP_NODE_INTEGER_NODE:
  case YP_NODE_STRING_NODE:
  case YP_NODE_SYMBOL_NODE:
    return TRUE;
  default:
    return FALSE;
  }
}

static mrb_bool
false_always(yp_node_t *node)
{
  switch (node->type) {
  case YP_NODE_FALSE_NODE:
  case YP_NODE_NIL_NODE:
    return TRUE;
  default:
    return FALSE;
  }
}

#if 0
static void
gen_blkmove(codegen_scope *s, uint16_t ainfo, int lv)
{
  int m1 = (ainfo>>7)&0x3f;
  int r  = (ainfo>>6)&0x1;
  int m2 = (ainfo>>1)&0x1f;
  int kd = (ainfo)&0x1;
  int off = m1+r+m2+kd+1;
  if (lv == 0) {
    gen_move(s, cursp(), off, 0);
  }
  else {
    genop_3(s, OP_GETUPVAR, cursp(), off, lv);
  }
  push();
}
#endif

static void
codegen(codegen_scope *s, yp_node_t *node, int val)
{
#if 0
  int nt;
#endif
  int rlev = s->rlev;

  if (!node) {
    if (val) {
      genop_1(s, OP_LOADNIL, cursp());
      push();
    }
    return;
  }

  s->rlev++;
  if (s->rlev > MRB_CODEGEN_LEVEL_MAX) {
    codegen_error(s, "too complex expression");
  }
#if 0
  if (s->irep && s->filename_index != tree->filename_index) {
    mrb_sym fname = mrb_parser_get_filename(s->parser, s->filename_index);
    const char *filename = mrb_sym_name_len(s->mrb, fname, NULL);

    mrb_debug_info_append_file(s->mrb, s->irep->debug_info,
                               filename, s->lines, s->debug_start_pos, s->pc);
    s->debug_start_pos = s->pc;
    s->filename_index = tree->filename_index;
    s->filename_sym = mrb_parser_get_filename(s->parser, tree->filename_index);
  }

  nt = nint(tree->car);
  s->lineno = tree->lineno;
  tree = tree->cdr;
#endif
  switch (node->type) {
  case YP_NODE_STRING_INTERPOLATED_NODE:
    node = (yp_node_t*)((yp_string_interpolated_node_t*)node)->statements;
    /* fall through */
  case YP_NODE_STATEMENTS_NODE: {
    yp_node_list_t body = ((yp_statements_node_t*)node)->body;
    if (body.size > 0) {
      for (size_t i=0; i < body.size-1; i++) {
        codegen(s, body.nodes[i], NOVAL);
      }
      codegen(s, body.nodes[body.size-1], val);
    } else {
      if (val) {
        genop_1(s, OP_LOADNIL, cursp());
        push();
      }
    }
    break;
  }

#if 0
  case NODE_RESCUE:
    {
      int noexc;
      uint32_t exend, pos1, pos2, tmp;
      struct loopinfo *lp;
      int catch_entry, begin, end;

      if (tree->car == NULL) goto exit;
      lp = loop_push(s, LOOP_BEGIN);
      lp->pc0 = new_label(s);
      catch_entry = catch_handler_new(s);
      begin = s->pc;
      codegen(s, tree->car, VAL);
      pop();
      lp->type = LOOP_RESCUE;
      end = s->pc;
      noexc = genjmp_0(s, OP_JMP);
      catch_handler_set(s, catch_entry, MRB_CATCH_RESCUE, begin, end, s->pc);
      tree = tree->cdr;
      exend = JMPLINK_START;
      pos1 = JMPLINK_START;
      if (tree->car) {
        node *n2 = tree->car;
        int exc = cursp();

        genop_1(s, OP_EXCEPT, exc);
        push();
        while (n2) {
          node *n3 = n2->car;
          node *n4 = n3->car;

          dispatch(s, pos1);
          pos2 = JMPLINK_START;
          do {
            if (n4 && n4->car && nint(n4->car->car) == NODE_SPLAT) {
              codegen(s, n4->car, VAL);
              gen_move(s, cursp(), exc, 0);
              push_n(2); pop_n(2); /* space for one arg and a block */
              pop();
              genop_3(s, OP_SEND, cursp(), new_sym(s, MRB_SYM_2(s->mrb, __case_eqq)), 1);
            }
            else {
              if (n4) {
                codegen(s, n4->car, VAL);
              }
              else {
                genop_2(s, OP_GETCONST, cursp(), new_sym(s, MRB_SYM_2(s->mrb, StandardError)));
                push();
              }
              pop();
              genop_2(s, OP_RESCUE, exc, cursp());
            }
            tmp = genjmp2(s, OP_JMPIF, cursp(), pos2, val);
            pos2 = tmp;
            if (n4) {
              n4 = n4->cdr;
            }
          } while (n4);
          pos1 = genjmp_0(s, OP_JMP);
          dispatch_linked(s, pos2);

          pop();
          if (n3->cdr->car) {
            gen_assignment(s, n3->cdr->car, NULL, exc, NOVAL);
          }
          if (n3->cdr->cdr->car) {
            codegen(s, n3->cdr->cdr->car, val);
            if (val) pop();
          }
          tmp = genjmp(s, OP_JMP, exend);
          exend = tmp;
          n2 = n2->cdr;
          push();
        }
        if (pos1 != JMPLINK_START) {
          dispatch(s, pos1);
          genop_1(s, OP_RAISEIF, exc);
        }
      }
      pop();
      tree = tree->cdr;
      dispatch(s, noexc);
      if (tree->car) {
        codegen(s, tree->car, val);
      }
      else if (val) {
        push();
      }
      dispatch_linked(s, exend);
      loop_pop(s, NOVAL);
    }
    break;

  case NODE_ENSURE:
    if (!tree->cdr || !tree->cdr->cdr ||
        (nint(tree->cdr->cdr->car) == NODE_BEGIN &&
         tree->cdr->cdr->cdr)) {
      int catch_entry, begin, end, target;
      int idx;

      catch_entry = catch_handler_new(s);
      begin = s->pc;
      codegen(s, tree->car, val);
      end = target = s->pc;
      push();
      idx = cursp();
      genop_1(s, OP_EXCEPT, idx);
      push();
      codegen(s, tree->cdr->cdr, NOVAL);
      pop();
      genop_1(s, OP_RAISEIF, idx);
      pop();
      catch_handler_set(s, catch_entry, MRB_CATCH_ENSURE, begin, end, target);
    }
    else {                      /* empty ensure ignored */
      codegen(s, tree->car, val);
    }
    break;
#endif

  case YP_NODE_LAMBDA_NODE:
    if (val) {
      yp_lambda_node_t *lambda = (yp_lambda_node_t*)node;
      yp_parameters_node_t *parameters = NULL;
      if (lambda->parameters)
        parameters = lambda->parameters->parameters;
      int idx = lambda_body(s, lambda->scope, parameters, lambda->statements, 1);

      genop_2(s, OP_LAMBDA, cursp(), idx);
      push();
    }
    break;

  case YP_NODE_BLOCK_NODE:
    if (val) {
      yp_block_node_t *block = (yp_block_node_t*)node;
      yp_parameters_node_t *parameters = NULL;
      if (block->parameters)
        parameters = block->parameters->parameters;
      int idx = lambda_body(s, block->scope, parameters, block->statements, 1);

      genop_2(s, OP_BLOCK, cursp(), idx);
      push();
    }
    break;

  case YP_NODE_IF_NODE:
  case YP_NODE_UNLESS_NODE:
    {
      uint32_t pos1, pos2;
      mrb_bool nil_p = FALSE;
      yp_node_t *predicate;
      yp_node_t *statements = NULL;
      yp_node_t *consequent = NULL;
      if (node->type == YP_NODE_IF_NODE) {
        yp_if_node_t *ifnode = (yp_if_node_t*)node;
        predicate = ifnode->predicate;
        statements = (yp_node_t*)ifnode->statements;
        if (ifnode->consequent) {
          mrb_assert(ifnode->consequent->type == YP_NODE_ELSE_NODE);
          yp_else_node_t *elsenode = (yp_else_node_t*)ifnode->consequent;
          consequent = (yp_node_t*)elsenode->statements;
        }
      } else {
        yp_unless_node_t *unless = (yp_unless_node_t*)node;
        predicate = unless->predicate;
        consequent = (yp_node_t*)unless->statements;
        if (unless->consequent)
          statements = (yp_node_t*)unless->consequent->statements;
      }

      if (!predicate) {
        codegen(s, consequent, val);
        goto exit;
      }
      if (true_always(predicate)) {
        codegen(s, statements, val);
        goto exit;
      }
      if (false_always(predicate)) {
        codegen(s, consequent, val);
        goto exit;
      }
      if (predicate->type == YP_NODE_CALL_NODE) {
        yp_call_node_t *call = (yp_call_node_t*)predicate;
        mrb_sym mid = yarp_sym3(s->mrb, &call->name);
        mrb_sym sym_nil_p = MRB_SYM_Q_2(s->mrb, nil);
        if (call->receiver != NULL &&
            !yarp_safe_call_p(call) &&
            mid == sym_nil_p && call->arguments == NULL) {
          nil_p = TRUE;
          codegen(s, call->receiver, VAL);
        }
      }
      if (!nil_p) {
        codegen(s, predicate, VAL);
      }
      pop();
      if (val || statements) {
        if (nil_p) {
          pos2 = genjmp2_0(s, OP_JMPNIL, cursp(), val);
          pos1 = genjmp_0(s, OP_JMP);
          dispatch(s, pos2);
        }
        else {
          pos1 = genjmp2_0(s, OP_JMPNOT, cursp(), val);
        }
        codegen(s, statements, val);
        if (val) pop();
        if (consequent || val) {
          pos2 = genjmp_0(s, OP_JMP);
          dispatch(s, pos1);
          codegen(s, consequent, val);
          dispatch(s, pos2);
        }
        else {
          dispatch(s, pos1);
        }
      }
      else {                    /* empty then-part */
        if (consequent) {
          if (nil_p) {
            pos1 = genjmp2_0(s, OP_JMPNIL, cursp(), val);
          }
          else {
            pos1 = genjmp2_0(s, OP_JMPIF, cursp(), val);
          }
          codegen(s, consequent, val);
          dispatch(s, pos1);
        }
        else if (val && !nil_p) {
          genop_1(s, OP_LOADNIL, cursp());
          push();
        }
      }
    }
    break;

  case YP_NODE_AND_NODE:
    {
      yp_and_node_t *and = (yp_and_node_t*)node;
      uint32_t pos;

      if (true_always(and->left)) {
        codegen(s, and->right, val);
        goto exit;
      }
      if (false_always(and->left)) {
        codegen(s, and->left, val);
        goto exit;
      }
      codegen(s, and->left, VAL);
      pop();
      pos = genjmp2_0(s, OP_JMPNOT, cursp(), val);
      codegen(s, and->right, val);
      dispatch(s, pos);
    }
    break;

  case YP_NODE_OR_NODE:
    {
      yp_or_node_t *or = (yp_or_node_t*)node;
      uint32_t pos;

      if (true_always(or->left)) {
        codegen(s, or->left, val);
        goto exit;
      }
      if (false_always(or->left)) {
        codegen(s, or->right, val);
        goto exit;
      }
      codegen(s, or->left, VAL);
      pop();
      pos = genjmp2_0(s, OP_JMPIF, cursp(), val);
      codegen(s, or->right, val);
      dispatch(s, pos);
    }
    break;

  case YP_NODE_WHILE_NODE:
  case YP_NODE_UNTIL_NODE:
    {
      yp_node_t *predicate;
      yp_statements_node_t *statements;
      if (node->type == YP_NODE_WHILE_NODE) {
        yp_while_node_t *whilenode = (yp_while_node_t*)node;
        predicate = whilenode->predicate;
        statements = whilenode->statements;
      } else {
        yp_until_node_t *until = (yp_until_node_t*)node;
        predicate = until->predicate;
        statements = until->statements;
      }
      if (true_always(predicate)) {
        if (node->type == YP_NODE_UNTIL_NODE) {
          if (val) {
            genop_1(s, OP_LOADNIL, cursp());
            push();
          }
          goto exit;
        }
      }
      else if (false_always(predicate)) {
        if (node->type == YP_NODE_WHILE_NODE) {
          if (val) {
            genop_1(s, OP_LOADNIL, cursp());
            push();
          }
          goto exit;
        }
      }

      uint32_t pos = JMPLINK_START;
      struct loopinfo *lp = loop_push(s, LOOP_NORMAL);

      if (!val) lp->reg = -1;
      lp->pc0 = new_label(s);
      codegen(s, predicate, VAL);
      pop();
      if (node->type == YP_NODE_WHILE_NODE) {
        pos = genjmp2_0(s, OP_JMPNOT, cursp(), NOVAL);
      }
      else {
        pos = genjmp2_0(s, OP_JMPIF, cursp(), NOVAL);
      }
      lp->pc1 = new_label(s);
      codegen(s, (yp_node_t*)statements, NOVAL);
      genjmp(s, OP_JMP, lp->pc0);
      dispatch(s, pos);
      loop_pop(s, val);
    }
    break;

  case YP_NODE_FOR_NODE:
    for_body(s, (yp_for_node_t*)node);
    if (val) push();
    break;

  case YP_NODE_CASE_NODE:
    {
      yp_case_node_t *casenode = (yp_case_node_t*)node;
      int head = 0;
      uint32_t pos1, pos2, pos3, tmp;

      pos3 = JMPLINK_START;
      if (casenode->predicate) {
        head = cursp();
        codegen(s, casenode->predicate, VAL);
      }
      for (size_t i = 0; i <= casenode->conditions.size; i++) {
        yp_node_t **conditions;
        size_t condition_count;
        yp_statements_node_t *statements;
        if (i < casenode->conditions.size) {
          yp_node_t *n = casenode->conditions.nodes[i];
          mrb_assert(n->type == YP_NODE_WHEN_NODE);
          yp_when_node_t *when = (yp_when_node_t*)n;
          conditions = when->conditions.nodes;
          condition_count = when->conditions.size;
          statements = when->statements;
        } else {
          conditions = NULL;
          condition_count = 0;
          if (casenode->consequent)
            statements = casenode->consequent->statements;
        }
        pos1 = pos2 = JMPLINK_START;
        for (size_t j = 0; j < condition_count; j++) {
          codegen(s, conditions[j], VAL);
          if (head) {
            gen_move(s, cursp(), head, 0);
            push(); push(); pop(); pop(); pop();
            if (conditions[j]->type == YP_NODE_SPLAT_NODE) {
              genop_3(s, OP_SEND, cursp(), new_sym(s, MRB_SYM_2(s->mrb, __case_eqq)), 1);
            }
            else {
              genop_3(s, OP_SEND, cursp(), new_sym(s, MRB_OPSYM_2(s->mrb, eqq)), 1);
            }
          }
          else {
            pop();
          }
          tmp = genjmp2(s, OP_JMPIF, cursp(), pos2, !head);
          pos2 = tmp;
        }
        if (condition_count > 0) {
          pos1 = genjmp_0(s, OP_JMP);
          dispatch_linked(s, pos2);
        }
        codegen(s, (yp_node_t*)statements, val);
        if (val) pop();
        tmp = genjmp(s, OP_JMP, pos3);
        pos3 = tmp;
        dispatch(s, pos1);
      }
      if (val) {
        uint32_t pos = cursp();
        genop_1(s, OP_LOADNIL, cursp());
        if (pos3 != JMPLINK_START) dispatch_linked(s, pos3);
        if (head) pop();
        if (cursp() != pos) {
          gen_move(s, cursp(), pos, 0);
        }
        push();
      }
      else {
        if (pos3 != JMPLINK_START) {
          dispatch_linked(s, pos3);
        }
        if (head) {
          pop();
        }
      }
    }
    break;

  case YP_NODE_PROGRAM_NODE: {
    yp_program_node_t *program = (yp_program_node_t*)node;
    scope_body(s, program->scope, (yp_node_t*)program->statements, NOVAL);
    break;
  }

  case YP_NODE_CALL_NODE:
    gen_call(s, (yp_call_node_t*)node, val);
    break;

  case YP_NODE_RANGE_NODE: {
    yp_range_node_t *range = (yp_range_node_t*)node;
    codegen(s, range->left, val);
    codegen(s, range->right, val);
    if (!val)
      break;
    // TODO: Add field to differentiate '..' and '...' ? Or replace location with token?
    if (range->operator_loc.end - range->operator_loc.start == 2) {
      pop(); pop();
      genop_1(s, OP_RANGE_INC, cursp());
      push();
    }
    else {
      pop(); pop();
      genop_1(s, OP_RANGE_EXC, cursp());
      push();
    }
    break;
  }

  case YP_NODE_CONSTANT_PATH_NODE: {
    yp_constant_path_node_t *path = (yp_constant_path_node_t*)node;
    mrb_assert(path->child->type == YP_NODE_CONSTANT_READ_NODE);
    if (path->parent) {
      int sym = new_sym(s, yarp_sym2(s->mrb, path->child->location));

      codegen(s, path->parent, VAL);
      pop();
      genop_2(s, OP_GETMCNST, cursp(), sym);
      if (val) push();
    }
    else {
      int sym = new_sym(s, yarp_sym2(s->mrb, path->child->location));

      genop_1(s, OP_OCLASS, cursp());
      genop_2(s, OP_GETMCNST, cursp(), sym);
      if (val) push();
    }
    break;
  }

  case YP_NODE_ARRAY_NODE:
    {
      yp_array_node_t *array = (yp_array_node_t*)node;
      int n;

      n = gen_values(s, array->elements.nodes, array->elements.size, val, 0);
      if (val) {
        if (n >= 0) {
          pop_n(n);
          genop_2(s, OP_ARRAY, cursp(), n);
        }
        push();
      }
    }
    break;

  case YP_NODE_HASH_NODE:
    {
      yp_hash_node_t *hash = (yp_hash_node_t*)node;
      int nk = gen_hash(s, hash->elements.nodes, hash->elements.size, val, GEN_LIT_ARY_MAX);
      if (val && nk >= 0) {
        pop_n(nk*2);
        genop_2(s, OP_HASH, cursp(), nk);
        push();
      }
    }
    break;

  case YP_NODE_SPLAT_NODE:
    codegen(s, ((yp_splat_node_t*)node)->expression, val);
    break;

  case YP_NODE_GLOBAL_VARIABLE_WRITE_NODE:
    gen_assignment(s, node, ((yp_global_variable_write_node_t*)node)->value, 0, val);
    break;
  case YP_NODE_LOCAL_VARIABLE_WRITE_NODE:
    gen_assignment(s, node, ((yp_local_variable_write_node_t*)node)->value, 0, val);
    break;
  case YP_NODE_INSTANCE_VARIABLE_WRITE_NODE:
    gen_assignment(s, node, ((yp_instance_variable_write_node_t*)node)->value, 0, val);
    break;
  case YP_NODE_CLASS_VARIABLE_WRITE_NODE:
    gen_assignment(s, node, ((yp_class_variable_write_node_t*)node)->value, 0, val);
    break;
  case YP_NODE_CONSTANT_PATH_WRITE_NODE:
    gen_assignment(s, node, ((yp_constant_path_write_node_t*)node)->value, 0, val);
    break;

  case YP_NODE_MULTI_WRITE_NODE:
    {
      yp_multi_write_node_t *write = (yp_multi_write_node_t*)node;
      yp_node_list_t targets = write->targets;
      int len = 0, n = 0, post = 0;
      yp_node_t *t = write->value;
      int rhs = cursp();

      if (!val && t->type == YP_NODE_ARRAY_NODE && nosplat((yp_array_node_t*)t)) {
        /* fixed rhs */
        yp_array_node_t *array = (yp_array_node_t*)t;
        for (size_t i = 0; i < array->elements.size; i++) {
          codegen(s, array->elements.nodes[i], VAL);
          len++;
        }
        int idx = 0, pre = 0;
        yp_splat_node_t *splat = NULL;
        count_multi_write(targets, &pre, &splat, &post);
        if (pre > 0) {
          n = 0;
          while (idx < pre) {
            if (n < len) {
              gen_assignment(s, targets.nodes[idx], NULL, rhs+n, NOVAL);
              n++;
            }
            else {
              genop_1(s, OP_LOADNIL, rhs+n);
              gen_assignment(s, targets.nodes[idx], NULL, rhs+n, NOVAL);
            }
            idx++;
          }
        }
        if (true) {
          if (splat) {
            int rn;

            if (len < post + n) {
              rn = 0;
            }
            else {
              rn = len - post - n;
            }
            if (cursp() == rhs+n) {
              genop_2(s, OP_ARRAY, cursp(), rn);
            }
            else {
              genop_3(s, OP_ARRAY2, cursp(), rhs+n, rn);
            }
            gen_assignment(s, splat->expression, NULL, cursp(), NOVAL);
            n += rn;
          }
          if (post > 0) {
            while (idx < targets.size) {
              if (n<len) {
                gen_assignment(s, targets.nodes[idx], NULL, rhs+n, NOVAL);
              }
              else {
                genop_1(s, OP_LOADNIL, cursp());
                gen_assignment(s, targets.nodes[idx], NULL, cursp(), NOVAL);
              }
              idx++;
              n++;
            }
          }
        }
        pop_n(len);
      }
      else {
        /* variable rhs */
        codegen(s, t, VAL);
        gen_massignment(s, targets, rhs, val);
        if (!val) {
          pop();
        }
      }
    }
    break;

#if 0
  case NODE_OP_ASGN:
    {
      mrb_sym sym = nsym(tree->cdr->car);
      mrb_int len;
      const char *name = mrb_sym_name_len(s->mrb, sym, &len);
      int idx, callargs = -1, vsp = -1;

      if ((len == 2 && name[0] == '|' && name[1] == '|') &&
          (nint(tree->car->car) == NODE_CONST ||
           nint(tree->car->car) == NODE_CVAR)) {
        int catch_entry, begin, end;
        int noexc, exc;
        struct loopinfo *lp;

        lp = loop_push(s, LOOP_BEGIN);
        lp->pc0 = new_label(s);
        catch_entry = catch_handler_new(s);
        begin = s->pc;
        exc = cursp();
        codegen(s, tree->car, VAL);
        end = s->pc;
        noexc = genjmp_0(s, OP_JMP);
        lp->type = LOOP_RESCUE;
        catch_handler_set(s, catch_entry, MRB_CATCH_RESCUE, begin, end, s->pc);
        genop_1(s, OP_EXCEPT, exc);
        genop_1(s, OP_LOADF, exc);
        dispatch(s, noexc);
        loop_pop(s, NOVAL);
      }
      else if (nint(tree->car->car) == NODE_CALL) {
        node *n = tree->car->cdr;
        int base, i, nargs = 0;
        callargs = 0;

        if (val) {
          vsp = cursp();
          push();
        }
        codegen(s, n->car, VAL);   /* receiver */
        idx = new_sym(s, nsym(n->cdr->car));
        base = cursp()-1;
        if (n->cdr->cdr->car) {
          nargs = gen_values(s, n->cdr->cdr->car->car, VAL, 13);
          if (nargs >= 0) {
            callargs = nargs;
          }
          else { /* varargs */
            push();
            nargs = 1;
            callargs = CALL_MAXARGS;
          }
        }
        /* copy receiver and arguments */
        gen_move(s, cursp(), base, 1);
        for (i=0; i<nargs; i++) {
          gen_move(s, cursp()+i+1, base+i+1, 1);
        }
        push_n(nargs+2);pop_n(nargs+2); /* space for receiver, arguments and a block */
        genop_3(s, OP_SEND, cursp(), idx, callargs);
        push();
      }
      else {
        codegen(s, tree->car, VAL);
      }
      if (len == 2 &&
          ((name[0] == '|' && name[1] == '|') ||
           (name[0] == '&' && name[1] == '&'))) {
        uint32_t pos;

        pop();
        if (val) {
          if (vsp >= 0) {
            gen_move(s, vsp, cursp(), 1);
          }
          pos = genjmp2_0(s, name[0]=='|'?OP_JMPIF:OP_JMPNOT, cursp(), val);
        }
        else {
          pos = genjmp2_0(s, name[0]=='|'?OP_JMPIF:OP_JMPNOT, cursp(), val);
        }
        codegen(s, tree->cdr->cdr->car, VAL);
        pop();
        if (val && vsp >= 0) {
          gen_move(s, vsp, cursp(), 1);
        }
        if (nint(tree->car->car) == NODE_CALL) {
          if (callargs == CALL_MAXARGS) {
            pop();
            genop_2(s, OP_ARYPUSH, cursp(), 1);
          }
          else {
            pop_n(callargs);
            callargs++;
          }
          pop();
          idx = new_sym(s, attrsym(s, nsym(tree->car->cdr->cdr->car)));
          genop_3(s, OP_SEND, cursp(), idx, callargs);
        }
        else {
          gen_assignment(s, tree->car, NULL, cursp(), val);
        }
        dispatch(s, pos);
        goto exit;
      }
      codegen(s, tree->cdr->cdr->car, VAL);
      push(); pop();
      pop(); pop();

      if (len == 1 && name[0] == '+')  {
        gen_addsub(s, OP_ADD, cursp());
      }
      else if (len == 1 && name[0] == '-')  {
        gen_addsub(s, OP_SUB, cursp());
      }
      else if (len == 1 && name[0] == '*')  {
        genop_1(s, OP_MUL, cursp());
      }
      else if (len == 1 && name[0] == '/')  {
        genop_1(s, OP_DIV, cursp());
      }
      else if (len == 1 && name[0] == '<')  {
        genop_1(s, OP_LT, cursp());
      }
      else if (len == 2 && name[0] == '<' && name[1] == '=')  {
        genop_1(s, OP_LE, cursp());
      }
      else if (len == 1 && name[0] == '>')  {
        genop_1(s, OP_GT, cursp());
      }
      else if (len == 2 && name[0] == '>' && name[1] == '=')  {
        genop_1(s, OP_GE, cursp());
      }
      else {
        idx = new_sym(s, sym);
        genop_3(s, OP_SEND, cursp(), idx, 1);
      }
      if (callargs < 0) {
        gen_assignment(s, tree->car, NULL, cursp(), val);
      }
      else {
        if (val && vsp >= 0) {
          gen_move(s, vsp, cursp(), 0);
        }
        if (callargs == CALL_MAXARGS) {
          pop();
          genop_2(s, OP_ARYPUSH, cursp(), 1);
        }
        else {
          pop_n(callargs);
          callargs++;
        }
        pop();
        idx = new_sym(s, attrsym(s,nsym(tree->car->cdr->cdr->car)));
        genop_3(s, OP_SEND, cursp(), idx, callargs);
      }
    }
    break;

  case NODE_SUPER:
    {
      codegen_scope *s2 = s;
      int lv = 0;
      int n = 0, nk = 0, st = 0;

      push();
      while (!s2->mscope) {
        lv++;
        s2 = s2->prev;
        if (!s2) break;
      }
      if (tree) {
        node *args = tree->car;
        if (args) {
          st = n = gen_values(s, args, VAL, 14);
          if (n < 0) {
            st = 1; n = 15;
            push();
          }
        }
        /* keyword arguments */
        if (tree->cdr->car) {
          nk = gen_hash(s, tree->cdr->car->cdr, VAL, 14);
          if (nk < 0) {st++; nk = 15;}
          else st += nk*2;
          n |= nk<<4;
        }
        /* block arguments */
        if (tree->cdr->cdr) {
          codegen(s, tree->cdr->cdr, VAL);
        }
        else if (s2) gen_blkmove(s, s2->ainfo, lv);
        else {
          genop_1(s, OP_LOADNIL, cursp());
          push();
        }
      }
      else {
        if (s2) gen_blkmove(s, s2->ainfo, lv);
        else {
          genop_1(s, OP_LOADNIL, cursp());
          push();
        }
      }
      st++;
      pop_n(st+1);
      genop_2(s, OP_SUPER, cursp(), n);
      if (val) push();
    }
    break;

  case NODE_ZSUPER:
    {
      codegen_scope *s2 = s;
      int lv = 0;
      uint16_t ainfo = 0;
      int n = CALL_MAXARGS;
      int sp = cursp();

      push();        /* room for receiver */
      while (!s2->mscope) {
        lv++;
        s2 = s2->prev;
        if (!s2) break;
      }
      if (s2 && s2->ainfo > 0) {
        ainfo = s2->ainfo;
      }
      if (ainfo > 0) {
        genop_2S(s, OP_ARGARY, cursp(), (ainfo<<4)|(lv & 0xf));
        push(); push(); push();   /* ARGARY pushes 3 values at most */
        pop(); pop(); pop();
        /* keyword arguments */
        if (ainfo & 0x1) {
          n |= CALL_MAXARGS<<4;
          push();
        }
        /* block argument */
        if (tree && tree->cdr && tree->cdr->cdr) {
          push();
          codegen(s, tree->cdr->cdr, VAL);
        }
      }
      else {
        /* block argument */
        if (tree && tree->cdr && tree->cdr->cdr) {
          codegen(s, tree->cdr->cdr, VAL);
        }
        else {
          gen_blkmove(s, 0, lv);
        }
        n = 0;
      }
      s->sp = sp;
      genop_2(s, OP_SUPER, cursp(), n);
      if (val) push();
    }
    break;

  case NODE_RETURN:
    if (tree) {
      gen_retval(s, tree);
    }
    else {
      genop_1(s, OP_LOADNIL, cursp());
    }
    if (s->loop) {
      gen_return(s, OP_RETURN_BLK, cursp());
    }
    else {
      gen_return(s, OP_RETURN, cursp());
    }
    if (val) push();
    break;

  case NODE_YIELD:
    {
      codegen_scope *s2 = s;
      int lv = 0, ainfo = -1;
      int n = 0, sendv = 0;

      while (!s2->mscope) {
        lv++;
        s2 = s2->prev;
        if (!s2) break;
      }
      if (s2) {
        ainfo = (int)s2->ainfo;
      }
      if (ainfo < 0) codegen_error(s, "invalid yield (SyntaxError)");
      push();
      if (tree) {
        n = gen_values(s, tree, VAL, 14);
        if (n < 0) {
          n = sendv = 1;
          push();
        }
      }
      push();pop(); /* space for a block */
      pop_n(n+1);
      genop_2S(s, OP_BLKPUSH, cursp(), (ainfo<<4)|(lv & 0xf));
      if (sendv) n = CALL_MAXARGS;
      genop_3(s, OP_SEND, cursp(), new_sym(s, MRB_SYM_2(s->mrb, call)), n);
      if (val) push();
    }
    break;

  case NODE_BREAK:
    loop_break(s, tree);
    if (val) push();
    break;

  case NODE_NEXT:
    if (!s->loop) {
      raise_error(s, "unexpected next");
    }
    else if (s->loop->type == LOOP_NORMAL) {
      codegen(s, tree, NOVAL);
      genjmp(s, OP_JMPUW, s->loop->pc0);
    }
    else {
      if (tree) {
        codegen(s, tree, VAL);
        pop();
      }
      else {
        genop_1(s, OP_LOADNIL, cursp());
      }
      gen_return(s, OP_RETURN, cursp());
    }
    if (val) push();
    break;

  case NODE_REDO:
    if (!s->loop || s->loop->type == LOOP_BEGIN || s->loop->type == LOOP_RESCUE) {
      raise_error(s, "unexpected redo");
    }
    else {
      genjmp(s, OP_JMPUW, s->loop->pc1);
    }
    if (val) push();
    break;

  case NODE_RETRY:
    {
      const char *msg = "unexpected retry";
      const struct loopinfo *lp = s->loop;

      while (lp && lp->type != LOOP_RESCUE) {
        lp = lp->prev;
      }
      if (!lp) {
        raise_error(s, msg);
      }
      else {
        genjmp(s, OP_JMPUW, lp->pc0);
      }
      if (val) push();
    }
    break;
#endif

  case YP_NODE_LOCAL_VARIABLE_READ_NODE:
    if (val) {
      mrb_sym name = yarp_sym2(s->mrb, node->location);
      int idx = lv_idx(s, name);

      if (idx > 0) {
        gen_move(s, cursp(), idx, val);
      }
      else {
        gen_getupvar(s, cursp(), name);
      }
      push();
    }
    break;

#if 0
  case NODE_NVAR:
    if (val) {
      int idx = nint(tree);

      gen_move(s, cursp(), idx, val);

      push();
    }
    break;
#endif

  case YP_NODE_GLOBAL_VARIABLE_READ_NODE:
    {
      int sym = new_sym(s, yarp_sym2(s->mrb, node->location));

      genop_2(s, OP_GETGV, cursp(), sym);
      if (val) push();
    }
    break;

  case YP_NODE_INSTANCE_VARIABLE_READ_NODE:
    {
      int sym = new_sym(s, yarp_sym2(s->mrb, node->location));

      genop_2(s, OP_GETIV, cursp(), sym);
      if (val) push();
    }
    break;

  case YP_NODE_CLASS_VARIABLE_READ_NODE:
    {
      int sym = new_sym(s, yarp_sym2(s->mrb, node->location));

      genop_2(s, OP_GETCV, cursp(), sym);
      if (val) push();
    }
    break;

  case YP_NODE_CONSTANT_READ_NODE:
    {
      int sym = new_sym(s, yarp_sym2(s->mrb, node->location));

      genop_2(s, OP_GETCONST, cursp(), sym);
      if (val) push();
    }
    break;

#if 0
  case NODE_BACK_REF:
    if (val) {
      char buf[] = {'$', nchar(tree)};
      int sym = new_sym(s, mrb_intern(s->mrb, buf, sizeof(buf)));

      genop_2(s, OP_GETGV, cursp(), sym);
      push();
    }
    break;

  case NODE_NTH_REF:
    if (val) {
      mrb_state *mrb = s->mrb;
      mrb_value str;
      int sym;

      str = mrb_format(mrb, "$%d", nint(tree));
      sym = new_sym(s, mrb_intern_str(mrb, str));
      genop_2(s, OP_GETGV, cursp(), sym);
      push();
    }
    break;

  case NODE_ARG:
    /* should not happen */
    break;
#endif

  case YP_NODE_BLOCK_ARGUMENT_NODE: {
    yp_block_argument_node_t *argument = (yp_block_argument_node_t*)node;
    if (!argument->expression) {
      int idx = lv_idx(s, MRB_OPSYM_2(s->mrb, and));

      if (idx == 0) {
        gen_getupvar(s, cursp(), MRB_OPSYM_2(s->mrb, and));
      }
      else {
        gen_move(s, cursp(), idx, val);
      }
      if (val) push();
    }
    else {
      codegen(s, argument->expression, val);
    }
    break;
  }

  case YP_NODE_INTEGER_NODE:
    if (val) {
      const char *p = node->location.start;
      const char *e = node->location.end;
      int base = 10; // TODO
      mrb_int i;
      mrb_bool overflow;

      i = readint(s, p, e, base, FALSE, &overflow);
      if (overflow) {
        int off = new_litbint(s, p, base, FALSE);
        genop_2(s, OP_LOADL, cursp(), off);
      }
      else {
        gen_int(s, cursp(), i);
      }
      push();
    }
    break;

#ifndef MRB_NO_FLOAT
  case YP_NODE_FLOAT_NODE:
    if (val) {
      const char *p = node->location.start;
      double f;
      mrb_read_float(p, NULL, &f);
      int off = new_lit_float(s, (mrb_float)f);

      genop_2(s, OP_LOADL, cursp(), off);
      push();
    }
    break;
#endif

#if 0
  case NODE_NEGATE:
    {
      nt = nint(tree->car);
      switch (nt) {
#ifndef MRB_NO_FLOAT
      case NODE_FLOAT:
        if (val) {
          char *p = (char*)tree->cdr;
          double f;
          mrb_read_float(p, NULL, &f);
          int off = new_lit_float(s, (mrb_float)-f);

          genop_2(s, OP_LOADL, cursp(), off);
          push();
        }
        break;
#endif

      case NODE_INT:
        if (val) {
          char *p = (char*)tree->cdr->car;
          int base = nint(tree->cdr->cdr->car);
          mrb_int i;
          mrb_bool overflow;

          i = readint(s, p, base, TRUE, &overflow);
          if (overflow) {
            int off = new_litbint(s, p, base, TRUE);
            genop_2(s, OP_LOADL, cursp(), off);
          }
          else {
            gen_int(s, cursp(), i);
          }
          push();
        }
        break;

      default:
        if (val) {
          codegen(s, tree, VAL);
          pop();
          push_n(2);pop_n(2); /* space for receiver&block */
          mrb_sym minus = MRB_OPSYM_2(s->mrb, minus);
          if (!gen_uniop(s, minus, cursp())) {
            genop_3(s, OP_SEND, cursp(), new_sym(s, minus), 0);
          }
          push();
        }
        else {
          codegen(s, tree, NOVAL);
        }
        break;
      }
    }
    break;
#endif

  case YP_NODE_STRING_NODE:
    if (val) {
      yp_string_node_t *string = (yp_string_node_t*)node;
      const char *p = string->content.start;
      mrb_int len = string->content.end - p;
      int off = new_lit_str(s, p, len);

      genop_2(s, OP_STRING, cursp(), off);
      push();
    }
    break;

#if 0
  case NODE_HEREDOC:
    tree = ((struct mrb_parser_heredoc_info*)tree)->doc;
    /* fall through */
#endif
  case YP_NODE_INTERPOLATED_STRING_NODE:
    if (val) {
      yp_interpolated_string_node_t *string = (yp_interpolated_string_node_t*)node;

      if (string->parts.size == 0) {
        genop_1(s, OP_LOADNIL, cursp());
        push();
        break;
      }
      size_t i = 0;
      if (string->parts.nodes[0]->type == YP_NODE_STRING_NODE) {
        codegen(s, string->parts.nodes[0], VAL);
        i++;
      } else {
        genop_2(s, OP_STRING, cursp(), new_lit_str(s, "", 0));
        push();
      }
      while (i < string->parts.size) {
        codegen(s, string->parts.nodes[i], VAL);
        pop(); pop();
        genop_1(s, OP_STRCAT, cursp());
        push();
        i++;
      }
    }
    else {
      yp_interpolated_string_node_t *string = (yp_interpolated_string_node_t*)node;

      for (size_t i = 0; i < string->parts.size; i++) {
        if (string->parts.nodes[i]->type != YP_NODE_STRING_NODE) {
          codegen(s, string->parts.nodes[i], NOVAL);
        }
      }
    }
    break;

#if 0
  case NODE_WORDS:
    gen_literal_array(s, tree, FALSE, val);
    break;

  case NODE_SYMBOLS:
    gen_literal_array(s, tree, TRUE, val);
    break;
#endif

  case YP_NODE_INTERPOLATED_X_STRING_NODE:
    {
      yp_interpolated_x_string_node_t *xstring = (yp_interpolated_x_string_node_t*)node;
      int sym = new_sym(s, MRB_SYM_2(s->mrb, Kernel));

      genop_1(s, OP_LOADSELF, cursp());
      push();
      size_t i = 0;
      if (xstring->parts.nodes[0]->type == YP_NODE_STRING_NODE) {
        codegen(s, xstring->parts.nodes[0], VAL);
        i++;
      } else {
        genop_2(s, OP_STRING, cursp(), new_lit_str(s, "", 0));
        push();
      }
      while (i < xstring->parts.size) {
        codegen(s, xstring->parts.nodes[i], VAL);
        pop(); pop();
        genop_1(s, OP_STRCAT, cursp());
        push();
        i++;
      }
      push();                   /* for block */
      pop_n(3);
      sym = new_sym(s, MRB_OPSYM_2(s->mrb, tick)); /* ` */
      genop_3(s, OP_SEND, cursp(), sym, 1);
      if (val) push();
    }
    break;

  case YP_NODE_X_STRING_NODE:
    {
      yp_x_string_node_t *xstring = (yp_x_string_node_t*)node;
      const char *p = xstring->content.start;
      mrb_int len = xstring->content.end - p;
      int off = new_lit_str(s, p, len);
      int sym;

      genop_1(s, OP_LOADSELF, cursp());
      push();
      genop_2(s, OP_STRING, cursp(), off);
      push(); push();
      pop_n(3);
      sym = new_sym(s, MRB_OPSYM_2(s->mrb, tick)); /* ` */
      genop_3(s, OP_SEND, cursp(), sym, 1);
      if (val) push();
    }
    break;

#if 0
  case NODE_REGX:
    if (val) {
      char *p1 = (char*)tree->car;
      char *p2 = (char*)tree->cdr->car;
      char *p3 = (char*)tree->cdr->cdr;
      int sym = new_sym(s, mrb_intern_lit(s->mrb, REGEXP_CLASS));
      int off = new_lit_cstr(s, p1);
      int argc = 1;

      genop_1(s, OP_OCLASS, cursp());
      genop_2(s, OP_GETMCNST, cursp(), sym);
      push();
      genop_2(s, OP_STRING, cursp(), off);
      push();
      if (p2 || p3) {
        if (p2) { /* opt */
          off = new_lit_cstr(s, p2);
          genop_2(s, OP_STRING, cursp(), off);
        }
        else {
          genop_1(s, OP_LOADNIL, cursp());
        }
        push();
        argc++;
        if (p3) { /* enc */
          off = new_lit_str(s, p3, 1);
          genop_2(s, OP_STRING, cursp(), off);
          push();
          argc++;
        }
      }
      push(); /* space for a block */
      pop_n(argc+2);
      sym = new_sym(s, MRB_SYM_2(s->mrb, compile));
      genop_3(s, OP_SEND, cursp(), sym, argc);
      push();
    }
    break;

  case NODE_DREGX:
    if (val) {
      node *n = tree->car;
      int sym = new_sym(s, mrb_intern_lit(s->mrb, REGEXP_CLASS));
      int argc = 1;
      int off;
      char *p;

      genop_1(s, OP_OCLASS, cursp());
      genop_2(s, OP_GETMCNST, cursp(), sym);
      push();
      codegen(s, n->car, VAL);
      n = n->cdr;
      while (n) {
        codegen(s, n->car, VAL);
        pop(); pop();
        genop_1(s, OP_STRCAT, cursp());
        push();
        n = n->cdr;
      }
      n = tree->cdr->cdr;
      if (n->car) { /* tail */
        p = (char*)n->car;
        off = new_lit_cstr(s, p);
        codegen(s, tree->car, VAL);
        genop_2(s, OP_STRING, cursp(), off);
        pop();
        genop_1(s, OP_STRCAT, cursp());
        push();
      }
      if (n->cdr->car) { /* opt */
        char *p2 = (char*)n->cdr->car;
        off = new_lit_cstr(s, p2);
        genop_2(s, OP_STRING, cursp(), off);
        push();
        argc++;
      }
      if (n->cdr->cdr) { /* enc */
        char *p2 = (char*)n->cdr->cdr;
        off = new_lit_cstr(s, p2);
        genop_2(s, OP_STRING, cursp(), off);
        push();
        argc++;
      }
      push(); /* space for a block */
      pop_n(argc+2);
      sym = new_sym(s, MRB_SYM_2(s->mrb, compile));
      genop_3(s, OP_SEND, cursp(), sym, argc);
      push();
    }
    else {
      node *n = tree->car;

      while (n) {
        if (nint(n->car->car) != NODE_STR) {
          codegen(s, n->car, NOVAL);
        }
        n = n->cdr;
      }
    }
    break;
#endif

  case YP_NODE_SYMBOL_NODE:
    if (val) {
      int sym = new_sym(s, yarp_sym(s->mrb, ((yp_symbol_node_t*)node)->value));

      genop_2(s, OP_LOADSYM, cursp(), sym);
      push();
    }
    break;

  case YP_NODE_INTERPOLATED_SYMBOL_NODE: {
    yp_interpolated_string_node_t string = {.base = {.type = YP_NODE_INTERPOLATED_STRING_NODE}, .parts = ((yp_interpolated_symbol_node_t*)node)->parts};
    codegen(s, (yp_node_t*)&string, val);
    if (val) {
      gen_intern(s);
    }
    break;
  }

  case YP_NODE_SELF_NODE:
    if (val) {
      genop_1(s, OP_LOADSELF, cursp());
      push();
    }
    break;

  case YP_NODE_NIL_NODE:
    if (val) {
      genop_1(s, OP_LOADNIL, cursp());
      push();
    }
    break;

  case YP_NODE_TRUE_NODE:
    if (val) {
      genop_1(s, OP_LOADT, cursp());
      push();
    }
    break;

  case YP_NODE_FALSE_NODE:
    if (val) {
      genop_1(s, OP_LOADF, cursp());
      push();
    }
    break;

  case YP_NODE_ALIAS_NODE:
    {
      yp_alias_node_t *alias = (yp_alias_node_t*)node;
      if (alias->new_name->type != YP_NODE_SYMBOL_NODE || alias->old_name->type != YP_NODE_SYMBOL_NODE) {
        codegen_error(s, "alias only supports simple symbols");
      }
      int a = new_sym(s, yarp_sym(s->mrb, ((yp_symbol_node_t*)alias->new_name)->value));
      int b = new_sym(s, yarp_sym(s->mrb, ((yp_symbol_node_t*)alias->old_name)->value));

      genop_2(s, OP_ALIAS, a, b);
      if (val) {
        genop_1(s, OP_LOADNIL, cursp());
        push();
      }
    }
   break;

  case YP_NODE_UNDEF_NODE:
    {
      yp_undef_node_t *undef = (yp_undef_node_t*)node;

      for (size_t i = 0; i < undef->names.size; i++) {
        yp_node_t *n = undef->names.nodes[i];
        if (n->type != YP_NODE_SYMBOL_NODE) {
          codegen_error(s, "undef only supports simple symbols");
        }
        int symbol = new_sym(s, yarp_sym(s->mrb, ((yp_symbol_node_t*)n)->value));
        genop_1(s, OP_UNDEF, symbol);
      }
      if (val) {
        genop_1(s, OP_LOADNIL, cursp());
        push();
      }
    }
    break;

  case YP_NODE_CLASS_NODE:
    {
      yp_class_node_t *class = (yp_class_node_t*)node;
      yp_constant_path_node_t *path = NULL;
      yp_node_t *name = class->constant_path;
      if (name->type == YP_NODE_CONSTANT_PATH_NODE) {
        path = (yp_constant_path_node_t*)name;
        name = path->child;
      }
      mrb_assert(name->type == YP_NODE_CONSTANT_READ_NODE);
      int idx;

      if (path && path->parent == NULL) {
        genop_1(s, OP_LOADNIL, cursp());
        push();
      }
      else if (path == NULL) {
        genop_1(s, OP_OCLASS, cursp());
        push();
      }
      else {
        codegen(s, path->parent, VAL);
      }
      if (class->superclass) {
        codegen(s, class->superclass, VAL);
      }
      else {
        genop_1(s, OP_LOADNIL, cursp());
        push();
      }
      pop(); pop();
      idx = new_sym(s, yarp_sym2(s->mrb, name->location));
      genop_2(s, OP_CLASS, cursp(), idx);
      if (class->statements == NULL) {
        genop_1(s, OP_LOADNIL, cursp());
      }
      else {
        idx = scope_body(s, class->scope, class->statements, val);
        genop_2(s, OP_EXEC, cursp(), idx);
      }
      if (val) {
        push();
      }
    }
    break;

  case YP_NODE_MODULE_NODE:
    {
      yp_module_node_t *module = (yp_module_node_t*)node;
      yp_constant_path_node_t *path = NULL;
      yp_node_t *name = module->constant_path;
      if (name->type == YP_NODE_CONSTANT_PATH_NODE) {
        path = (yp_constant_path_node_t*)name;
        name = path->child;
      }
      mrb_assert(name->type == YP_NODE_CONSTANT_READ_NODE);
      int idx;

      if (path && path->parent == NULL) {
        genop_1(s, OP_LOADNIL, cursp());
        push();
      }
      else if (path == NULL) {
        genop_1(s, OP_OCLASS, cursp());
        push();
      }
      else {
        codegen(s, path->parent, VAL);
      }
      pop();
      idx = new_sym(s, yarp_sym2(s->mrb, name->location));
      genop_2(s, OP_MODULE, cursp(), idx);
      if (module->statements == NULL) {
        genop_1(s, OP_LOADNIL, cursp());
      }
      else {
        idx = scope_body(s, module->scope, module->statements, val);
        genop_2(s, OP_EXEC, cursp(), idx);
      }
      if (val) {
        push();
      }
    }
    break;

  case YP_NODE_SINGLETON_CLASS_NODE:
    {
      yp_singleton_class_node_t *sclass = (yp_singleton_class_node_t*)node;
      int idx;

      codegen(s, sclass->expression, VAL);
      pop();
      genop_1(s, OP_SCLASS, cursp());
      if (sclass->statements == NULL) {
        genop_1(s, OP_LOADNIL, cursp());
      }
      else {
        idx = scope_body(s, sclass->scope, sclass->statements, val);
        genop_2(s, OP_EXEC, cursp(), idx);
      }
      if (val) {
        push();
      }
    }
    break;

  case YP_NODE_DEF_NODE:
    {
      yp_def_node_t *def = (yp_def_node_t*)node;
      if (def->receiver != NULL) { assert(false); } // TODO
      int sym = new_sym(s, yarp_sym(s->mrb, def->name));
      int idx = lambda_body(s, def->scope, def->parameters, def->statements, 0);

      genop_1(s, OP_TCLASS, cursp());
      push();
      genop_2(s, OP_METHOD, cursp(), idx);
      push(); pop();
      pop();
      genop_2(s, OP_DEF, cursp(), sym);
      if (val) push();
    }
    break;

#if 0
  case NODE_SDEF:
    {
      node *recv = tree->car;
      int sym = new_sym(s, nsym(tree->cdr->car));
      int idx = lambda_body(s, tree->cdr->cdr, 0);

      codegen(s, recv, VAL);
      pop();
      genop_1(s, OP_SCLASS, cursp());
      push();
      genop_2(s, OP_METHOD, cursp(), idx);
      push(); pop();
      pop();
      genop_2(s, OP_DEF, cursp(), sym);
      if (val) push();
    }
    break;

  case NODE_POSTEXE:
    codegen(s, tree, NOVAL);
    break;
#endif

  default:
    fprintf(stderr, "unsupported: %d\n", node->type);
    exit(1);
    break;
  }
 exit:
  s->rlev = rlev;
}

static void
scope_add_irep(codegen_scope *s)
{
  mrb_irep *irep;
  codegen_scope *prev = s->prev;

  if (prev->irep == NULL) {
    irep = mrb_add_irep(s->mrb);
    prev->irep = s->irep = irep;
    return;
  }
  else {
    if (prev->irep->rlen == UINT16_MAX) {
      codegen_error(s, "too many nested blocks/methods");
    }
    s->irep = irep = mrb_add_irep(s->mrb);
    if (prev->irep->rlen == prev->rcapa) {
      prev->rcapa *= 2;
      prev->reps = (mrb_irep**)codegen_realloc(s, prev->reps, sizeof(mrb_irep*)*prev->rcapa);
    }
    prev->reps[prev->irep->rlen] = irep;
    prev->irep->rlen++;
  }
}

static codegen_scope*
scope_new(mrb_state *mrb, mrbc_context *cxt, codegen_scope *prev, mrb_sym *nlv, size_t lvsize)
{
  static const codegen_scope codegen_scope_zero = { 0 };
  mrb_pool *pool = mrb_pool_open(mrb);
  codegen_scope *s = (codegen_scope*)mrb_pool_alloc(pool, sizeof(codegen_scope));

  if (!s) {
    if (prev)
      codegen_error(prev, "unexpected scope");
    return NULL;
  }
  *s = codegen_scope_zero;
  s->mrb = mrb;
  s->cxt = cxt;
  s->mpool = pool;
  if (!prev) return s;
  s->prev = prev;
  s->ainfo = 0;
  s->mscope = 0;

  scope_add_irep(s);

  s->rcapa = 8;
  s->reps = (mrb_irep**)mrb_malloc(mrb, sizeof(mrb_irep*)*s->rcapa);

  s->icapa = 1024;
  s->iseq = (mrb_code*)mrb_malloc(mrb, sizeof(mrb_code)*s->icapa);

  s->pcapa = 32;
  s->pool = (mrb_pool_value*)mrb_malloc(mrb, sizeof(mrb_pool_value)*s->pcapa);

  s->scapa = 256;
  s->syms = (mrb_sym*)mrb_malloc(mrb, sizeof(mrb_sym)*s->scapa);

  s->lv = nlv;
  s->lvsize = lvsize;
  s->sp += lvsize+1;               /* add self */
  s->nlocals = s->sp;
  if (nlv) {
    mrb_sym *lv;
    size_t i = 0;

    s->irep->lv = lv = (mrb_sym*)mrb_malloc(mrb, sizeof(mrb_sym)*(s->nlocals-1));
    for (i=0; i < lvsize; i++) {
      lv[i] = nlv[i];
    }
    mrb_assert(i + 1 == s->nlocals);
  }
  s->ai = mrb_gc_arena_save(mrb);

#if 0
  s->filename_sym = prev->filename_sym;
  if (s->filename_sym) {
    s->lines = (uint16_t*)mrb_malloc(mrb, sizeof(short)*s->icapa);
  }
  s->lineno = prev->lineno;

  /* debug setting */
  s->debug_start_pos = 0;
  if (s->filename_sym) {
    mrb_debug_info_alloc(mrb, s->irep);
  }
  else {
#endif
    s->irep->debug_info = NULL;
#if 0
  }
#endif
  s->parser = prev->parser;
#if 0
  s->filename_index = prev->filename_index;
#endif

  s->rlev = prev->rlev+1;

  return s;
}

static void
scope_finish(codegen_scope *s)
{
  mrb_state *mrb = s->mrb;
  mrb_irep *irep = s->irep;

  if (s->nlocals > 0xff) {
    codegen_error(s, "too many local variables");
  }
  irep->flags = 0;
  if (s->iseq) {
    size_t catchsize = sizeof(struct mrb_irep_catch_handler) * irep->clen;
    irep->iseq = (const mrb_code*)codegen_realloc(s, s->iseq, sizeof(mrb_code)*s->pc + catchsize);
    irep->ilen = s->pc;
    if (irep->clen > 0) {
      memcpy((void*)(irep->iseq + irep->ilen), s->catch_table, catchsize);
    }
  }
  else {
    irep->clen = 0;
  }
  mrb_free(s->mrb, s->catch_table);
  s->catch_table = NULL;
  irep->pool = (const mrb_pool_value*)codegen_realloc(s, s->pool, sizeof(mrb_pool_value)*irep->plen);
  irep->syms = (const mrb_sym*)codegen_realloc(s, s->syms, sizeof(mrb_sym)*irep->slen);
  irep->reps = (const mrb_irep**)codegen_realloc(s, s->reps, sizeof(mrb_irep*)*irep->rlen);
#if 0
  if (s->filename_sym) {
    mrb_sym fname = mrb_parser_get_filename(s->parser, s->filename_index);
    const char *filename = mrb_sym_name_len(s->mrb, fname, NULL);

    mrb_debug_info_append_file(s->mrb, s->irep->debug_info,
                               filename, s->lines, s->debug_start_pos, s->pc);
  }
  mrb_free(s->mrb, s->lines);
#endif

  irep->nlocals = s->nlocals;
  irep->nregs = s->nregs;

  mrb_gc_arena_restore(mrb, s->ai);
  mrb_pool_close(s->mpool);
}

static struct loopinfo*
loop_push(codegen_scope *s, enum looptype t)
{
  struct loopinfo *p = (struct loopinfo*)codegen_palloc(s, sizeof(struct loopinfo));

  p->type = t;
  p->pc0 = p->pc1 = p->pc2 = JMPLINK_START;
  p->prev = s->loop;
  p->reg = cursp();
  s->loop = p;

  return p;
}

#if 0
static void
loop_break(codegen_scope *s, node *tree)
{
  if (!s->loop) {
    codegen(s, tree, NOVAL);
    raise_error(s, "unexpected break");
  }
  else {
    struct loopinfo *loop;


    loop = s->loop;
    if (tree) {
      if (loop->reg < 0) {
        codegen(s, tree, NOVAL);
      }
      else {
        gen_retval(s, tree);
      }
    }
    while (loop) {
      if (loop->type == LOOP_BEGIN) {
        loop = loop->prev;
      }
      else if (loop->type == LOOP_RESCUE) {
        loop = loop->prev;
      }
      else{
        break;
      }
    }
    if (!loop) {
      raise_error(s, "unexpected break");
      return;
    }

    if (loop->type == LOOP_NORMAL) {
      int tmp;

      if (loop->reg >= 0) {
        if (tree) {
          gen_move(s, loop->reg, cursp(), 0);
        }
        else {
          genop_1(s, OP_LOADNIL, loop->reg);
        }
      }
      tmp = genjmp(s, OP_JMPUW, loop->pc2);
      loop->pc2 = tmp;
    }
    else {
      if (!tree) {
        genop_1(s, OP_LOADNIL, cursp());
      }
      gen_return(s, OP_BREAK, cursp());
    }
  }
}
#endif

static void
loop_pop(codegen_scope *s, int val)
{
  if (val) {
    genop_1(s, OP_LOADNIL, cursp());
  }
  dispatch_linked(s, s->loop->pc2);
  s->loop = s->loop->prev;
  if (val) push();
}

#if 0
static int
catch_handler_new(codegen_scope *s)
{
  size_t newsize = sizeof(struct mrb_irep_catch_handler) * (s->irep->clen + 1);
  s->catch_table = (struct mrb_irep_catch_handler*)codegen_realloc(s, (void*)s->catch_table, newsize);
  return s->irep->clen++;
}

static void
catch_handler_set(codegen_scope *s, int ent, enum mrb_catch_type type, uint32_t begin, uint32_t end, uint32_t target)
{
  struct mrb_irep_catch_handler *e;

  mrb_assert(ent >= 0 && ent < s->irep->clen);

  e = &s->catch_table[ent];
  uint8_to_bin(type, &e->type);
  mrb_irep_catch_handler_pack(begin, e->begin);
  mrb_irep_catch_handler_pack(end, e->end);
  mrb_irep_catch_handler_pack(target, e->target);
}
#endif

static struct RProc*
generate_code(mrb_state *mrb, mrbc_context *cxt, yp_parser_t *p, yp_node_t *node, int val)
{
  codegen_scope *scope = scope_new(mrb, cxt, 0, 0, 0);
  struct mrb_jmpbuf *prev_jmp = mrb->jmp;
  struct mrb_jmpbuf jmpbuf;
  struct RProc *proc;

  mrb->jmp = &jmpbuf;

  scope->mrb = mrb;
  scope->parser = p;
#if 0
  scope->filename_sym = p->filename_sym;
  scope->filename_index = p->current_filename_index;
#endif

  MRB_TRY(mrb->jmp) {
    /* prepare irep */
    codegen(scope, node, val);
    proc = mrb_proc_new(mrb, scope->irep);
    mrb_irep_decref(mrb, scope->irep);
    mrb_pool_close(scope->mpool);
    proc->c = NULL;
    if (mrb->c->cibase && mrb->c->cibase->proc == proc->upper) {
      proc->upper = NULL;
    }
    mrb->jmp = prev_jmp;
    return proc;
  }
  MRB_CATCH(mrb->jmp) {
    mrb_irep_decref(mrb, scope->irep);
    mrb_pool_close(scope->mpool);
    mrb->jmp = prev_jmp;
    return NULL;
  }
  MRB_END_EXC(mrb->jmp);
}

MRB_API struct RProc*
yarp_generate_code(mrb_state *mrb, mrbc_context *cxt, yp_parser_t *p, yp_node_t *node)
{
  return generate_code(mrb, cxt, p, node, VAL);
}

#if 0
void
mrb_irep_remove_lv(mrb_state *mrb, mrb_irep *irep)
{
  int i;

  if (irep->flags & MRB_IREP_NO_FREE) return;
  if (irep->lv) {
    mrb_free(mrb, (void*)irep->lv);
    irep->lv = NULL;
  }
  if (!irep->reps) return;
  for (i = 0; i < irep->rlen; i++) {
    mrb_irep_remove_lv(mrb, (mrb_irep*)irep->reps[i]);
  }
}
#endif
