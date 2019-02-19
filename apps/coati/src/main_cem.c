// This app is a cold-chain equipment monitoring program that reads from a
// sensor and stores the the data in a compressed format. The interrupt supplies
// data the sensor so that it doesn't have to poll. This app uses a transaction
// to separate the interrupt driven event and the code that does the actual
// processing.


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

#define NIL 0 // like NULL, but for indexes, not real pointers
#define USING_TIMER 0

#define DICT_SIZE         512
#define BLOCK_SIZE         16

#define NUM_LETTERS_IN_SAMPLE        2
#define LETTER_MASK             0x00FF
#define LETTER_SIZE_BITS             8
#define NUM_LETTERS (LETTER_MASK + 1)
#define MAX_RUNS 8

#define zero 0
#define one  1
#define two  2
#define three 3
#define four 4

capybara_task_cfg_t pwr_configs[5] = {
  CFG_ROW(zero, CONFIGD, LOWP,zero),
  CFG_ROW(one, PREBURST, LOWP, HIGHP),
  CFG_ROW(two, CONFIGD, LOWP,zero),
  CFG_ROW(three, CONFIGD, LOWP,zero),
  CFG_ROW(four, CONFIGD, LOWP,zero),
};


typedef unsigned index_t;
typedef unsigned letter_t;
typedef unsigned sample_t;

typedef struct _node_t {
	letter_t letter; // 'letter' of the alphabet
	index_t sibling; // this node is a member of the parent's children list
	index_t child;   // link-list of children
} node_t;

TASK(1, task_init)
TASK(2, task_init_dict)
TASK(3, task_sample)
TASK(5, task_letterize)
TASK(7, task_find_sibling)
TASK(8, task_add_node)
TASK(9, task_add_insert)
TASK(11, task_print)
TASK(12, task_done)
DEFERRED_EVENT(13, event_timer)
TASK(14, task_pre_sample)
TASK(15, task_bump)

// For instrumentation only
__nv unsigned _numEvents = 0;
__nv unsigned _numEvents_missed = 0;
__nv unsigned complete = 0;
__nv unsigned delayed = 0;
__nv unsigned test_ns = 0;

#if USING_TIMER
void disable() {
  TA0CCTL0 &= ~CCIE;
  return;
}

void enable() {
  TA0CCTL0 |= CCIE;
  return;
}

