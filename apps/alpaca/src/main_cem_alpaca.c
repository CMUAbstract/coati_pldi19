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

#define zero 0
#define one  1
#define two  2
#define three 3
#define four 4

capybara_task_cfg_t pwr_configs[5] = {
  CFG_ROW(zero, CONFIGD, LOWP,zero),
  CFG_ROW(one, PREBURST, LOWP,HIGHP),
  CFG_ROW(two, CONFIGD, LOWP,zero),
  CFG_ROW(three, CONFIGD, LOWP,zero),
  CFG_ROW(four, CONFIGD, LOWP,zero),
};


#define USING_TIMER 0
#ifdef CONFIG_LIBEDB_PRINTF
#include <libedb/edb.h>
#endif

#include "pins.h"

#define NIL 0 // like NULL, but for indexes, not real pointers

#define DICT_SIZE         512
#define BLOCK_SIZE         16

#define NUM_LETTERS_IN_SAMPLE        2
#define LETTER_MASK             0x00FF
#define LETTER_SIZE_BITS             8
#define NUM_LETTERS (LETTER_MASK + 1)
#define MAX_RUNS 8

typedef unsigned index_t;
typedef unsigned letter_t;
typedef unsigned sample_t;

typedef struct _node_t {
	letter_t letter; // 'letter' of the alphabet
	index_t sibling; // this node is a member of the parent's children list
	index_t child;   // link-list of children
} node_t;

TASK(task_init)
TASK(task_init_dict)
TASK(task_sample)
TASK(task_letterize)
TASK(task_compress)
TASK(task_find_sibling)
TASK(task_add_node)
TASK(task_add_insert)
TASK( task_print)
TASK( task_done)
TASK( task_bump)
TASK( task_pre_sample)

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
// For instrumentation only
__nv unsigned  _numEvents = 0;
__nv unsigned complete = 0;
__nv unsigned test_ns = 0;

GLOBAL_SB(letter_t, letter);
GLOBAL_SB(unsigned, letter_idx);
GLOBAL_SB(sample_t, prev_sample);
GLOBAL_SB(index_t, out_len);
GLOBAL_SB(index_t, node_count);
GLOBAL_SB(node_t, dict, DICT_SIZE);
GLOBAL_SB(sample_t, sample);
GLOBAL_SB(index_t, sample_count);
GLOBAL_SB(index_t, sibling);
GLOBAL_SB(index_t, child);
GLOBAL_SB(index_t, parent);
GLOBAL_SB(index_t, parent_next);
GLOBAL_SB(node_t, parent_node);
GLOBAL_SB(node_t, compressed_data, BLOCK_SIZE);
GLOBAL_SB(node_t, sibling_node);
GLOBAL_SB(index_t, symbol);
GLOBAL_SB(sample_t, sample_in);
GLOBAL_SB(unsigned, run_count) = 0;
GLOBAL_SB(unsigned, iter);
GLOBAL_SB(unsigned, last_iter);
GLOBAL_SB(unsigned, ready);
GLOBAL_SB(unsigned, new_samp);

