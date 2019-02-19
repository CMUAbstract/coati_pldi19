#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <libmspware/driverlib.h>

#include <libalpaca/alpaca.h>

#include <libcapybara/capybara.h>
#include <libcapybara/power.h>
#include <libcapybara/reconfig.h>
#include <libcapybara/board.h>


#include <libmspbuiltins/builtins.h>
#include <libio/console.h>
#include <libmsp/mem.h>
#include <libmsp/periph.h>
#include <libmsp/clock.h>
#include <libmsp/sleep.h>
#include <libmsp/watchdog.h>
#include <libmsp/gpio.h>
//#include <libmspmath/msp-math.h>


#include "pins.h"

#define USING_TIMER 0
#define SEED 7L
#define ITER 256
#define MAX ITER
#define CHAR_BIT 8
#define STEP 1
#define ROUNDS 16

__nv static char bits[256] =
{
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,  /* 0   - 15  */
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  /* 16  - 31  */
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  /* 32  - 47  */
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 48  - 63  */
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  /* 64  - 79  */
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 80  - 95  */
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 96  - 111 */
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  /* 112 - 127 */
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  /* 128 - 143 */
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 144 - 159 */
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 160 - 175 */
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  /* 176 - 191 */
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  /* 192 - 207 */
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  /* 208 - 223 */
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  /* 224 - 239 */
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8   /* 240 - 255 */
};

typedef struct lock_ {
  uint8_t id;
  uint8_t state;
} lock_t;

GLOBAL_SB(lock_t, l1);

// For instrumentation only
__nv _numEvents = 0;
__nv _numEvents_missed = 0;
GLOBAL_SB(unsigned, n_0);
GLOBAL_SB(unsigned, n_1);
GLOBAL_SB(unsigned, n_2);
GLOBAL_SB(unsigned, n_3);
GLOBAL_SB(unsigned, n_4);
GLOBAL_SB(unsigned, n_5);
GLOBAL_SB(unsigned, n_6);

GLOBAL_SB(unsigned, func);

GLOBAL_SB(uint32_t, seed);
GLOBAL_SB(unsigned, iter);
GLOBAL_SB(unsigned, insert_seed);
GLOBAL_SB(unsigned, run_count) = 0;
	TASK(task_init)
	TASK(task_select_func)
	TASK(task_bit_count)
	TASK(task_bitcount)
	TASK(task_ntbl_bitcnt)
	TASK(task_ntbl_bitcount)
	TASK(task_BW_btbl_bitcount)
	TASK(task_AR_btbl_bitcount)
	TASK(task_bit_shifter)
  TASK(task_end)

