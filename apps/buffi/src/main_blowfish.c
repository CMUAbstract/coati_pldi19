// But this one actually has a transaction in it (I promise), it'll be used for
// the next round of evals
// Womp, but it doesn't fit onto an msp430fr5949 because it has too large a
// footprint to fully buffer given the other massive arrays that need to fit on
// the chip
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
#include <libmspmath/msp-math.h>

#include "pins.h"

#define USING_TIMER 0
#define LENGTH 13
#define INPUT_ARR_SIZE 16
#define MAX_ITER 8

//#if VERBOSE > 1
void my_print_long(uint32_t obj) {
  PRINTF("%x %x\r\n", (uint16_t)((obj & 0xFFFF0000) >> 16),
                      (uint16_t)(obj & 0xFFFF));
  return;
}
//#endif

__nv unsigned char VAL_ARRAY[INPUT_ARR_SIZE + 1][13] = {
  #include "../../../../data/extra_blowfish_inputs.txt"
};

__nv char cp[32] =
{'1','2','3','4','5','6','7','8','9','0',
'A','B','C','D','E','F','F','E','D','C','B','A',
'0','9','8','7','6','5','4','3','2','1'};
//mimicing 16byte hex key (0x1234_5678_90ab_cdef_fedc_ba09_8765_4321)

__nv uint32_t init_key[18] = {
  #include "../../../../data/init_key.txt"
};

__nv uint32_t init_s0[256] = {
  #include "../../../../data/init_s0.txt"
};

 __nv uint32_t init_s1[256] = {
  #include "../../../../data/init_s1.txt"
};

__nv uint32_t init_s2[256] = {
  #include "../../../../data/init_s2.txt"
};

 __nv uint32_t init_s3[256] = {
  #include "../../../../data/init_s3.txt"
};
#define zero 0
#define one  1
#define two  2
#define three 3
#define four 4

capybara_task_cfg_t pwr_configs[5] = {
  CFG_ROW(zero, CONFIGD, LOWP,zero),
  CFG_ROW(one, CONFIGD, LOWP,zero),
  CFG_ROW(two, PREBURST, LOWP,MEDP),
  CFG_ROW(three, CONFIGD, LOWP,zero),
  CFG_ROW(four, CONFIGD, LOWP,zero),
};



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


// Have to define the vector table elements manually, because clang,
// unlike gcc, does not generate sections for the vectors, it only
// generates symbols (aliases). The linker script shipped in the
// TI GCC distribution operates on sections, so we define a symbol and put it
// in its own section here named as the linker script wants it.
// The 2 bytes per alias symbol defined by clang are wasted.

TASK(1,   task_init)
TASK(2,   task_set_ukey)
TASK(3,   task_done)
TASK(4,   task_init_key)
TASK(5,   task_init_s)
TASK(6,   task_set_key)
TASK(7,   task_set_key2)
TASK(8,   task_encrypt)
TASK(8,   task_start_encrypt)
TASK(9,   task_start_encrypt2)
TASK(10,  task_start_encrypt3)
TASK(11,  task_really_done)
EVENT(12, event_timer)

// For instrumentation only
__nv unsigned _numEvents = 0;
__nv unsigned _numEvents_missed = 0;
__nv unsigned complete = 0;
__nv unsigned delayed = 0;
__nv unsigned test_ready = 0;

__nv char procdata[LENGTH] = {"Hello, World!"};
__nv char result[LENGTH];
__nv unsigned char ukey[16];
__nv uint32_t s0[256];
__nv uint32_t s1[256];
__nv uint32_t s2[256];
__nv uint32_t s3[256];
__nv unsigned index;
__nv uint32_t index2;
__nv unsigned n;
__nv task_t* next_task;
__nv uint32_t input[2];
__nv unsigned char iv[8];
__nv uint32_t key[18];
__nv unsigned temp_i;
__nv unsigned ready = 0;
__nv unsigned iter = 0;

#if VERBOSE > 1
	void print_long(uint32_t l) {
		LOG("%04x", (unsigned)((l>>16) & 0xffff));
		LOG("%04x\r\n",l & 0xffff);
	}
#endif

void task_init()
{ disable();
  capybara_transition(0);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
  WRITE(n, 0, unsigned, 0);
  WRITE(index, 0, unsigned, 0);
  WRITE(index2, 0, unsigned, 0);
	unsigned i;
	for (i=0; i<8; ++i) {
		WRITE(iv[i], 0, char, 0);
	}
	TRANSITION_TO(task_set_ukey);
}