#if USING_TIMER
void init()
{
	// only for timers
	capybara_init();
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  /*for(int i = 0; i < 100; i++) {
    __delay_cycles(40000);
  }*/
  TA0CCR0 = 500;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
  TA0CCTL0 |= CCIE;
  LOG("cem\r\n");
	// only for timers
	PRINTF(".%u.\r\n", curctx->task->idx);
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

static sample_t acquire_sample_ISR(letter_t prev_sample)
{
	letter_t sample = (prev_sample + 1) & 0x03;
  //LOG("measure: %u\r\n",sample);
	return sample;
}

void task_init()
{ disable();
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
  //capybara_transition(0);
	LOG("init\r\n");
	GV(parent_next) = 0;
  GV(parent) = 0;
  GV(child) = 0;
  GV(sibling) = 0;
	LOG("init: start parent %u\r\n", GV(parent));
	GV(out_len) = 0;
	GV(letter) = 0;
  GV(sample) = 0;
	GV(prev_sample) = 0;
	GV(letter_idx) = 0;;
	GV(sample_count) = 1;
  GV(iter) = 0;
  GV(last_iter) = 0xFF;
  GV(new_samp) = 0;
  GV(ready) = 0;
  GV(node_count) = 0;
	disable();{TRANSITION_TO(task_init_dict)};
}

void task_init_dict()
{ enable();
	LOG("init dict: letter %u\r\n", GV(letter));
  unsigned start = GV(node_count);
  unsigned i;

  for(i = start; i < start + (NUM_LETTERS >> 2); i++) {
    LOG("%i\r\n",i);
    GV(dict, i).letter = i;
    GV(dict, i).sibling = NIL;// no siblings for 'root' nodes
    GV(dict, i).child = NIL;// init an empty list for children
  }
	GV(node_count) = i;
  if(i < NUM_LETTERS) {
    disable();{TRANSITION_TO(task_init_dict)};
  }
  GV(letter) = NUM_LETTERS;
	disable();{TRANSITION_TO(task_sample)};
}

void task_bump() {
  enable();
  GV(iter) = GV(iter) + 1;
  PRINTF("Bump: iter %u\r\n",GV(iter));
  if(GV(new_samp)) {
    GV(ready) = 1;
    GV(new_samp) = 0;
    GV(sample) = GV(sample_in);
    complete = 0;
  }
  else {
    GV(ready) = 0;
  }
  disable();{TRANSITION_TO(task_sample)};
}

void task_sample()
{ enable();
	//PRINTF("sample: letter idx %u\r\n", GV(letter_idx));

	unsigned next_letter_idx = GV(letter_idx) + 1;
	if (next_letter_idx == NUM_LETTERS_IN_SAMPLE){
		next_letter_idx = 0;
  }
  if(complete && (GV(new_samp) != test_ns)) {
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    PRINTF("Erorr! masked update tyring to get sample! %u %u\r\n",
                                      GV(new_samp), test_ns);
    while(1);
  }
  complete = 0;

	if (GV(letter_idx) == 0) {
    // Pull data into buffer
    if(GV(new_samp)) {
      GV(sample) = GV(sample_in);
    }
    if(GV(ready) || GV(new_samp)) {
      GV(letter_idx) = next_letter_idx;
      GV(ready) = 0;
      GV(new_samp) = 0;
      disable();{TRANSITION_TO(task_letterize)};
    }
    // wait for new data if we don't have it. (I think)
    // Actually, for now let's not wait.
    else {
      //PRINTF("Waiting!\r\n");
      __delay_cycles(40000);
      disable();{TRANSITION_TO(task_sample)};
      disable();{TRANSITION_TO(task_sample)};
    }
	} else {
		GV(letter_idx) =  next_letter_idx;
		disable();{TRANSITION_TO(task_letterize)};
	}

}


void task_letterize()
{ enable();
	unsigned letter_idx = GV(letter_idx);
	if (letter_idx == 0)
		letter_idx = NUM_LETTERS_IN_SAMPLE;
	else
		letter_idx--;
	unsigned letter_shift = LETTER_SIZE_BITS * letter_idx;
	letter_t letter = (GV(sample) & (LETTER_MASK << letter_shift)) >> letter_shift;


	GV(letter) = letter;
	LOG("letterize: sample %x letter %x (%u)\r\n", GV(sample), GV(letter),
                                                 GV(letter));
	disable();{TRANSITION_TO(task_compress)};
}

void task_compress()
{ enable();
	node_t parent_node;

	// pointer into the dictionary tree; starts at a root's child
	index_t parent = GV(parent_next);

	LOG("compress: parent %u\r\n", parent);

	parent_node = GV(dict, parent);

	LOG("compress: parent node: l %u s %u c %u\r\n", parent_node.letter, parent_node.sibling, parent_node.child);

	GV(sibling) = parent_node.child;
	GV(parent_node) = parent_node;
	GV(parent) = parent;
	GV(child) = parent_node.child;
	GV(sample_count)++;

	disable();{TRANSITION_TO(task_find_sibling)};
}

void task_find_sibling()
{ enable();
	node_t *sibling_node;

	LOG("find sibling: l %u s %u\r\n", GV(letter), GV(sibling));

	if (GV(sibling) != NIL) {
		int i = GV(sibling);
    LOG("Checking %u\r\n",i);
		sibling_node = &GV(dict,i); 

		LOG("find sibling: l %u, sn: l %u s %u c %u\r\n", GV(letter),
				sibling_node->letter, sibling_node->sibling, sibling_node->child);

		if (sibling_node->letter == GV(letter)) { // found
			LOG("find sibling: found %u\r\n", GV(sibling));

			GV(parent_next) = GV(sibling);

			disable();{TRANSITION_TO(task_letterize)};
		} else { // continue traversing the siblings
			if(sibling_node->sibling != 0){
				GV(sibling) = sibling_node->sibling;
				disable();{TRANSITION_TO(task_find_sibling)};
			}
		}

	} 
	LOG("find sibling: not found\r\n");

	index_t starting_node_idx = (index_t)GV(letter);
	GV(parent_next) = starting_node_idx;

	LOG("find sibling: child %u\r\n", GV(child));


	if (GV(child) == NIL) {
		disable();{TRANSITION_TO(task_add_insert)};
	} else {
		disable();{TRANSITION_TO(task_add_node)}; 
	}
}

void task_add_node()
{enable();
	node_t *sibling_node;

	int i = GV(sibling);
	sibling_node = &GV(dict, i);

	LOG("add node: s %u, sn: l %u s %u c %u\r\n", GV(sibling),
			sibling_node->letter, sibling_node->sibling, sibling_node->child);

	if (sibling_node->sibling != NIL) {
		index_t next_sibling = sibling_node->sibling;
		GV(sibling) = next_sibling;
		disable();{TRANSITION_TO(task_add_node)};

	} else { // found last sibling in the list

		LOG("add node: found last\r\n");

		node_t sibling_node_obj = *sibling_node;
		GV(sibling_node) = sibling_node_obj;

		disable();{TRANSITION_TO(task_add_insert)};
	}
}

void task_add_insert()
{enable();
	LOG("add insert: nodes %u\r\n", GV(node_count));
  // TODO is this the end condition?
	if (GV(node_count) == DICT_SIZE) { // wipe the table if full
		while (1);
	}
	LOG("add insert: l %u p %u, pn l %u s %u c%u\r\n", GV(letter), GV(parent),
			GV(parent_node).letter, GV(parent_node).sibling, GV(parent_node).child);

	index_t child = GV(node_count);
  LOG("new child: %u\r\n",child);
	node_t child_node = {
		.letter = GV(letter),
		.sibling = NIL,
		.child = NIL,
	};
	int i = GV(parent);
	index_t last_sibling = GV(sibling);

	if (GV(parent_node).child == NIL) { // the only child
		LOG("add insert: only child\r\n");

		node_t parent_node_obj = GV(parent_node);
		parent_node_obj.child = child;
    // TODO PRINT OUT HERE WHAT THIS NODE LOOKS LIKE
		GV(dict, i) = parent_node_obj;

	} else { // a sibling

		node_t last_sibling_node = GV(sibling_node);                   

		LOG("add insert: sibling %u\r\n", last_sibling);

		last_sibling_node.sibling = child;
		GV(dict, last_sibling) = last_sibling_node;
	}
	GV(dict, child) = child_node;
	GV(symbol) = GV(parent);
	GV(node_count)++;
  LOG("add insert: parent l %u s %u c %u\r\n",GV(dict,i).letter,
                                            GV(dict,i).sibling,
                                            GV(dict,i).child);
  LOG("add insert: child l %u s %u c %u\r\n",GV(dict,child).letter,
                                            GV(dict,child).sibling,
                                            GV(dict,child).child);
  LOG("add insert: sibling l %u s %u c %u\r\n",GV(dict,last_sibling).letter,
                                            GV(dict,last_sibling).sibling,
                                            GV(dict,last_sibling).child);

	i = GV(out_len);
	GV(compressed_data, i).letter = GV(symbol);
	LOG("append comp:let %u sym %u len %u \r\n",GV(compressed_data,i).letter, 
                                                  GV(symbol), GV(out_len));

	if (++GV(out_len) == BLOCK_SIZE) {
		disable();{TRANSITION_TO(task_print)};
	} else {
		disable();{TRANSITION_TO(task_pre_sample)};
	}
}

void task_print()
{enable();
	unsigned i;

	BLOCK_PRINTF_BEGIN();
	BLOCK_PRINTF("run %u\r\n",GV(run_count));
	for (i = 0; i < BLOCK_SIZE; ++i) {
		index_t index = GV(compressed_data, i).letter;
		BLOCK_PRINTF("%04x ", index);
		if (i > 0 && (i + 1) % 8 == 0){
			BLOCK_PRINTF("\r\n");
		}
	}
	BLOCK_PRINTF("\r\n");
	BLOCK_PRINTF("rate: samples/block: %u/%u\r\n", GV(sample_count), BLOCK_SIZE);
	BLOCK_PRINTF_END();
  GV(run_count)++;
  if(GV(run_count) > MAX_RUNS) {
    disable();{TRANSITION_TO(task_done)};
  }
  else {
    GV(iter) = 0;
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    disable();{TRANSITION_TO(task_init)};
  }
}

void task_pre_sample() {
  disable();capybara_transition(0);enable();
	unsigned next_letter_idx = GV(letter_idx) + 1;
	if (next_letter_idx == NUM_LETTERS_IN_SAMPLE) {
    LOG("sample: zeroing idx\r\n");
		next_letter_idx = 0;
  }
  if(GV(letter_idx) == 0) {
    {TRANSITION_TO(task_bump)};
  }
  else {
    {TRANSITION_TO(task_sample)};
  }
}

void task_done()
{ disable();
  PRINTF("Alpaca finish: %u events %u missed\r\n", _numEvents,
                                                0);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
	while(1);
  //	{TRANSITION_TO(task_init)};
}
#if USING_TIMER
void __attribute__((interrupt(0))) Timer0_A0_ISR(void) {
#else
void __attribute__((interrupt(0))) Port_3_ISR(void) {
  P3IFG &= ~BIT5;
#endif

  _numEvents++;
  complete = 0;
  //LOG("\r\nevent %u\r\n", GV(new_samp));
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;

  // Read in new data
  if(GV(iter) != GV(last_iter)) {
    GV(new_samp) = 1;
    GV(last_iter) = GV(iter);
    sample_t locsample = acquire_sample_ISR(GV(prev_sample));
    GV(prev_sample) =  locsample;
    GV(sample_in) =  locsample;
    /*P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;*/
  }
  test_ns = GV(new_samp);
  complete = 1;
}

#if USING_TIMER
__attribute__((section("__interrupt_vector_timer0_a0"),aligned(2)))
void(*__vector_timer0_a0)(void) = Timer0_A0_ISR;
#else
__attribute__((section("__interrupt_vector_port3"),aligned(2)))
void(*__vector_port3)(void) = Port_3_ISR;
#endif

__attribute__((interrupt(0)))
void(*__vector_compe_e)(void) = COMP_VBANK_ISR;


ENTRY_TASK(task_init)
INIT_FUNC(init)
