#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <libmspware/driverlib.h>

#include <libcoatigcc/coati.h>
#include <libcoatigcc/tx.h>
#include <libcoatigcc/filter.h>
#include <libcoatigcc/event.h>
#include <libcoatigcc/top_half.h>

#include <libcapybara/capybara.h>
#include <libcapybara/power.h>
#include <libcapybara/reconfig.h>
#include <libcapybara/board.h>


#include <libmspbuiltins/builtins.h>
#include <libio/console.h>
#include <libmsp/mem.h>
#include <libmsp/periph.h>
#include <libmsp/clock.h>
#include <libmsp/watchdog.h>
#include <libmsp/gpio.h>

#ifdef CONFIG_LIBEDB_PRINTF
#include <libedb/edb.h>
#endif


#include "pins.h"

#define USING_TIMER 0
#define SEED 7L
//#define ITER 512
#define ITER 256
#define MAX ITER
#define CHAR_BIT 8
#define WINDOW_LEN 15
#define NUM_BYTES 256
#define STEP 1
#define CHECK_NUM 2
#define ROUNDS 16

static void delay(uint32_t cycles)
{
    unsigned i;
    for (i = 0; i < cycles / (1U << 15); ++i)
        __delay_cycles(1U << 15);
}

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

// For testing only!
__nv unsigned  _numEvents = 0;
__nv unsigned  _numEvents_missed = 0;


// Top half variable stuff
TH_VAR(loc_x, unsigned);

TH_ARRAY(loc_ys, unsigned, 5);

__nv unsigned n_0;
__nv unsigned n_1;
__nv unsigned n_2;
__nv unsigned n_3;
__nv unsigned n_4;
__nv unsigned n_5;
__nv unsigned n_6;

__nv unsigned func;
__nv uint32_t seed;
__nv uint32_t insert_seed;
__nv unsigned iter;
__nv unsigned chck_cnt;
__nv unsigned n_temp;

__nv volatile unsigned run_count = 0;

TASK(1, task_init)
TASK(2, task_select_func)
TASK(3, task_bit_count)
TASK(4, task_bitcount)
TASK(5, task_ntbl_bitcnt)
TASK(6, task_ntbl_bitcount)
TASK(7, task_BW_btbl_bitcount)
TASK(8, task_AR_btbl_bitcount)
TASK(9, task_bit_shifter)
TASK(10, task_end)

DEFERRED_EVENT(11, event_timer)


#if USING_TIMER
void disable() {
  TA0CCTL0 &= ~CCIE;
  return;
}

void enable() {
  TA0CCTL0 |= CCIE;
  return;
}
#else
void disable() {
  P3IE &= ~BIT5; //disable interrupt bit
  return;
}

void enable() {
  P3IE |= BIT5; //enable interrupt bit
  return;
}
#endif