void task_set_ukey() {
	unsigned i = 0;
	unsigned by = 0;
	while (i < 32) {
		if(cp[i] >= '0' && cp[i] <= '9')
			by = (by << 4) + cp[i] - '0';
		else if(cp[i] >= 'A' && cp[i] <= 'F') //currently, key should be 0-9 or A-F
			by = (by << 4) + cp[i] - 'A' + 10;
		else
			PRINTF("Key must be hex\r\n");
		if ((i++) & 1) {
			WRITE(ukey[i/2 - 1], by & 0xff, char, 0);
			LOG("ukey[%u]: %u\r\n",i/2-1,by & 0xff);
		}

	}
	TRANSITION_TO(task_init_key);
}

void task_init_key() {
	unsigned i;
	for (i = 0; i < 18; ++i) {
		WRITE(key[i], init_key[i], uint32_t, 0);
	}
  WRITE(temp_i, 0, unsigned, 0);
	TRANSITION_TO(task_init_s);
}

void task_init_s() {
	unsigned i;
  unsigned temp_ind = READ(index, unsigned);
  unsigned loc_temp_i = READ(temp_i, unsigned);
  //PRINTF("Before: temp_ind: %u,temp_i %u\r\n", temp_ind, temp_i);
	for (i = loc_temp_i; i < loc_temp_i + 32; ++i) {
		//printf("%u\r\n",i);
    if (temp_ind == 0) {
			WRITE(s0[i], init_s0[i], uint32_t, 0);
    }
		else if (temp_ind == 1) {
			WRITE(s1[i], init_s1[i], uint32_t, 0);
    }
		else if (temp_ind == 2) {
			WRITE(s2[i], init_s2[i], uint32_t, 0);
    }
		else if (temp_ind == 3) {
			WRITE(s3[i], init_s3[i], uint32_t, 0);
    }
	}
  //PRINTF("After: temp_ind: %u,temp_i %u\r\n", temp_ind, i);
	if(temp_ind == 3 && i == 256){
    TRANSITION_TO(task_set_key);
	}
	else {
    WRITE(temp_i, i, unsigned, 0);
    if(i == 256) {
      WRITE(index, temp_ind + 1, unsigned, 0);
      WRITE(temp_i, 0, unsigned, 0);
    }
		TRANSITION_TO(task_init_s);
	}
}

void task_set_key() {
  unsigned i;
	uint32_t ri, ri2;
	unsigned d = 0;
	for (i = 0; i < 18; ++i) {
    ri = READ(ukey[d++], char);

		d = (d >= 8)? 0 : d;

		ri <<= 8;
    ri2 = READ(ukey[d++], char);
		ri |= ri2;
		d = (d >= 8)? 0 : d;

		ri <<= 8;
    ri2 = READ(ukey[d++], char);
		ri |= ri2;
		d = (d >= 8)? 0 : d;

		ri <<= 8;
    ri2 = READ(ukey[d++], char);
		ri |= ri2;
		d = (d >= 8)? 0 : d;
    WRITE(key[i], READ(key[i], uint32_t) ^ ri, uint32_t, 0);
    LOG("key[i] = %x%x\r\n",(uint16_t)((READ(key[i],uint32_t) & 0xFF00) >> 16),
                               (uint16_t)((READ(key[i],uint32_t) & 0xFF)));
	}
	TRANSITION_TO(task_set_key2);
}

