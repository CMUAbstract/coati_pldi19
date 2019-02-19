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
#include <libmspmath/msp-math.h>

#ifdef CONFIG_LIBEDB_PRINTF
#include <libedb/edb.h>
#endif

#include "pins.h"

#define USING_TIMER 0
#define LENGTH 13
#define INPUT_ARR_SIZE 16
#define MAX_ITER 8
void my_print_long(uint32_t obj) {
  LOG("%x %x\r\n", (uint16_t)((obj & 0xFFFF0000) >> 16),
                      (uint16_t)(obj & 0xFFFF));
  return;
}

static __ro_nv const unsigned char VAL_ARRAY[INPUT_ARR_SIZE + 1][13] = {
  #include "../../../../data/extra_blowfish_inputs.txt"
};

static __ro_nv const char cp[32] = {'1','2','3','4','5','6','7','8','9','0',
	'A','B','C','D','E','F','F','E','D','C','B','A',
	'0','9','8','7','6','5','4','3','2','1'}; //mimicing 16byte hex key (0x1234_5678_90ab_cdef_fedc_ba09_8765_4321)

static __ro_nv const uint32_t init_key[18] = {
  #include "../../../../data/init_key.txt"
};

static __ro_nv const uint32_t init_s0[256] = {
  #include "../../../../data/init_s0.txt"
};

static __ro_nv const uint32_t init_s1[256] = {
  #include "../../../../data/init_s1.txt"
};

static __ro_nv const uint32_t init_s2[256] = {
  #include "../../../../data/init_s2.txt"
};