#if USING_TIMER
void __attribute__((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR(void) {
#else
void __attribute__((interrupt(PORT3_VECTOR))) Port_3_ISR(void) {
  P3IFG &= ~BIT5;
#endif
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  disable();
  EV_TIMER_START
  uint8_t list_status = 0;
  list_status = TOP_HALF_CHECK_START();
  if(list_status) {
    _numEvents_missed++;
    return;
  }
  EV_TIMER_STOP
  
 SET_EV_TRANS  TRANS_TIMER_START
  TOP_HALF_RETURN(TASK_REF(event_timer));
  // TODO: I suspect this enable is a bad idea... we really haven't escaped the
  // ISR here...
  enable();
}


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
  insert_seed = 0;
  //TA0CCR0 = 900;
  TA0CCR0 = 500;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
  PRINTF("bc ");
}
#else
void init() {
  capybara_init();
  P3SEL1 &= ~BIT5; // Configure for GPIO
  P3SEL0 &= ~BIT5;
  P3OUT &= ~BIT5;	// Set P3.5 as  pull down
  P3DIR &= ~BIT5; // Set P3.5 as input
  P3REN |= BIT5; // enable input pull up/down

  P3IES &= ~BIT5; // Set IFG on high-->low
  P3IFG &= ~BIT5; // Clear flag bit
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  TIMER_INIT;
}
#endif

void task_init() {
  disable();
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
	capybara_transition(0);
  LOG("task init %x\r\n", curctx->task->func);
  WRITE(func, 0, unsigned, 0);
  WRITE(n_0, 0, unsigned, 0);
  WRITE(n_1, 0, unsigned, 0);
  WRITE(n_2, 0, unsigned, 0);
  WRITE(n_3, 0, unsigned, 0);
  WRITE(n_4, 0, unsigned, 0);
  WRITE(n_5, 0, unsigned, 0);
  WRITE(n_6, 0, unsigned, 0);
  WRITE(insert_seed, 0, unsigned, 0);
	TRANSITION_TO(task_select_func);
}

void task_select_func() {
	disable();capybara_transition(0);enable();
	LOG("select func\r\n");

	WRITE(seed,(uint32_t)SEED,unsigned,0);
  WRITE(iter, 0, unsigned,0);
	LOG("func: %u\r\n", READ(func,unsigned));
	if(READ(func,unsigned) == 0){
		WRITE(func, READ(func,unsigned) + 1, unsigned, 0);
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
    #endif
		TRANSITION_TO(task_bit_count);
	}
	else if(READ(func,unsigned) == 1){
		WRITE(func,READ(func,unsigned) + 1,unsigned,0);
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
    TRANSITION_TO(task_bitcount);
	}
	else if(READ(func,unsigned) == 2){
		WRITE(func,READ(func,unsigned) + 1,unsigned,0);
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
    TRANSITION_TO(task_ntbl_bitcnt);
	}
	else if(READ(func,unsigned) == 3){
		WRITE(func,READ(func,unsigned) + 1,unsigned,0);
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
    TRANSITION_TO(task_ntbl_bitcount);
	}
	else if(READ(func,unsigned) == 4){
		WRITE(func,READ(func,unsigned) + 1,unsigned,0);
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
    TRANSITION_TO(task_BW_btbl_bitcount);
	}
	else if(READ(func,unsigned) == 5){
		WRITE(func,READ(func,unsigned) + 1,unsigned,0);
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
    TRANSITION_TO(task_AR_btbl_bitcount);
	}
	else if(READ(func,unsigned) == 6){
		WRITE(func,READ(func,unsigned) + 1,unsigned,0);
    #if USING_TIMER
    P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
		#endif
		TRANSITION_TO(task_bit_shifter);
	}
	else{
		TRANSITION_TO(task_end);

	}
}

void task_bit_count() {
	TX_BEGIN
  disable();capybara_transition(0);enable();
  LOG("bit_count, %u, %u, %x\r\n",TX_READ(iter,unsigned),
                              TX_READ(chck_cnt,unsigned),
                              curctx->task->func);
	uint32_t tmp_seed = TX_READ(seed,uint32_t);
  if(tmp_seed + STEP >= MAX) {
    TX_WRITE(seed, 0, uint32_t, 0);
  }
  else {
    TX_WRITE(seed, tmp_seed + STEP, uint32_t, 0);
  }
  //TX_WRITE(seed, TX_READ(seed,uint32_t) + STEP, uint32_t, 0);
  //LOG("Seed = %x\r\n",TX_READ(seed,uint32_t));
	unsigned temp = 0;
	LOG("looop start\r\n");
	if(tmp_seed) do{
		temp++;
    LOG("tmp_seed = %ld\r\n",tmp_seed);
  } while (0 != (tmp_seed = tmp_seed&(tmp_seed-1)));
	LOG("loop end\r\n");
  TX_WRITE(n_0, TX_READ(n_0, unsigned) + temp, unsigned, 0);
	TX_WRITE(iter, TX_READ(iter,unsigned) + 1, unsigned, 0);
  LOG("iter = %u\r\n",TX_READ(iter,unsigned));
	if(TX_READ(iter,unsigned) < ITER){
		TX_TRANSITION_TO(task_bit_count);
	}
  else {
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;*/

		TX_END_TRANSITION_TO(task_select_func);
  }
}

void task_bitcount() {
	TX_BEGIN
	disable();capybara_transition(0);enable();
  LOG("bitcount\r\n");
	uint32_t tmp_seed = TX_READ(seed,uint32_t);
  if(tmp_seed + STEP >= MAX) {
    TX_WRITE(seed, 0, uint32_t, 0);
  }
  else {
    TX_WRITE(seed, tmp_seed + STEP, uint32_t, 0);
  }
	tmp_seed = ((tmp_seed & 0xAAAAAAAAL) >>  1) + (tmp_seed & 0x55555555L);
	tmp_seed = ((tmp_seed & 0xCCCCCCCCL) >>  2) + (tmp_seed & 0x33333333L);
	tmp_seed = ((tmp_seed & 0xF0F0F0F0L) >>  4) + (tmp_seed & 0x0F0F0F0FL);
	tmp_seed = ((tmp_seed & 0xFF00FF00L) >>  8) + (tmp_seed & 0x00FF00FFL);
	tmp_seed = ((tmp_seed & 0xFFFF0000L) >> 16) + (tmp_seed & 0x0000FFFFL);
	TX_WRITE(n_1, TX_READ(n_1,unsigned) + (int)tmp_seed, unsigned, 0);
	TX_WRITE(iter, TX_READ(iter,unsigned) + 1, unsigned,0)

	if(TX_READ(iter,unsigned) < ITER){
		TX_TRANSITION_TO(task_bitcount);
	}
	else{
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;*/
		TX_END_TRANSITION_TO(task_select_func);
	}
}


int recursive_cnt(uint32_t x){
	int cnt = bits[(int)(x & 0x0000000FL)];

	if (0L != (x >>= 4))
		cnt += recursive_cnt(x);

	return cnt;
}
void task_ntbl_bitcnt() {
	TX_BEGIN
	disable();capybara_transition(0);enable();
  LOG("ntbl_bitcnt\r\n");
	int temp = recursive_cnt(TX_READ(seed,uint32_t));
  TX_WRITE(n_2, TX_READ(n_2, unsigned) + temp, unsigned, 0);
  if(TX_READ(seed,uint32_t) + STEP >= MAX) {
    TX_WRITE(seed, 0, uint32_t, 0);
  }
  else {
    TX_WRITE(seed,TX_READ(seed,uint32_t) + STEP, uint32_t, 0);
  }

  TX_WRITE(iter,TX_READ(iter,unsigned) + 1, unsigned, 0);

	if(TX_READ(iter,unsigned) < ITER){
		TX_TRANSITION_TO(task_ntbl_bitcnt);
	}
	else{
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;*/
		TX_END_TRANSITION_TO(task_select_func);
	}
}

void task_ntbl_bitcount() {
	TX_BEGIN
	disable();capybara_transition(0);enable();
  LOG("ntbl_bitcount\r\n");
  unsigned temp = bits[ (int) (TX_READ(seed,uint32_t) & 0x0000000FUL)       ] +
		bits[ (int)((TX_READ(seed,uint32_t) & 0x000000F0UL) >> 4) ] +
		bits[ (int)((TX_READ(seed,uint32_t) & 0x00000F00UL) >> 8) ] +
		bits[ (int)((TX_READ(seed,uint32_t) & 0x0000F000UL) >> 12)] +
		bits[ (int)((TX_READ(seed,uint32_t) & 0x000F0000UL) >> 16)] +
		bits[ (int)((TX_READ(seed,uint32_t) & 0x00F00000UL) >> 20)] +
		bits[ (int)((TX_READ(seed,uint32_t) & 0x0F000000UL) >> 24)] +
		bits[ (int)((TX_READ(seed,uint32_t) & 0xF0000000UL) >> 28)];
	TX_WRITE(n_3, TX_READ(n_3, unsigned) + temp, unsigned, 0);
  if(TX_READ(seed,uint32_t) + STEP >= MAX) {
    TX_WRITE(seed, 0, uint32_t, 0);
  }
  else {
    TX_WRITE(seed, TX_READ(seed,uint32_t) + STEP, uint32_t, 0);
  }

  TX_WRITE(iter, TX_READ(iter,unsigned) + 1, unsigned, 0);

	if(TX_READ(iter,unsigned) < ITER){
		TX_TRANSITION_TO(task_ntbl_bitcount);
	}
	else{
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;*/
		TX_END_TRANSITION_TO(task_select_func);
	}
}

void task_BW_btbl_bitcount() {
	TX_BEGIN
	disable();capybara_transition(0);enable();
  LOG("BW_btbl_bitcount\r\n");
	union
	{
		unsigned char ch[4];
		long y;
	} U;

	U.y = TX_READ(seed,uint32_t);
  // Huh? There's nothing in U.ch at this point? You'll just get garbage!
  unsigned temp;
  temp = bits[ U.ch[0] ] + bits[ U.ch[1] ] +
		bits[ U.ch[3] ] + bits[ U.ch[2] ];
  TX_WRITE(n_4, TX_READ(n_4,unsigned) + temp, unsigned, 0);
  if(TX_READ(seed, uint32_t) + STEP >= MAX) {
    TX_WRITE(seed, 0, uint32_t, 0);
  }
  else {
    TX_WRITE(seed, TX_READ(seed, uint32_t) + STEP, uint32_t, 0);
  }
	
  TX_WRITE(iter,TX_READ(iter,unsigned) + 1,unsigned,0);

	if(TX_READ(iter,unsigned) < ITER){
		TX_TRANSITION_TO(task_BW_btbl_bitcount);
	}
	else{
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;*/
		TX_END_TRANSITION_TO(task_select_func);
	}
}

// This is some serious bit bashing... so the task takes seed (a 32 bit number)
// and hands it off to an 8 bit pointer.
void task_AR_btbl_bitcount() {
	TX_BEGIN
	disable();capybara_transition(0);enable();
  LOG("AR_btbl_bitcount\r\n");
	unsigned char * Ptr = (unsigned char *) &TX_READ(seed,uint32_t) ;
	int Accu ;
  // What does this do? This increments a memory address and then dereferences
  // it??
	Accu  = bits[ *Ptr++ ];
	Accu += bits[ *Ptr++ ];
	Accu += bits[ *Ptr++ ];
	Accu += bits[ *Ptr ];
  TX_WRITE(n_5, TX_READ(n_5,unsigned) + Accu, unsigned, 0);
  if(TX_READ(seed,uint32_t) + STEP >= MAX) {
    TX_WRITE(seed, 0, uint32_t, 0);
  }
  else {
    TX_WRITE(seed, TX_READ(seed,uint32_t) + STEP, uint32_t, 0);
  }

  TX_WRITE(iter,TX_READ(iter,unsigned) + 1, unsigned, 0);

	if(TX_READ(iter,unsigned) < ITER){
		TX_TRANSITION_TO(task_AR_btbl_bitcount);
	}
	else{
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;*/
		TX_END_TRANSITION_TO(task_select_func);
	}
}

void task_bit_shifter() {
	TX_BEGIN
	disable();capybara_transition(0);enable();
  LOG("bit_shifter\r\n");
	int i, nn;
	uint32_t tmp_seed = TX_READ(seed,uint32_t);
  if(tmp_seed + STEP >= MAX) {
    TX_WRITE(seed, 0, uint32_t, 0);
  }
  else {
    TX_WRITE(seed, tmp_seed + STEP, uint32_t, 0);
  }
	for (i = nn = 0; tmp_seed && (i < (sizeof(long) * CHAR_BIT)); ++i, tmp_seed >>= 1)
		nn += (int)(tmp_seed & 1L);
  TX_WRITE(n_6, TX_READ(n_6,unsigned) + nn, unsigned, 0);
  TX_WRITE(iter, TX_READ(iter,unsigned) + 1, unsigned, 0);

	if(TX_READ(iter,unsigned) < ITER){
		TX_TRANSITION_TO(task_bit_shifter);
	}
	else{
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;*/
		TX_END_TRANSITION_TO(task_select_func);
	}
}

void task_end() {
  disable();
	capybara_transition(0);
  //P1OUT |= BIT4;
  //P1DIR |= BIT4;
  //P1OUT &= ~BIT4;
  WRITE(run_count, READ(run_count, unsigned) + 1, unsigned, 0);
  PRINTF("Easy Run %i numEvents %u\r\n",READ(run_count, unsigned), _numEvents);
	PRINTF("%u %u %u %u %u %u %u\r\n", READ(n_0, unsigned),
                                    READ(n_1, unsigned),
                                    READ(n_2, unsigned),
                                    READ(n_3, unsigned),
                                    READ(n_4, unsigned),
                                    READ(n_5, unsigned),
                                    READ(n_6, unsigned));
  if(READ(run_count,unsigned) > ROUNDS){
	  P1OUT |= BIT5;
    P1DIR |= BIT5;
	  PRINTF("%u events %u missed\r\n",_numEvents, _numEvents_missed);
    APP_FINISHED;
    while(1);
  }
  TRANSITION_TO(task_init);
}

void event_timer() {
  EV_TIMER_START;
  _numEvents++;
  uint32_t start = EV_READ(insert_seed,uint32_t);
  start += 0xF;
  if(start > 255) {
    start = _numBoots & 0xF;
  }
  EV_WRITE(insert_seed, start, uint32_t,0);
  LOG("timer, seed = %x !\r\n",start);
  EV_WRITE(seed, start, uint32_t, 0);
  /*P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;*/
  EVENT_RETURN();
}

ENTRY_TASK(task_init)
INIT_FUNC(init)
EVENT_DISABLE_FUNC(disable)
EVENT_ENABLE_FUNC(enable)