void task_set_key2() {
  TX_BEGIN;
  PRINTF("task_set_key2\r\n");
  LOG("Index2 = ");
#if VERBOSE > 1
  my_print_long(TX_READ(index2, uint32_t));
#endif
	if (TX_READ(index2, uint32_t) == 0) {
		TX_WRITE(input[0], 0, uint32_t, 0);
		TX_WRITE(input[1], 0, uint32_t, 0);
		TX_WRITE(index2, TX_READ(index2, uint32_t) + 2, uint32_t, 0);
	  TX_WRITE(next_task, (uint16_t)TASK_REF(task_set_key2), uint16_t, 0);

		TX_TRANSITION_TO(task_encrypt);
	}
	else {
    uint32_t temp = TX_READ(index2, uint32_t);
		if (temp < 20) { //set key
		  TX_WRITE(key[temp - 2], TX_READ(input[0], uint32_t), uint32_t, 0);	
		  TX_WRITE(key[temp - 1], TX_READ(input[1], uint32_t), uint32_t, 0);	
#if VERBOSE > 1
			printf("key[%u]=",temp-2);
			my_print_long(TX_READ(input[0], uint32_t));
			printf("key[%u]=",temp-1);
			my_print_long(TX_READ(input[1], uint32_t));	
#endif
      TX_WRITE(index2, temp + 2, uint32_t, 0);
			TX_TRANSITION_TO(task_encrypt);
		}
		else { //set s
			if (temp < (256 + 20)) { //set s0 
				TX_WRITE(s0[temp-20], TX_READ(input[0], uint32_t), uint32_t, 0);
				TX_WRITE(s0[temp-19], TX_READ(input[1], uint32_t), uint32_t, 0);
#if VERBOSE > 1
				if (temp == 20 || temp == 254 + 20) {
					printf("s0[%u]=",temp-20);
					my_print_long(TX_READ(input[0], uint32_t));
					//print_long(k0);
					printf("s0[%u]=",temp-19);
					my_print_long(TX_READ(input[1], uint32_t));
					//print_long(k1);	
				}
#endif
				TX_WRITE(index2, temp + 2, uint32_t, 0);
				TX_TRANSITION_TO(task_encrypt);
			}
			else if (temp < (512 + 20)) { //set s1
				TX_WRITE(s1[temp-(256+20)], TX_READ(input[0], uint32_t), uint32_t, 0);
				TX_WRITE(s1[temp-(256+19)], TX_READ(input[1], uint32_t), uint32_t, 0);
#if VERBOSE > 1
				if (temp == 256 + 20 || temp == (256*2-2) + 20) {
					printf("s1[%u]=",temp-(256+20));
					my_print_long(TX_READ(input[0], uint32_t));
					//print_long(k0);
					printf("s1[%u]=",temp-(256+19));
					my_print_long(TX_READ(input[1], uint32_t));
					//print_long(k1);
				}
#endif
				TX_WRITE(index2, temp + 2, uint32_t, 0);
				TX_TRANSITION_TO(task_encrypt);
			}
			else if (temp < (256*3 + 20)) { //set s2
				TX_WRITE(s2[temp-(256*2+20)], TX_READ(input[0], uint32_t), uint32_t, 0);
				TX_WRITE(s2[temp-(256*2+19)], TX_READ(input[1], uint32_t), uint32_t, 0);
#if VERBOSE > 1
				if (temp == 256*2 + 20 || temp == (256*3-2) + 20) {
					printf("s2[%u]=",temp-(256*2+20));
					my_print_long(TX_READ(input[0], uint32_t));
					//print_long(k0);
					printf("s2[%u]=",temp-(256*2+19));
					my_print_long(TX_READ(input[1], uint32_t));
					//print_long(k1);
				}
#endif
				TX_WRITE(index2, temp + 2, uint32_t, 0);
				TX_TRANSITION_TO(task_encrypt);
			}
			else if (temp < (256*4 + 20)) {
				TX_WRITE(s3[temp-(256*3+20)], TX_READ(input[0], uint32_t), uint32_t, 0);
				TX_WRITE(s3[temp-(256*3+19)], TX_READ(input[1], uint32_t), uint32_t, 0);
#if VERBOSE > 1
				if (temp == 256*3 + 20 || temp == (256*4-2) + 20) {
					printf("s3[%u]=",temp-(256*3+20));
					my_print_long(TX_READ(input[0], uint32_t));
					//print_long(k0);
					printf("s3[%u]=",temp-(256*3+19));
					my_print_long(TX_READ(input[1], uint32_t));
					//print_long(k1);
				}
#endif
				TX_WRITE(index2, temp + 2, uint32_t, 0);
				if (temp + 2 < (256*4 + 20)) {
					TX_TRANSITION_TO(task_encrypt);
				}
				else { //done
					TX_WRITE(index2, 0, uint32_t, 0);
					TX_TRANSITION_TO(task_start_encrypt);
				}
			}
		}
	}
}