void __attribute__((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR(void) {
  disable();
  TOP_HALF_START();
  TOP_HALF_RETURN(TASK_REF(event_timer));
  enable();
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
  TIMER_INIT;
}

void disable() {
  P3IE &= ~BIT5; //disable interrupt bit
  return;
}


void enable() {
  P3IE |= BIT5; //enable interrupt bit
  return;
}

void __attribute__((interrupt(PORT3_VECTOR))) Port_3_ISR(void) {
  P3IFG &= ~BIT5;
  disable();
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  EV_TIMER_START
  uint8_t list_status = 0;
  list_status = TOP_HALF_CHECK_START();
  if(list_status) {
    _numEvents_missed++;
    return;
  }
  EV_TIMER_STOP;
  
 SET_EV_TRANS  TRANS_TIMER_START;
  TOP_HALF_RETURN(TASK_REF(event_timer));
  enable();
}


#endif

// We're cheating here so we can just loop on task_init without adding another
// task
__nv unsigned run_count = 0;
__nv unsigned iter;
__nv unsigned new_samp;
__nv unsigned ready;
__nv unsigned last_iter;
__nv letter_t letter;
__nv unsigned letter_idx;
__nv sample_t prev_sample;
__nv index_t out_len;
__nv index_t node_count;
__nv node_t dict[DICT_SIZE];
__nv sample_t sample;
__nv index_t sample_count;
__nv index_t sibling;
__nv index_t child;
__nv index_t parent;
__nv index_t parent_next;
__nv node_t parent_node;
__nv node_t compressed_data[BLOCK_SIZE];
__nv node_t sibling_node;
__nv index_t symbol;

#if USING_TIMER
void init()
{ 
	capybara_init();
  //__delay_cycles(4000000);
  TA0CCR0 = 500;
  //TA0CCR0 = 337;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
  PRINTF("cem\r\n");
	// only for timers
}
#endif

static sample_t acquire_sample(letter_t locprev_sample)
{
	letter_t locsample = (locprev_sample + 1) & 0x03;
  LOG("measure: %u\r\n",locsample);
	return locsample;
}

void task_init()
{ disable();
  capybara_transition(0);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
	LOG("init\r\n");
	WRITE(parent_next, 0, unsigned, 0);
  WRITE(parent, 0, unsigned, 0);
  WRITE(child, 0, unsigned, 0);
  WRITE(sibling, 0, unsigned, 0);
  // TODO: this var hasn't been initialized has it?
	LOG("init: start parent %u\r\n", READ(parent,unsigned));
	WRITE(out_len, 0, unsigned, 0);
	WRITE(letter, 0, unsigned, 0);
  WRITE(sample, 0, unsigned, 0);
	WRITE(prev_sample, 0, unsigned, 0);
	WRITE(letter_idx, 0, unsigned, 0);
	WRITE(sample_count, 1, unsigned, 0);
  WRITE(iter, 0, unsigned, 0);
  WRITE(last_iter, 0xFF, unsigned, 0);
  WRITE(new_samp, 0, unsigned, 0);
  WRITE(ready, 0, unsigned, 0);
  WRITE(node_count, 0, unsigned, 0);
	TRANSITION_TO(task_init_dict);
}

void task_init_dict()
{
	LOG("init dict: letter %u out of %u\r\n", READ(letter,unsigned), NUM_LETTERS);
  unsigned start = READ(node_count, unsigned);
  unsigned i;
  for( i = start; i < start + (NUM_LETTERS >> 2); i++) {
    LOG2("%i\r\n",i);
    WRITE(dict[i].letter, i, unsigned, 0);
    WRITE(dict[i].sibling, 0, unsigned, 0);// no siblings for 'root' nodes
    WRITE(dict[i].child, 0, unsigned,0);// init an empty list for children
  }
  WRITE(node_count, i, unsigned, 0);
  if(i < NUM_LETTERS) {
    TRANSITION_TO(task_init_dict);
  }
  WRITE(letter, NUM_LETTERS, unsigned, 0);
	TRANSITION_TO(task_sample);
}

// Still within tx. Basically decides if we're going to start a new tx or not.
void task_pre_sample() {
	unsigned next_letter_idx = TX_READ(letter_idx, unsigned) + 1;
	if (next_letter_idx == NUM_LETTERS_IN_SAMPLE) {
    LOG("sample: zeroing idx\r\n");
		next_letter_idx = 0;
  }
  if(TX_READ(letter_idx, unsigned) == 0) {
    TX_END_TRANSITION_TO(task_bump);
  }
  else {
    TX_TRANSITION_TO(task_sample);
  }
}

void task_bump() {
  WRITE(iter, READ(iter, unsigned) + 1, unsigned, 0);
  PRINTF("Bump: iter %u\r\n",READ(iter,unsigned));
  if(READ(new_samp, unsigned)) {
    WRITE(ready, 1, unsigned, 0);
    WRITE(new_samp, 0, unsigned, 0);
    complete = 0;
  }
  else {
    WRITE(ready, 0, unsigned, 0);
  }
  TRANSITION_TO(task_sample);
}

void task_sample()
{ TX_BEGIN
  LOG("sample: letter idx %u\r\n", TX_READ(letter_idx, unsigned));
	unsigned next_letter_idx = TX_READ(letter_idx, unsigned) + 1;
	if (next_letter_idx == NUM_LETTERS_IN_SAMPLE) {
    LOG("sample: zeroing idx\r\n");
    LOG("Start\r\n");
		next_letter_idx = 0;
  }
  
  if(complete && delayed &&
    (TX_READ(new_samp, unsigned) != test_ns)) {
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    PRINTF("Erorr! masked update in task_sample! %u %u",
                        TX_READ(new_samp, unsigned), test_ns);
    while(1);
  }
  complete = 0;
  delayed = 0;
	
  if (TX_READ(letter_idx, unsigned) == 0) {
    if(TX_READ(ready, unsigned) || TX_READ(new_samp, unsigned)) {
      WAIT_TIMER_STOP;
      TX_WRITE(letter_idx, next_letter_idx, unsigned, 0);
      TX_WRITE(ready,0, unsigned, 0);
      TX_WRITE(new_samp, 0, unsigned, 0);
      TX_TRANSITION_TO(task_letterize);
    }
    // wait for new data if we don't have it. (I think)
    // Actually, for now let's not wait.
    else {
      WAIT_TIMER_START;
      __delay_cycles(40000);
      NI_TX_END_TRANSITION_TO(task_sample);
    }
	} else {
		TX_WRITE(letter_idx, next_letter_idx, unsigned, 0);
		TX_TRANSITION_TO(task_letterize);
	}
}

void task_letterize()
{ TX_BEGIN
	unsigned locletter_idx = TX_READ(letter_idx, unsigned);
	if (locletter_idx == 0){
		locletter_idx = NUM_LETTERS_IN_SAMPLE;
  }
	else{
		locletter_idx--;
  }
	unsigned letter_shift = LETTER_SIZE_BITS * locletter_idx;
	letter_t locletter;
  locletter = (TX_READ(sample,unsigned) & (LETTER_MASK << letter_shift)) >> letter_shift;

  TX_WRITE(letter, locletter, unsigned, 0);
	//PRINTF("letterize: sample %x letter %x (%u) on iter %u\r\n", 
	LOG("letterize: sample %x letter %x (%u)\r\n", 
                                                TX_READ(sample, unsigned),
                                                TX_READ(locletter, unsigned),
                                                TX_READ(locletter, unsigned)
                                                );
                                                //,TX_READ(iter, unsigned));
	node_t locparent_node;

	// pointer into the dictionary tree; starts at a root's child
	index_t locparent = TX_READ(parent_next, unsigned);

	LOG("compress: parent %u\r\n", locparent);
  // TODO get rid of multiple copies!!
	locparent_node.letter = TX_READ(dict[locparent].letter, unsigned);
	locparent_node.sibling = TX_READ(dict[locparent].sibling, unsigned);
	locparent_node.child = TX_READ(dict[locparent].child, unsigned);

	LOG("compress: parent node: l %u s %u c %u\r\n", locparent_node.letter,
                                      locparent_node.sibling, locparent_node.child);
  TX_WRITE(sibling, locparent_node.child, unsigned, 0);
  // Broken out b/c coati...
  TX_WRITE(parent_node.letter, locparent_node.letter, unsigned, 0);
  TX_WRITE(parent_node.sibling, locparent_node.sibling, unsigned, 0);
  TX_WRITE(parent_node.child, locparent_node.child, unsigned, 0);
  TX_WRITE(parent, locparent, unsigned, 0);
  TX_WRITE(child, locparent_node.child, unsigned, 0);
  TX_WRITE(sample_count, TX_READ(sample_count, unsigned) + 1, unsigned, 0);

	TX_TRANSITION_TO(task_find_sibling);
}


// TODO rewrite as a single task instead of a loop of tiny tasks
void task_find_sibling()
{
	node_t locsibling_node;

	LOG("find sibling: l %u s %u\r\n", TX_READ(letter, unsigned),
                                     TX_READ(sibling,unsigned));

	if (TX_READ(sibling, unsigned) != 0) {
		int i = TX_READ(sibling, unsigned);
    LOG2("Checking %u\r\n",i);
    locsibling_node.letter = (TX_READ(dict[i].letter, unsigned));
    locsibling_node.sibling = (TX_READ(dict[i].sibling, unsigned));
    locsibling_node.child = (TX_READ(dict[i].child, unsigned));

		LOG("find sibling: l %u, sn: l %u s %u c %u\r\n", TX_READ(letter, unsigned),
				locsibling_node.letter, locsibling_node.sibling, locsibling_node.child);

		if (locsibling_node.letter == TX_READ(letter, unsigned)) { // found
			LOG("find sibling: found %u\r\n", TX_READ(sibling, unsigned));

			TX_WRITE(parent_next,TX_READ(sibling,unsigned),unsigned,0);

			TX_TRANSITION_TO(task_letterize);
		} else { // continue traversing the siblings
			if(locsibling_node.sibling != 0){
				TX_WRITE(sibling, locsibling_node.sibling, unsigned, 0);
				TX_TRANSITION_TO(task_find_sibling);
			}
		}
	}
	LOG("find sibling: not found\r\n");

	index_t starting_node_idx = (index_t)TX_READ(letter, unsigned);
  TX_WRITE(parent_next, starting_node_idx, unsigned, 0);

	LOG("find sibling: child %u\r\n", TX_READ(child, unsigned));


	if (TX_READ(child, unsigned) == 0) {
		TX_TRANSITION_TO(task_add_insert);
	} else {
		TX_TRANSITION_TO(task_add_node);
	}
}

void task_add_node()
{
	node_t locsibling_node;

	int i = TX_READ(sibling, unsigned);
  locsibling_node.letter = (TX_READ(dict[i].letter, unsigned));
  locsibling_node.sibling = (TX_READ(dict[i].sibling, unsigned));
  locsibling_node.child = (TX_READ(dict[i].child, unsigned));


	LOG("add node: s %u, sn: l %u s %u c %u\r\n", TX_READ(sibling, unsigned),
			locsibling_node.letter, locsibling_node.sibling, locsibling_node.child);

	if (sibling_node.sibling != 0) {
		index_t next_sibling = locsibling_node.sibling;
		TX_WRITE(sibling, next_sibling, unsigned, 0);
		TX_TRANSITION_TO(task_add_node);

	} else { // found last sibling in the list

		LOG("add node: found last\r\n");
    TX_WRITE(sibling_node.letter, locsibling_node.letter, unsigned, 0);
    TX_WRITE(sibling_node.sibling, locsibling_node.sibling, unsigned, 0);
    TX_WRITE(sibling_node.child, locsibling_node.child, unsigned, 0);

		TX_TRANSITION_TO(task_add_insert);
	}
}

void task_add_insert()
{
	LOG("add insert: nodes %u\r\n", TX_READ(node_count, unsigned));
  // TODO is this the end condition?
	if (TX_READ(node_count, unsigned) == DICT_SIZE) { // wipe the table if full
		while (1);
	}
	LOG("add insert: l %u p %u, pn l %u s %u c%u\r\n", TX_READ(letter, unsigned),
  TX_READ(parent, unsigned), TX_READ(parent_node.letter, unsigned),
  TX_READ(parent_node.sibling,unsigned), TX_READ(parent_node.child, unsigned));

	index_t locchild = TX_READ(node_count, unsigned);
  LOG("new child: %u\r\n",locchild);
	node_t locchild_node = {
		.letter = TX_READ(letter, unsigned),
		.sibling = NIL,
		.child = NIL,
	};
	
  int i = TX_READ(parent, unsigned);
	index_t last_sibling = TX_READ(sibling, unsigned);

	if (TX_READ(parent_node.child,unsigned) == 0) { // the only child
		LOG("add insert: only child\r\n");
    // TODO remove multiple copies!
		node_t parent_node_obj;
    parent_node_obj.letter= TX_READ(parent_node.letter, unsigned);
    parent_node_obj.sibling= TX_READ(parent_node.sibling, unsigned);
		parent_node_obj.child = locchild;
		TX_WRITE(dict[i].letter, parent_node_obj.letter, unsigned, 0);
		TX_WRITE(dict[i].sibling, parent_node_obj.sibling, unsigned, 0);
		TX_WRITE(dict[i].child, parent_node_obj.child, unsigned, 0);

	} else { // a sibling

		node_t last_sibling_node;
    last_sibling_node.letter = TX_READ(sibling_node.letter, unsigned);
    last_sibling_node.sibling = TX_READ(sibling_node.sibling, unsigned);
    last_sibling_node.child = TX_READ(sibling_node.child, unsigned);

		LOG("add insert: sibling %u\r\n", last_sibling);

		last_sibling_node.sibling = locchild;
		TX_WRITE(dict[last_sibling].letter, last_sibling_node.letter, unsigned, 0);
		TX_WRITE(dict[last_sibling].sibling, last_sibling_node.sibling, unsigned, 0);
		TX_WRITE(dict[last_sibling].child, last_sibling_node.child, unsigned, 0);
	}
  TX_WRITE(dict[locchild].letter, locchild_node.letter, unsigned, 0);
  TX_WRITE(dict[locchild].sibling, locchild_node.sibling, unsigned, 0);
  TX_WRITE(dict[locchild].child, locchild_node.child, unsigned, 0);
	TX_WRITE(symbol, TX_READ(parent, unsigned), unsigned, 0);
	TX_WRITE(node_count, TX_READ(node_count, unsigned) + 1, unsigned, 0);
  LOG("add insert: parent l %u s %u c %u\r\n",
                                            TX_READ(dict[i].letter,unsigned),
                                            TX_READ(dict[i].sibling,unsigned),
                                            TX_READ(dict[i].child,unsigned));
  LOG("add insert: child l %u s %u c %u\r\n",
                                            TX_READ(dict[locchild].letter,unsigned),
                                            TX_READ(dict[locchild].sibling,unsigned),
                                            TX_READ(dict[locchild].child,unsigned));
  LOG("add insert: sibling l %u s %u c %u\r\n",
                                            TX_READ(dict[last_sibling].letter,unsigned),
                                            TX_READ(dict[last_sibling].sibling,unsigned),
                                            TX_READ(dict[last_sibling].child,unsigned));

	//int i = TX_READ(out_len, unsigned);
	i = TX_READ(out_len, unsigned);
	TX_WRITE(compressed_data[i].letter,TX_READ(symbol, unsigned), unsigned, 0);
	LOG("append comp:let %04x sym %u len %u\r\n", 
                      TX_READ(compressed_data[i].letter, unsigned),
                                         TX_READ(symbol, unsigned),
                                         TX_READ(out_len,unsigned));
  TX_WRITE(out_len, TX_READ(out_len, unsigned) + 1, unsigned, 0);
	if (TX_READ(out_len, unsigned)== BLOCK_SIZE) {
		TX_END_TRANSITION_TO(task_print);
	} else {
    // Check if we need to end the transaction first
    TX_TRANSITION_TO(task_pre_sample);
	}
}

void task_print()
{ disable();
  capybara_transition(0);
	unsigned i;

	BLOCK_PRINTF_BEGIN();
	BLOCK_PRINTF("run %u, events %u\r\n",READ(run_count, unsigned), _numEvents);
	for (i = 0; i < BLOCK_SIZE; ++i) {
		// Print letter
    index_t locindex = READ(compressed_data[i].letter, unsigned);
		BLOCK_PRINTF("%04x ", locindex);
		// Add new line
    if (i > 0 && (i + 1) % 8 == 0){
			BLOCK_PRINTF("\r\n");
		}
	}
	BLOCK_PRINTF("\r\n");
	BLOCK_PRINTF("rate: samples/block: %u/%u\r\n", READ(sample_count, unsigned),
                                                 BLOCK_SIZE);
	BLOCK_PRINTF_END();
  WRITE(run_count, READ(run_count, unsigned) + 1, unsigned, 0);
  if(READ(run_count,unsigned) > MAX_RUNS) {
    TRANSITION_TO(task_done);
  }
  else {
    WRITE(iter, 0, unsigned, 0);
    TRANSITION_TO(task_init);
  }
}

// TODO add loop conditions
void task_done()
{
  disable();
  PRINTF("Split finish: %u events %u missed\r\n", _numEvents,
                                      _numEvents_missed);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  APP_FINISHED;
  while(1);
}

void event_timer() {
  EV_TIMER_START;
  _numEvents++;
  LOG("\r\nevent %u\r\n", EV_READ(new_samp, unsigned));
  EV_WRITE(complete, 0, unsigned, 0);
  // Read in new data
  if(EV_READ(iter,unsigned) != EV_READ(last_iter, unsigned)) {
    sample_t locsample = acquire_sample(EV_READ(prev_sample, unsigned));
    LOG("measure: %u\r\n", locsample);
    EV_WRITE(prev_sample, locsample, unsigned, 0);
    EV_WRITE(sample, locsample, unsigned, 0);
    EV_WRITE(last_iter, EV_READ(iter, unsigned), unsigned, 0);
    EV_WRITE(new_samp, 1, unsigned, 0);
  }
  test_ns = EV_READ(new_samp, unsigned);
  EV_WRITE(complete, 1, unsigned, 0);
  EVENT_RETURN();
}

INIT_FUNC(init)
ENTRY_TASK(task_init)
EVENT_DISABLE_FUNC(disable)
EVENT_ENABLE_FUNC(enable)