static __ro_nv const uint32_t init_s3[256] = {
  #include "../../../../data/init_s3.txt"
};
  // For instrumentation only
  __nv unsigned _numEvents = 0;
  __nv unsigned complete = 0;
  __nv unsigned test_ready = 0;

  TASK(task_init)
	TASK(task_set_ukey)
	TASK(task_done)
	TASK(task_init_key)
	TASK(task_init_s)
	TASK(task_set_key)
	TASK(task_set_key2)
	TASK(task_encrypt)
	TASK(task_start_encrypt)
	TASK(task_start_encrypt2)
  TASK( task_start_encrypt3)
  TASK(task_really_done)

	GLOBAL_SB(uint8_t*, return_to);	
	GLOBAL_SB(char, result, LENGTH);
	GLOBAL_SB(unsigned char, ukey, 16);
	GLOBAL_SB(uint32_t, s0, 256);
	GLOBAL_SB(uint32_t, s1, 256);
	GLOBAL_SB(uint32_t, s2, 256);
	GLOBAL_SB(uint32_t, s3, 256);
	GLOBAL_SB(unsigned, index);
	GLOBAL_SB(uint32_t, index2);
	GLOBAL_SB(unsigned, n);
	GLOBAL_SB(void*, next_task);
	//GLOBAL_SB(task_t*, next_task);
	GLOBAL_SB(uint32_t, input, 2);
	GLOBAL_SB(unsigned char, iv, 8);
	GLOBAL_SB(uint32_t, key, 18);
  GLOBAL_SB(unsigned, ready) = 0;
  GLOBAL_SB(unsigned, iter) = 0;
  GLOBAL_SB(char, procdata, LENGTH) = {"Hello, World!"};
  GLOBAL_SB(char, indata, LENGTH) = {"Hello, World!"};

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
void __attribute__((interrupt(0))) Timer0_A0_ISR(void) {
#else
void __attribute__((interrupt(0))) Port_3_ISR(void) {
  P3IFG &= ~BIT5;
#endif
  _numEvents++;
  /*P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;*/
  complete = 0;
  unsigned cur_iter = GV(iter);
  if(cur_iter > 0) {
    GV(ready) =  1;
    while(cur_iter >= INPUT_ARR_SIZE) {
      cur_iter -= INPUT_ARR_SIZE;
    }
    #if 0
    // Only for testing!!!
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    __delay_cycles(40000);
    P1OUT &= ~BIT5;
    ////////////////////
    #endif
    for(int i = 0; i < LENGTH; i++) {
      GV(indata, i) = VAL_ARRAY[cur_iter][i];
    }
    test_ready = GV(ready);
    complete = 1;
  }
}

#if USING_TIMER
__attribute__((section("__interrupt_vector_timer0_a0"),aligned(2)))
void(*__vector_timer0_a0)(void) = Timer0_A0_ISR;
#else
__attribute__((section("__interrupt_vector_port3"),aligned(2)))
void(*__vector_port3)(void) = Port_3_ISR;
#endif


#define zero 0
#define one  1
#define two  2
#define three 3
#define four 4

capybara_task_cfg_t pwr_configs[5] = {
  CFG_ROW(zero, CONFIGD, LOWP,zero),
  CFG_ROW(one, CONFIGD, LOWP,zero),
  CFG_ROW(two, CONFIGD, LOWP,zero),
  CFG_ROW(three, CONFIGD, LOWP,zero),
  CFG_ROW(four, CONFIGD, LOWP,zero),
};

#if VERBOSE > 0
	void print_long(uint32_t l) {
		LOG("%04x", (unsigned)((l>>16) & 0xffff));
		LOG("%04x\r\n",l & 0xffff);
	}
#endif
void task_init()
{ disable(); //capybara_transition(0);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
	GV(n) = 0;
	GV(index) = 0;
	GV(index2) = 0;
	unsigned i;
	for (i=0; i<8; ++i) {
		GV(iv, i) = 0;
	}
	disable();{TRANSITION_TO(task_set_ukey)};
}
void task_set_ukey() {
  enable(); 
	unsigned i = 0;
	unsigned by = 0;
	while (i < 32) {
		if(cp[i] >= '0' && cp[i] <= '9')
			by = (by << 4) + cp[i] - '0';
		else if(cp[i] >= 'A' && cp[i] <= 'F') //currently, key should be 0-9 or A-F
			by = (by << 4) + cp[i] - 'A' + 10;
		else
			PRINTF("Key must be hexadecimal!!\r\n");
		if ((i++) & 1) {
			GV(ukey, i/2-1) = by & 0xff;
			LOG("ukey[%u]: %u\r\n",i/2-1,by & 0xff);
		}

	}
	disable();{TRANSITION_TO(task_init_key)};
}
void task_init_key() { enable();
  
	unsigned i;
	for (i = 0; i < 18; ++i) {
		GV(key, i) = init_key[i];
	}
	disable();{TRANSITION_TO(task_init_s)};
}
void task_init_s() { enable();
  
	unsigned i;

	for (i = 0; i < 256; ++i) {
		if (GV(index) == 0) 
			GV(s0, i) = init_s0[i];
		else if (GV(index) == 1)
			GV(s1, i) = init_s1[i];
		else if (GV(index) == 2)
			GV(s2, i) = init_s2[i];
		else if (GV(index) == 3)
			GV(s3, i) = init_s3[i];
	}
	if(GV(index) == 3){
		disable();{TRANSITION_TO(task_set_key)};
	}
	else {
		++GV(index);
		disable();{TRANSITION_TO(task_init_s)};
	}
	/*
	   for (i = 0; i < 1024; ++i) {
	   if (i < 256) 
	   GV(s0, i) = init_s0[i];
	   else if (i < 256*2)
	   GV(s1, i-256) = init_s1[i-256];
	   else if (i < 256*3)
	   GV(s2, i-256*2) = init_s2[i-256*2];
	   else 
	   GV(s3, i-256*3) = init_s3[i-256*3];
	   }
	   disable();{TRANSITION_TO(task_set_key)};*/
}
void task_set_key() { enable();
  
	unsigned i;
	uint32_t ri, ri2;
	unsigned d = 0;
	for (i = 0; i < 18; ++i) {
		ri = GV(ukey, d++);

		d = (d >= 8)? 0 : d;

		ri <<= 8;
		ri2 = GV(ukey, d++);
		ri |= ri2;
		d = (d >= 8)? 0 : d;

		ri <<= 8;
		ri2 = GV(ukey, d++);
		ri |= ri2;
		d = (d >= 8)? 0 : d;

		ri <<= 8;
		ri2 = GV(ukey, d++);
		ri |= ri2;
		d = (d >= 8)? 0 : d;

		GV(key, i) ^= ri;
    LOG("key[i] = %x %x\r\n",(uint16_t)((GV(key,i) & 0xFF00) >> 16),
                               (uint16_t)((GV(key,i) & 0xFF)));
	}
	disable();{TRANSITION_TO(task_set_key2)};	
}
void task_set_key2() { enable();
  
  LOG("Index2 = ");
  my_print_long(GV(index2));
	if (GV(index2) == 0) {
		GV(input, 0) = 0;
		GV(input, 1) = 0;
		GV(index2) += 2;
		GV(next_task) = TASK_REF(task_set_key2);

		disable();{TRANSITION_TO(task_encrypt)};
	}
	else {
		if (GV(index2) < 20) { //set key
			GV(key, _global_index2-2) = GV(input, 0);
			GV(key, _global_index2-1) = GV(input, 1);
#if VERBOSE > 1
			printf("key[%u]=",_global_index2-2);
			my_print_long(GV(input, 0));
			printf("key[%u]=",_global_index2-1);
			my_print_long(GV(input, 1));
#endif
			GV(index2) += 2;
			disable();{TRANSITION_TO(task_encrypt)};
		}
		else { //set s
			if (GV(index2) < (256 + 20)) { //set s0 
				GV(s0, _global_index2-20) = GV(input, 0);
				GV(s0, _global_index2-19) = GV(input, 1);
#if VERBOSE > 1
				if (GV(index2) == 20 || GV(index2) == 254 + 20) {
					printf("s0[%u]=",GV(index2)-20);
					my_print_long(GV(input, 0));
					//print_long(k0);
					printf("s0[%u]=",GV(index2)-19);
					my_print_long(GV(input, 1));
					//print_long(k1);	
				}
#endif
				GV(index2) += 2;
				disable();{TRANSITION_TO(task_encrypt)};
			}
			else if (GV(index2) < (512 + 20)) { //set s1
				GV(s1, _global_index2-(256+20)) = GV(input, 0);
				GV(s1, _global_index2-(256+19)) = GV(input, 1);
#if VERBOSE > 1
				if (GV(index2) == 256 + 20 || GV(index2) == (256*2-2) + 20) {
					printf("s1[%u]=",GV(index2)-(256+20));
			printf("%x %x\r\n",((uint16_t)((GV(input, 0) & 0xFFFF0000)>> 16)),
                                ((uint16_t)(GV(input, 0) & 0xFFFF)));	
			//		print_long(GV(input, 0));
					//print_long(k0);
					printf("s1[%u]=",GV(index2)-(256+19));
			printf("%x %x\r\n",((uint16_t)((GV(input, 1) & 0xFFFF0000)>> 16)),
                                ((uint16_t)(GV(input, 1) & 0xFFFF)));	
			//		print_long(GV(input, 1));
					//print_long(k1);	
				}
#endif
				GV(index2) += 2;
				disable();{TRANSITION_TO(task_encrypt)};
			}
			else if (GV(index2) < (256*3 + 20)) { //set s2
				GV(s2, _global_index2-(256*2+20)) = GV(input, 0);
				GV(s2, _global_index2-(256*2+19)) = GV(input, 1);
#if VERBOSE > 1
				if (GV(index2) == 256*2 + 20 || GV(index2) == (256*3-2) + 20) {
					printf("s2[%u]=",GV(index2)-(256*2+20));
			printf("%x %x\r\n",((uint16_t)((GV(input, 0) & 0xFFFF0000)>> 16)),
                                ((uint16_t)(GV(input, 0) & 0xFFFF)));	
			//		print_long(GV(input, 0));
					//print_long(k0);
					printf("s2[%u]=",GV(index2)-(256*2+19));
			printf("%x %x\r\n",((uint16_t)((GV(input, 1) & 0xFFFF0000)>> 16)),
                                ((uint16_t)(GV(input, 1) & 0xFFFF)));	
			//		print_long(GV(input, 1));
					//print_long(k1);	
				}
#endif
				GV(index2) += 2;
				disable();{TRANSITION_TO(task_encrypt)};
			}
			else if (GV(index2) < (256*4 + 20)) {
				GV(s3, _global_index2-(256*3+20)) = GV(input, 0);
				GV(s3, _global_index2-(256*3+19)) = GV(input, 1);
#if VERBOSE > 1
				if (GV(index2) == 256*3 + 20 || GV(index2) == (256*4-2) + 20) {
					printf("s3[%u]=",GV(index2)-(256*3+20));
			printf("%x %x\r\n",((uint16_t)((GV(input, 0) & 0xFFFF0000)>> 16)),
                                ((uint16_t)(GV(input, 0) & 0xFFFF)));	
			//		print_long(GV(input, 0));
					//print_long(k0);
					printf("s3[%u]=",GV(index2)-(256*3+19));
			printf("%x %x\r\n",((uint16_t)((GV(input, 1) & 0xFFFF0000)>> 16)),
                                ((uint16_t)(GV(input, 1) & 0xFFFF)));	
			//		print_long(GV(input, 1));
					//print_long(k1);	
				}
#endif
				GV(index2) += 2;
				if (GV(index2) < (256*4 + 20)) {
					disable();{TRANSITION_TO(task_encrypt)};
				}
				else { //done
					GV(index2) = 0;
					disable();{TRANSITION_TO(task_start_encrypt)};	
				}
			}
		}
	}
}

void task_encrypt() { enable();
  
	uint32_t p, l, r, s0, s1, s2, s3, tmp;
	//	unsigned index = *GV(GV(index));
	//	uint8_t* return_to;
	//	struct GV_POINTER(key)* return_to;
	unsigned index;
	r = GV(input, 0);
	l = GV(input, 1);
	for (index = 0; index < 17; ++index) {
		p = GV(key, index);
#if VERBOSE > 1
      PRINTF("p = ");
			my_print_long(p);
      PRINTF("r = ");
			my_print_long(r);
#endif
		if (index == 0) {
			r ^= p;
			++index;
		}
		p = GV(key, index);
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
		s0 = GV(s0, (r>>24L));
		s1 = GV(s1, ((r>>16L)&0xff));
		s2 = GV(s2, ((r>> 8L)&0xff));
		s3 = GV(s3, ((r     )&0xff));
    l^=(((	s0 + s1)^ s2)+ s3)&0xffffffff;
		tmp = r;
		r = l;
		l = tmp;
#if VERBOSE > 1
      PRINTF("Almost final prints:\r\n");
			my_print_long(s0);
			my_print_long(s1);
			my_print_long(s2);
			my_print_long(s3);
			my_print_long(r);
			my_print_long(l);
#endif
		//while(1);
	}
	p = GV(key, 17);
	l ^= p;
	GV(input, 1) = r;
	GV(input, 0) = l;
  {
  // Subbed in from libalpaca
  context_t *next_ctx;
	next_ctx = (curctx == &context_0 ? &context_1 : &context_0);
	next_ctx->task = GV(next_task);
	next_ctx->numRollback = 0;
	curctx = next_ctx;}
	//transition_to(GV(next_task));
}

void task_start_encrypt() { enable();
  
	unsigned i; 
	//	n = *GV(GV(n));
  LOG("in start encrypt\r\n");
	if (GV(n) == 0) {
    LOG("start_encrypt:\r\n");
		GV(input, 0) =((unsigned long)(GV(iv, 0)))<<24L;
		GV(input, 0)|=((unsigned long)(GV(iv, 1)))<<16L;
		GV(input, 0)|=((unsigned long)(GV(iv, 2)))<< 8L;
		GV(input, 0)|=((unsigned long)(GV(iv, 3)));
    my_print_long(GV(input,0));
		GV(input, 1) =((unsigned long)(GV(iv, 4)))<<24L;
		GV(input, 1)|=((unsigned long)(GV(iv, 5)))<<16L;
		GV(input, 1)|=((unsigned long)(GV(iv, 6)))<< 8L;
		GV(input, 1)|=((unsigned long)(GV(iv, 7)));
    my_print_long(GV(input,1));
		GV(next_task) = TASK_REF(task_start_encrypt2);
		disable();{TRANSITION_TO(task_encrypt)};
	}
	else {
		disable();{TRANSITION_TO(task_start_encrypt3)};
	}
}
void task_start_encrypt2() { enable();
  
	GV(iv, 0) = (unsigned char)(((GV(input, 0))>>24L)&0xff);
	GV(iv, 1) = (unsigned char)(((GV(input, 0))>>16L)&0xff);
	GV(iv, 2) = (unsigned char)(((GV(input, 0))>> 8L)&0xff);
	GV(iv, 3) = (unsigned char)(((GV(input, 0))     )&0xff);
	GV(iv, 4) = (unsigned char)(((GV(input, 1))>>24L)&0xff);
	GV(iv, 5) = (unsigned char)(((GV(input, 1))>>16L)&0xff);
	GV(iv, 6) = (unsigned char)(((GV(input, 1))>> 8L)&0xff);
	GV(iv, 7) = (unsigned char)(((GV(input, 1))     )&0xff);
#if VERBOSE > 1
	for (int i=0; i<8; ++i){
		LOG("iv[%u]=%u\r\n",i,GV(iv,i));
	}	
#endif
	disable();{TRANSITION_TO(task_start_encrypt3)};
}

void task_start_encrypt3() {

	unsigned char c;
	c = GV(procdata, _global_index2)^(GV(iv, _global_n));
  //c = procdata[GV(index2)]^(GV(iv, _global_n));
	GV(result, _global_index2) = c;
	PRINTF("result: %x, %c\r\n", c, GV(procdata, _global_index2));
	GV(iv, _global_n) = c;
	GV(n) = (GV(n)+1)&0x07;
	++GV(index2);
	if (GV(index2) == LENGTH) {
    LOG("Inc'ing iter to %u!\r\n", GV(iter));
    ++GV(iter);
		disable();{TRANSITION_TO(task_done)};
	}
else {
		disable();{TRANSITION_TO(task_start_encrypt)};
	}
}

// This task doesn't work under continuous power. Alpaca privatizes "ready", so
// on return from the interrupt which will set ready = 1, the new value isn't
// visible.
void task_done()
{ enable();
  unsigned loc_iter = GV(iter);
  if(loc_iter > MAX_ITER) {
    disable();{TRANSITION_TO(task_really_done)};
  }
  else {
    if(complete && (GV(ready) != test_ready)) {
      P1OUT |= BIT5;
      P1DIR |= BIT5;
      P1OUT &= ~BIT5;
      PRINTF("Erorr! masked update in task_done! %u %u",
                          GV(ready), test_ready);
      while(1);
    }
    complete = 0;
    // Inc iteration
    if(GV(ready) == 0) {
      disable();{TRANSITION_TO(task_done)};
    }
    else {
      GV(ready) = 0;
    }
    // Clear ready flag
    GV(ready) =  0;
    // Copy over contents of indata
    for(int i = 0; i < LENGTH; i++) {
      GV(procdata, i) = GV(indata, i);
    }
    for(int i = 0; i < LENGTH; i++) {
      PRINTF("%c",GV(procdata,i));
    }
    PRINTF("\r\n");
    PRINTF("Bf run %u, events %u\r\n",loc_iter, _numEvents);
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    disable();{TRANSITION_TO(task_init)};
  }
}

void task_really_done() {
  disable();
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  PRINTF("BF done! %u events\r\n",_numEvents);
  while(1);
}

#if USING_TIMER
void init()
{
  capybara_init();
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  /*for(int i=0; i < 1000; i++) {
    __delay_cycles(4000);
  }*/
  TA0CCR0 = 4000;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
  PRINTF("bf\r\n");
  //TA0CCTL0 |= CCIE;
  //PRINTF("bf .%u.\r\n", curctx->task->idx);
}
#else
void init()
{ capybara_init();
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;

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
}


#endif

__attribute__((interrupt(0)))
void(*__vector_compe_e)(void) = COMP_VBANK_ISR;

ENTRY_TASK(task_init)
INIT_FUNC(init)