void task_encrypt() {
  PRINTF("task_encrypt\r\n");
	uint32_t p, l, r, locs0, locs1, locs2, locs3, tmp;
	unsigned locindex;
	r = TX_READ(input[0], uint32_t);
	l = TX_READ(input[1], uint32_t);
	for (locindex = 0; locindex < 17; ++locindex) {
		p = TX_READ(key[locindex], uint32_t);
#if VERBOSE > 1
      PRINTF("p = ");
			my_print_long(p);
      PRINTF("r = ");
			my_print_long(r);
#endif
		if (locindex == 0) {
			r ^= p;
			++locindex;
		}
		p = TX_READ(key[locindex], uint32_t);
#if VERBOSE > 1
			PRINTF("p = ");
			my_print_long(p);
			PRINTF("r = ");
			my_print_long(r);
			PRINTF("l = ");
			my_print_long(l);
#endif
		l^=p;
#if VERBOSE > 1
			PRINTF("p = ");
      my_print_long(p);
			PRINTF("l = ");
			my_print_long(l);
#endif
    /*PRINTF("index & S1: ");
    my_print_long((r>>16L)&0xff);
    my_print_long(GV(s1, ((r>>16L)&0xff)));
    */
		locs0 = TX_READ(s0[(r>>24L)], uint32_t);
		locs1 = TX_READ(s1[((r>>16L)&0xff)], uint32_t);
		locs2 = TX_READ(s2[((r>> 8L)&0xff)], uint32_t);
		locs3 = TX_READ(s3[((r     )&0xff)], uint32_t);
		l^=(((locs0 + locs1)^ locs2)+ locs3)&0xffffffff;
		tmp = r;
		r = l;
		l = tmp;
#if VERBOSE > 1
      PRINTF("Almost final prints:\r\n");
			my_print_long(locs0);
			my_print_long(locs1);
			my_print_long(locs2);
			my_print_long(locs3);
			my_print_long(r);
			my_print_long(l);
#endif
		//while(1);
	}
	p = TX_READ(key[17], uint32_t);
	l ^= p;
  TX_WRITE(input[1], r, uint32_t, 0);
  TX_WRITE(input[0], l, uint32_t, 0);
  // TODO for measurments, need to add some SET_TSK_IN_TX_TRANS primitives
  curctx->commit_state = TSK_IN_TX_PH1;
	transition_to((task_t *)TX_READ(next_task, uint16_t));
}

void task_start_encrypt() {
  PRINTF("task_start_encrypt\r\n");
	unsigned i;
	//	n = *TX_READ(GV(n));
  LOG("in start encrypt\r\n");
	if (TX_READ(n, unsigned) == 0) {
    LOG("start_encrypt:\r\n");
    uint32_t temp;
		temp =((unsigned long)(TX_READ(iv[0], char)))<<24L;
		temp |=((unsigned long)(TX_READ(iv[1], char)))<<16L;
		temp |=((unsigned long)(TX_READ(iv[2], char)))<< 8L;
	  temp |=((unsigned long)(TX_READ(iv[3], char)));
    TX_WRITE(input[0], temp, uint32_t, 0);
#if VERBOSE > 1
    my_print_long(TX_READ(input[0],uint32_t));
#endif
		temp =((unsigned long)(TX_READ(iv[4], char)))<<24L;
		temp |=((unsigned long)(TX_READ(iv[5], char)))<<16L;
		temp |=((unsigned long)(TX_READ(iv[6], char)))<< 8L;
	  temp |=((unsigned long)(TX_READ(iv[7], char)));
    TX_WRITE(input[1], temp, uint32_t, 0);
#if VERBOSE > 1
    my_print_long(TX_READ(input[1],uint32_t));
#endif
		TX_WRITE(next_task, (uint16_t)TASK_REF(task_start_encrypt2), uint16_t, 0);
		TX_TRANSITION_TO(task_encrypt);
	}
	else {
		TX_TRANSITION_TO(task_start_encrypt3);
	}
}

void task_start_encrypt2() {
  PRINTF("task_start_encrypt2\r\n");
	uint32_t temp = TX_READ(input[0], uint32_t);
  TX_WRITE(iv[0], (unsigned char)(temp>>24L)&0xff, char, 0);
  TX_WRITE(iv[1], (unsigned char)(temp>>16L)&0xff, char, 0);
  TX_WRITE(iv[2], (unsigned char)(temp>> 8L)&0xff, char, 0);
  TX_WRITE(iv[3], (unsigned char)(temp     )&0xff, char, 0);
  temp = TX_READ(input[1], uint32_t);
  TX_WRITE(iv[4], (unsigned char)(temp>>24L)&0xff, char, 0);
  TX_WRITE(iv[5], (unsigned char)(temp>>16L)&0xff, char, 0);
  TX_WRITE(iv[6], (unsigned char)(temp>> 8L)&0xff, char, 0);
  TX_WRITE(iv[7], (unsigned char)(temp     )&0xff, char, 0);
#if VERBOSE > 1
	for (int i=0; i<8; ++i){
		LOG("iv[%u]=%u\r\n",i,TX_READ(iv[i], char));
	}
#endif
	TX_TRANSITION_TO(task_start_encrypt3);
}

