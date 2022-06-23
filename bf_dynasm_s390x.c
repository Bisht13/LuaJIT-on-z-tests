#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../dasm_proto.h"
#include "../dasm_s390x.h"
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

|.arch s390x
|.actionlist bf_actions
|.section code
|.globals lbl_

static void *link_and_encode(dasm_State **state, size_t *size)
{
  int dasm_status = dasm_link(state, size);
  assert(dasm_status == DASM_S_OK);

  void *ret = mmap(0, *size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  dasm_encode(state, ret);
  dasm_free(state);

  mprotect(ret, *size, PROT_READ | PROT_EXEC);
  return (int *)ret;
}

#define TAPE_SIZE 30000
#define MAX_NESTING 100
#define TAPE_END 29999

typedef struct bf_state
{
  unsigned char* tape;
  unsigned char (*get_ch)(struct bf_state*);
  void (*put_ch)(struct bf_state*, unsigned char);
} bf_state_t;

#define bad_program(s) exit(fprintf(stderr, "bad program near %.16s: %s\n", program, s))

static void(* bf_compile(const char* program) )(bf_state_t*)
{
  unsigned loops[MAX_NESTING];
  int nloops = 0;
  int n;
  dasm_State* d;
  int npc = 8;
  int nextpc = 0;

  dasm_init(&d, 1);
  void* labels[lbl__MAX];
  dasm_setupglobal(&d, labels, lbl__MAX);
  dasm_setup(&d, bf_actions);
  dasm_growpc(&d, npc);
  |.define aPtr, r8
  |.define aState, r9
  |.define aTapeBegin, r10
  |.define aTapeEnd, r12
  |.define rArg1, r2
  |.define rArg2, r3
  |.macro prepcall1, arg1
    | lgr rArg1, arg1
  |.endmacro
  |.macro prepcall2, arg1, arg2
    | lgr rArg1, arg1
    | lgr rArg2, arg2
  |.endmacro
  |.define postcall, .nop
  |.macro prologue
    | stmg r8, r14, 64(r15)
    | aghi r15, -160
    | lgr aState, rArg1
  |.endmacro
  |.macro epilogue
    | aghi r15, 160
    | lmg  r8, r14, 64(r15)
    | br   r14
  |.endmacro

  |.type state, bf_state_t, aState
  
  dasm_State** Dst = &d;
  |.code
  |->bf_main:
  | prologue
  | lg  aPtr, state->tape
  | ldr aTapeBegin, aPtr
  | afi aTapeBegin, -1
  | ldr aTapeEnd, aPtr
  | afi aTapeEnd, TAPE_END
  for(;;) {
    switch(*program++) {
    case '<':
      for(n = 1; *program == '<'; ++n, ++program);
      | afi aPtr, -n%TAPE_SIZE
      | cgr  aPtr, aTapeBegin
      break;
    case '>':
      for(n = 1; *program == '>'; ++n, ++program);
      | afi aPtr, n%TAPE_SIZE
      | cgr  aPtr, aTapeEnd
      break;
    case '+':
      for(n = 1; *program == '+'; ++n, ++program);
      |  lb   r11, 0(aPtr)
      |  ahi  r11, n
      |  stc  r11, 0(aPtr)
      break;
    case '-':
      for(n = 1; *program == '-'; ++n, ++program);
      |  lb   r11, 0(aPtr)
      |  ahi  r11, -n
      |  stc  r11, 0(aPtr)
      break;
    case ',':
      | prepcall1 aState
      | lg r11, state->get_ch
      | basr  r14, r11
      | postcall 1
      | stc   r2, 0(aPtr)
      break;
    case '.':
      | llgc   r11, 0(aPtr)
      | prepcall2 aState, r11
      | lg r11, state->put_ch
      | basr r14, r11
      | postcall 2
      break;
    case '[':
      if(nloops == MAX_NESTING)
        bad_program("Nesting too deep");
      if(program[0] == '-' && program[1] == ']') {
        program += 2;
        | slgr r11, r11 
        | stc  r11, 0(aPtr)
      } else {
        if(nextpc == npc) {
          npc *= 2;
          dasm_growpc(&d, npc);
        }
        | llgc   r11, 0(aPtr)
        | cgfi r11, 0
        | je =>nextpc+1

        |=>nextpc:
        loops[nloops++] = nextpc;
        nextpc += 2;
      }
      break;
    case ']':
      if(nloops == 0)
        bad_program("] without matching [");
      --nloops;
      | llgc   r11, 0(aPtr)
      | cgfi r11, 0
      | jne =>loops[nloops]

      |=>loops[nloops]+1:
      break;
    case 0:
      if(nloops != 0)
        program = "<EOF>", bad_program("[ without matching ]");
      | epilogue
      size_t size;
      link_and_encode(&d, &size);
      return (void(*)(bf_state_t*))labels[lbl_bf_main];
    }
  }
}

static void bf_putchar(bf_state_t* s, unsigned char c)
{
  putchar((int)c);
}

static unsigned char bf_getchar(bf_state_t* s)
{
  return (unsigned char)getchar();
}

static void bf_run(const char* program)
{
  bf_state_t state;
  unsigned char tape[TAPE_SIZE] = {0};
  state.tape = tape;
  state.get_ch = bf_getchar;
  state.put_ch = bf_putchar;
  bf_compile(program)(&state);
}

int main(int argc, char** argv)
{
  if(argc == 2) {
    long sz;
    char* program;
    FILE* f = fopen(argv[1], "r");
    if(!f) {
      fprintf(stderr, "Cannot open %s\n", argv[1]);
      return 1;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    program = (char*)malloc(sz + 1);
    fseek(f, 0, SEEK_SET);
    program[fread(program, 1, sz, f)] = 0;
    fclose(f);
    bf_run(program);
    return 0;
  } else {
    fprintf(stderr, "Usage: %s INFILE.bf\n", argv[0]);
    return 1;
  }
}