#if USING_TIMER
void __attribute__((interrupt(0))) ISR_Timer0_A0(void) {
#else
void __attribute__((interrupt(0))) Port_3_ISR(void) {
  P3IFG &= ~BIT5;
#endif
  _numEvents++;
  if(_global_l1.state){
    _numEvents_missed++;
    return;
  }
  //P1OUT |= BIT5;
  //P1DIR |= BIT5;
  //P1OUT &= ~BIT5;
  uint32_t next_seed = GV(insert_seed);
  next_seed += 0xF;
  if(next_seed > 255) {
    next_seed = _numBoots & 0xF;
  }
  GV(insert_seed) = next_seed;
  GV(seed) = next_seed;
}

#if USING_TIMER
__attribute__((section("__interrupt_vector_timer0_a0"),aligned(2)))
void(*__vector_timer0_a0)(void) = ISR_Timer0_A0;
#endif

#define zero 0
#define one 1
#define two 2
#define three 3
#define four 4

capybara_task_cfg_t pwr_configs[5] = {
  CFG_ROW(zero, CONFIGD, LOWP,zero),
  CFG_ROW(one, PREBURST, LOWP, MEDHIGHP),
  CFG_ROW(two, CONFIGD, LOWP,zero),
  CFG_ROW(three, CONFIGD, LOWP,zero),
  CFG_ROW(four, CONFIGD, LOWP,zero),
};

#if USING_TIMER
void init() {
  capybara_init();
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  //TA0CCR0 = 700;
  TA0CCR0 = 500;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
  PRINTF("bc ");
  TA0CCTL0 |= CCIE;
  //PRINTF(".%u.\r\n", curctx->task->idx);
}

void disable() {
  TA0CCTL0 &= ~CCIE;
  return;
}

void enable() {
  TA0CCTL0 |= CCIE;
  return;
}
#else
void init()
{
	capybara_init();
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  /*for(int i = 0; i < 1000; i++) {
    __delay_cycles(4000);
  }*/
  P3SEL1 &= ~BIT5; // Configure for GPIO
  P3SEL0 &= ~BIT5;
  P3OUT &= ~BIT5;	// Set P3.5 as  pull down
  P3DIR &= ~BIT5; // Set P3.5 as input
  P3REN |= BIT5; // enable input pull up/down

  P3IES &= ~BIT5; // Set IFG on high-->low
  P3IFG &= ~BIT5; // Clear flag bit
  LOG("BC\r\n");
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
}

__attribute__((section("__interrupt_vector_port3"),aligned(2)))
void(*__vector_port3)(void) = Port_3_ISR;

void disable() {
  P3IE &= ~BIT5; //disable interrupt bit
  return;
}

void enable() {
  P3IE |= BIT5; //enable interrupt bit
  return;
}

#endif

void task_init() {
	disable();capybara_transition(0);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
  //overflow = 0;
	LOG("init\r\n");

	GV(func) = 0;
	GV(n_0) = 0;
	GV(n_1) = 0;
	GV(n_2) = 0;
	GV(n_3) = 0;
	GV(n_4) = 0;
	GV(n_5) = 0;
	GV(n_6) = 0;
  GV(seed) = 0;
  GV(insert_seed) = 0;
  GV(l1).id = 0;
  GV(l1.)state = 0;
	disable(); {TRANSITION_TO(task_select_func)};
}

void task_select_func() {
	disable();capybara_transition(0);enable();
	LOG("select func\r\n");
	GV(seed) = (uint32_t)SEED; // for test, seed is always the same
	//GV(iter) = 0;
	LOG("func: %u\r\n", GV(func));
	if(GV(func) == 0){
		GV(func)++;
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
    disable(); {TRANSITION_TO(task_bit_count)};
	}
	else if(GV(func) == 1){
		GV(func)++;
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
		disable(); {TRANSITION_TO(task_bitcount)};
	}
	else if(GV(func) == 2){
		GV(func)++;
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
		disable(); {TRANSITION_TO(task_ntbl_bitcnt)};
	}
	else if(GV(func) == 3){
		GV(func)++;
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
		disable(); {TRANSITION_TO(task_ntbl_bitcount)};
	}
	else if(GV(func) == 4){
		GV(func)++;
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
    disable(); {TRANSITION_TO(task_BW_btbl_bitcount)};
	}
	else if(GV(func) == 5){
		GV(func)++;
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
    disable(); {TRANSITION_TO(task_AR_btbl_bitcount)};
	}
	else if(GV(func) == 6){
		GV(func)++;
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
    disable(); {TRANSITION_TO(task_bit_shifter)};
	}
	else{
		disable(); {TRANSITION_TO(task_end)};

	}
}
void task_bit_count() {
	disable();capybara_transition(0);enable();
	LOG("bit_count\r\n");
  //set lock state
  if(GV(l1).id == 0) {
    GV(l1).state = 1;
    GV(l1).id = 1;
    /*
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    */
    //PRINTF("Set\r\n");
  }
	uint32_t tmp_seed = GV(seed);
	GV(seed) = GV(seed) + STEP;
	unsigned temp = 0;

  if(tmp_seed + STEP >= MAX) {
    GV(seed) = 0;
  }
  else {
    GV(seed) = GV(seed) + STEP;
  }

  if(tmp_seed) do
		temp++;
	while (0 != (tmp_seed = tmp_seed&(tmp_seed-1)));
	GV(n_0) += temp;
	GV(iter)++;

	if(GV(iter) < ITER){
		disable(); {TRANSITION_TO(task_bit_count)};
	}
	else{
    // Release lock
	  GV(iter) = 0;
    GV(l1).state = 0;
    GV(l1).id = 0;
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT P1OUT &= ~BIT0;= ~BIT0; */
		disable(); {TRANSITION_TO(task_select_func)};
	}
}
void task_bitcount() {
	disable();capybara_transition(0);enable();
	LOG("bitcount\r\n");
  //set lock state
  if(GV(l1).id == 0) {
    GV(l1).state = 1;
    GV(l1).id = 1;
    /*
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    */
    //PRINTF("Set\r\n");
  }
	uint32_t tmp_seed = GV(seed);
  if(tmp_seed + STEP >= MAX) {
    GV(seed) = 0;
  }
  else {
    GV(seed) = GV(seed) + STEP;
  }
	tmp_seed = ((tmp_seed & 0xAAAAAAAAL) >>  1) + (tmp_seed & 0x55555555L);
	tmp_seed = ((tmp_seed & 0xCCCCCCCCL) >>  2) + (tmp_seed & 0x33333333L);
	tmp_seed = ((tmp_seed & 0xF0F0F0F0L) >>  4) + (tmp_seed & 0x0F0F0F0FL);
	tmp_seed = ((tmp_seed & 0xFF00FF00L) >>  8) + (tmp_seed & 0x00FF00FFL);
	tmp_seed = ((tmp_seed & 0xFFFF0000L) >> 16) + (tmp_seed & 0x0000FFFFL);
	GV(n_1) += (int)tmp_seed;
	GV(iter)++;

	if(GV(iter) < ITER){
		disable(); {TRANSITION_TO(task_bitcount)};
	}
	else{
    // Release lock
	  GV(iter) = 0;
    GV(l1).state = 0;
    GV(l1).id = 0;
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT P1OUT &= ~BIT0;= ~BIT0; */
		disable(); {TRANSITION_TO(task_select_func)};
	}
}
int recursive_cnt(uint32_t x){
	int cnt = bits[(int)(x & 0x0000000FL)];

	if (0L != (x >>= 4))
		cnt += recursive_cnt(x);

	return cnt;
}
void task_ntbl_bitcnt() {
	disable();capybara_transition(0);enable();
	LOG("ntbl_bitcnt\r\n");
  //set lock state
  if(GV(l1).id == 0) {
    GV(l1).state = 1;
    GV(l1).id = 1;
    /*
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    */
    //PRINTF("Set\r\n");
  }
	GV(n_2) += recursive_cnt(GV(seed));

  if(GV(seed) + STEP >= MAX) { GV(seed) = 0;
  }
  else {
    GV(seed) = GV(seed) + STEP;
  }

  GV(iter)++;

	if(GV(iter) < ITER){
		disable(); {TRANSITION_TO(task_ntbl_bitcnt)};
	}
	else{
	  GV(iter) = 0;
    GV(l1).state = 0;
    GV(l1).id = 0;
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT P1OUT &= ~BIT0;= ~BIT0; */
		disable(); {TRANSITION_TO(task_select_func)};
	}
}
void task_ntbl_bitcount() {
	disable();capybara_transition(0);enable();
	LOG("ntbl_bitcount\r\n");
  //set lock state
  if(GV(l1).id == 0) {
    GV(l1).state = 1;
    GV(l1).id = 1;
    /*
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    */
    //PRINTF("Set\r\n");
  }
	GV(n_3) += bits[ (int) (GV(seed) & 0x0000000FUL)       ] +
		bits[ (int)((GV(seed) & 0x000000F0UL) >> 4) ] +
		bits[ (int)((GV(seed) & 0x00000F00UL) >> 8) ] +
		bits[ (int)((GV(seed) & 0x0000F000UL) >> 12)] +
		bits[ (int)((GV(seed) & 0x000F0000UL) >> 16)] +
		bits[ (int)((GV(seed) & 0x00F00000UL) >> 20)] +
		bits[ (int)((GV(seed) & 0x0F000000UL) >> 24)] +
		bits[ (int)((GV(seed) & 0xF0000000UL) >> 28)];
  if(GV(seed) + STEP >= MAX) { GV(seed) = 0;
  }
  else {
    GV(seed) = GV(seed) + STEP;
  }
	GV(iter)++;

	if(GV(iter) < ITER){
		disable(); {TRANSITION_TO(task_ntbl_bitcount)};
	}
	else{
	  GV(iter) = 0;
    GV(l1).state = 0;
    GV(l1).id = 0;
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT P1OUT &= ~BIT0;= ~BIT0; */
		disable(); {TRANSITION_TO(task_select_func)};
	}
}
void task_BW_btbl_bitcount() {
	disable();capybara_transition(0);enable();
  //set lock state
  if(GV(l1).id == 0) {
    GV(l1).state = 1;
    GV(l1).id = 1;
    /*
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    */
    //PRINTF("Set\r\n");
  }
	LOG("BW_btbl_bitcount\r\n");
	union
	{
		unsigned char ch[4];
		long y;
	} U;

	U.y = GV(seed);

	GV(n_4) += bits[ U.ch[0] ] + bits[ U.ch[1] ] +
		bits[ U.ch[3] ] + bits[ U.ch[2] ];
  if(GV(seed) + STEP >= MAX) {
    GV(seed) = 0;
  }
  else {
    GV(seed) = GV(seed) + STEP;
  }

	GV(iter)++;

	if(GV(iter) < ITER){
		disable(); {TRANSITION_TO(task_BW_btbl_bitcount)};
	}
	else{
	  GV(iter) = 0;
    GV(l1).state = 0;
    GV(l1).id = 0;
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT P1OUT &= ~BIT0;= ~BIT0; */
		disable(); {TRANSITION_TO(task_select_func)};
	}
}
void task_AR_btbl_bitcount() {
	disable();capybara_transition(0);enable();
	LOG("AR_btbl_bitcount\r\n");
  //set lock state
  if(GV(l1).id == 0) {
    GV(l1).state = 1;
    GV(l1).id = 1;
    /*
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    */
    //PRINTF("Set\r\n");
  }
	unsigned char * Ptr = (unsigned char *) &GV(seed) ;
	int Accu ;

	Accu  = bits[ *Ptr++ ];
	Accu += bits[ *Ptr++ ];
	Accu += bits[ *Ptr++ ];
	Accu += bits[ *Ptr ];
	GV(n_5)+= Accu;
  if(GV(seed) + STEP >= MAX) {
    GV(seed) = 0;
  }
  else {
    GV(seed) = GV(seed) + STEP;
  }
	GV(iter)++;

	if(GV(iter) < ITER){
		disable(); {TRANSITION_TO(task_AR_btbl_bitcount)};
	}
	else{
	  GV(iter) = 0;
    GV(l1).state = 0;
    GV(l1).id = 0;
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT P1OUT &= ~BIT0;= ~BIT0; */
		disable(); {TRANSITION_TO(task_select_func)};
	}
}
void task_bit_shifter() {
	disable();capybara_transition(0);enable();
	LOG("bit_shifter\r\n");
  //set lock state
  if(GV(l1).id == 0) {
    GV(l1).state = 1;
    GV(l1).id = 1;
    /*
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    */
  }
	int i, nn;
	uint32_t tmp_seed = GV(seed);
	//PRINTF("%u %u\r\n",tmp_seed, GV(iter));
  for (i = nn = 0; tmp_seed && (i < (sizeof(long) * CHAR_BIT)); ++i, tmp_seed >>= 1)
		nn += (int)(tmp_seed & 1L);
	GV(n_6) += nn;
  if(GV(seed) + STEP >= MAX) {
    GV(seed) = 0;
  }
  else {
    GV(seed) = GV(seed) + STEP;
  }

	GV(iter)++;

	if(GV(iter) < ITER){
    //for(int i = 0; i < 2; i++) {
    //  __delay_cycles(40000);
    //}
		disable(); {TRANSITION_TO(task_bit_shifter)};
	}
	else{
	  GV(iter) = 0;
    GV(l1).state = 0;
    GV(l1).id = 0;
    LOG("%u\r\n",GV(n_6));
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT P1OUT &= ~BIT0;= ~BIT0; */
		disable(); {TRANSITION_TO(task_select_func)};
	}
}

void task_end() {
	disable();capybara_transition(0);
  LOG("end\r\n");
  /*P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;*/
  #if 1
  GV(run_count)++;
  PRINTF("Alpaca Run %i numEvents %u\r\n",GV(run_count), _numEvents);
  #endif
	PRINTF("%u %u %u %u %u %u %u\r\n", GV(n_0),
                                    GV(n_1),
                                    GV(n_2),
                                    GV(n_3),
                                    GV(n_4),
                                    GV(n_5),
                                    GV(n_6));
  if(GV(run_count) > ROUNDS){
	  P1OUT |= BIT5;
    P1DIR |= BIT5;
	  PRINTF("%u events %u missed\r\n",_numEvents, _numEvents_missed);
    while(1);
  }
  disable(); {TRANSITION_TO(task_init)};
}

__attribute__((interrupt(0)))
void(*__vector_compe_e)(void) = COMP_VBANK_ISR;

ENTRY_TASK(task_init)
INIT_FUNC(init)