void task_start_encrypt3() {
  PRINTF("task_start_encrypt3\r\n");
	unsigned char c;
  unsigned temp_n = TX_READ(n, unsigned);
  uint32_t temp_index2 = TX_READ(index2, uint32_t);
	c = (TX_READ(procdata[temp_index2],char))^(TX_READ(iv[temp_n],char));
	TX_WRITE(result[temp_index2], c, uint32_t,0);
	PRINTF("result: %x, %c\r\n", c, TX_READ(procdata[temp_index2],char));
  TX_WRITE(iv[temp_n], c, char, 0);
	TX_WRITE(n, (temp_n + 1)&0x07, unsigned, 0)
  TX_WRITE(index2, temp_index2 + 1, uint32_t, 0);
	if (TX_READ(index2, uint32_t) == LENGTH) {
    TX_WRITE(iter, TX_READ(iter, unsigned) + 1, unsigned, 0);
		TX_END_TRANSITION_TO(task_done);
	}
	else {
		TX_TRANSITION_TO(task_start_encrypt);
	}
}


void task_done() {
  PRINTF("task_done\r\n");
  unsigned loc_iter = READ(iter,unsigned);
  if(loc_iter > MAX_ITER) {
    TRANSITION_TO(task_really_done);
  }
  else {
    //PRINTF("%u\r\n", base_config.banks);
    if(complete && delayed && (READ(ready, unsigned) != test_ready)) {
      P1OUT |= BIT5;
      P1DIR |= BIT5;
      P1OUT &= ~BIT5;
      PRINTF("Erorr! masked update in task_done! %u %u",
                          READ(ready, unsigned), ready);
      while(1);
    }
    complete = 0;
    delayed = 0;
     while(!READ(ready,unsigned)) {
      WAIT_TIMER_START;
      delayed = 1;
      __delay_cycles(4000);
      // TODO Add sleep task
      NI_TRANSITION_TO(task_done);
    }
    WAIT_TIMER_STOP;
    // Clear ready flag
    WRITE(ready, 0, unsigned, 0);
    for(int i = 0; i < LENGTH; i++) {
      PRINTF("%c",READ(procdata[i], char));
    }
    PRINTF("\r\n");
    PRINTF("Bf run %u, events %u\r\n",loc_iter, _numEvents);
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    TRANSITION_TO(task_init);
  }
}

// Nothing left to do but go through it's pockets and look for loose change
void task_really_done()
{ disable();
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  PRINTF("BF done! %u events %u missed\r\n",_numEvents, _numEvents_missed);
  //TRANSITION_TO(task_init);
  APP_FINISHED;
  while(1);
}

#if USING_TIMER
void init()
{
  capybara_init();
  //__delay_cycles(4000000);
  TA0CCR0 = 500;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
#ifdef CONFIG_EDB
	edb_init();
#endif
	PRINTF("bf\r\n");
  //PRINTF(".%u.\r\n", curctx->task->idx);
}

void __attribute__((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR(void) {
  disable();
  TOP_HALF_START();
  P3OUT |= BIT5;
  P3DIR |= BIT5;
  P3OUT &= ~BIT5;
  TOP_HALF_RETURN(TASK_REF(event_timer));
  enable();
}
#else
void init()
{ capybara_init();
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

void __attribute__((interrupt(PORT3_VECTOR))) Port_3_ISR(void) {
  P3IFG &= ~BIT5;
  EV_TIMER_START

  SET_EV_TRANS  TRANS_TIMER_START
  event_handler(CONTEXT_REF(event_timer));
}
#endif

void event_timer() {
  EV_TIMER_START;
  _numEvents++;
  EV_WRITE(complete, 0, unsigned, 0);

  unsigned cur_iter = EV_READ(iter, unsigned);
  if(cur_iter > 0) {
    EV_WRITE(ready, 1, unsigned, 0);
    while(cur_iter >= INPUT_ARR_SIZE) {
      cur_iter -= INPUT_ARR_SIZE;
    }
    for(int i = 0; i < LENGTH; i++) {
      EV_WRITE(procdata[i], VAL_ARRAY[cur_iter][i], char, 0);
    }
    test_ready = EV_READ(ready, unsigned);
    EV_WRITE(complete, 1, unsigned, 0);
    //PRINTF("\r\n");
  }
  EVENT_RETURN();
}

INIT_FUNC(init)
ENTRY_TASK(task_init)
EVENT_ENABLE_FUNC(enable)
EVENT_DISABLE_FUNC(disable)
