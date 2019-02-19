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


#include "pins.h"

#define zero 0
#define one  1
#define two  2
#define three 3
#define four 4

capybara_task_cfg_t pwr_configs[5] = {
  CFG_ROW(zero, CONFIGD, LOWP,zero),
  CFG_ROW(one, CONFIGD, MEDLOWP,zero),
  CFG_ROW(two, CONFIGD, LOWP,zero),
  CFG_ROW(three, CONFIGD, LOWP,zero),
  CFG_ROW(four, CONFIGD, LOWP,zero),
};


#define USING_TIMER 0
#define MAX_ITER 24
#define NUM_INSERTS (NUM_BUCKETS / 4) // shoot for 25% occupancy
#define NUM_LOOKUPS (NUM_INSERTS)
#define NUM_BUCKETS 128 // must be a power of 2
#define MAX_RELOCATIONS 8
#define BUFFER_SIZE 32
#define BUFFER_SHIFT 5

typedef uint16_t value_t;
typedef uint16_t hash_t;
typedef uint16_t fingerprint_t;
typedef uint16_t index_t; // bucket index
typedef struct lock_ {
  uint8_t id;
  uint8_t state;
} lock_t;

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
__nv unsigned _numEvents = 0;
__nv unsigned reloc_overflow = 0;
__nv unsigned lookup_e_count = 0;
__nv unsigned wrote_key = 0;

__nv fingerprint_t isr_victim = 0;

TASK(task_init)
TASK(task_generate_key)
TASK(task_insert)
TASK(task_calc_indexes)
TASK(task_add) // TODO: rename: add 'insert' prefix
TASK(task_relocate)
TASK(task_insert_done)
TASK(task_lookup)
TASK(task_lookup_search)
TASK(task_lookup_done)
TASK(task_print_stats)
TASK(task_done)
TASK(task_acquire)
TASK(task_release)
TASK(task_pre_init)

GLOBAL_SB(fingerprint_t, filter, NUM_BUCKETS);
GLOBAL_SB(index_t, index);
GLOBAL_SB(value_t, key);
GLOBAL_SB(value_t, init_key) = 0x0001;
GLOBAL_SB(void *, next_task);
GLOBAL_SB(void*, after_synch_task);
//GLOBAL_SB(task_t*, next_task);
//GLOBAL_SB(task_t*, after_synch_task);
GLOBAL_SB(fingerprint_t, fingerprint);
GLOBAL_SB(value_t, index1);
GLOBAL_SB(value_t, index2);
GLOBAL_SB(value_t, relocation_count);
GLOBAL_SB(value_t, insert_count);
GLOBAL_SB(value_t, insert_ev_count);
GLOBAL_SB(value_t, insert_tsk_count);
GLOBAL_SB(value_t, inserted_count);
GLOBAL_SB(value_t, lookup_count);
GLOBAL_SB(value_t, member_count);
GLOBAL_SB(bool, success);
GLOBAL_SB(bool, member);
GLOBAL_SB(value_t, iter) = 0;
GLOBAL_SB(lock_t, init_key_lock);


static value_t init_key = 0x0001; // seeds the pseudo-random sequence of keys

static hash_t djb_hash(uint8_t* data, unsigned len)
{
	uint16_t hash = 5381;
	unsigned int i;

	for(i = 0; i < len; data++, i++)
		hash = ((hash << 5) + hash) + (*data);


	return hash & 0xFFFF;
}

static index_t hash_to_index(fingerprint_t fp)
{
	hash_t hash = djb_hash((uint8_t *)&fp, sizeof(fingerprint_t));
	return hash & (NUM_BUCKETS - 1); // NUM_BUCKETS must be power of 2
}

static fingerprint_t hash_to_fingerprint(value_t key)
{
	return djb_hash((uint8_t *)&key, sizeof(value_t));
}

void task_acquire() { enable();
	enable();
  GV(init_key_lock).state = 1;
  LOG("acquire after synch id = %x\r\n",GV(after_synch_task));
  // Subbed in from libalpaca
  context_t *next_ctx;
	next_ctx = (curctx == &context_0 ? &context_1 : &context_0);
	next_ctx->task = GV(after_synch_task);
	next_ctx->numRollback = 0;
	curctx = next_ctx;
  //transition_to(GV(after_synch_task));
}

void task_release() { enable();
  GV(init_key_lock).state = 0;
  LOG("release after synch id = %x\r\n",GV(after_synch_task));
  // Subbed in from libalpaca
  context_t *next_ctx;
	next_ctx = (curctx == &context_0 ? &context_1 : &context_0);
	next_ctx->task = GV(after_synch_task);
	next_ctx->numRollback = 0;
	curctx = next_ctx;
  //transition_to(GV(after_synch_task));
}

// This name is a hack so that Alpaca leaves this function alone. We're only
// calling it from within the ISR, so we'll add the ISR tag to the name even
// though this function is not, on it's own, and entire ISR
int victim_relocate_ISR(index_t ind1, fingerprint_t fp) {
	fingerprint_t fp_victim = fp;
	index_t fp_hash_victim = hash_to_index(fp_victim);
	index_t index2_victim = ind1 ^ fp_hash_victim;
  unsigned reloc_count = 0;
  // keep moving stuff until everything has found a home or we've hit our max
  // number of relocations
  while(1) {
    if (!GV(filter, index2_victim)) {
      // slot was free
      GV(filter, index2_victim) =fp_victim;
      return 0;
    } else { // slot was occupied, rellocate the next victim
      if (reloc_count >= MAX_RELOCATIONS) {
        isr_victim = fp_victim;
        // insert failed
        //PRINTF("relocate: max relocs reached: %u\r\n", reloc_count);
        return 1;
      }
      reloc_count++;
      ind1 = index2_victim;
      fp = GV(filter,index2_victim);
      GV(filter, index2_victim) = fp_victim;
      // Update for next round
      fp_victim = fp;
      fp_hash_victim = hash_to_index(fp_victim);
      index2_victim = ind1 ^ fp_hash_victim;
    }
  }
  return 0;
}
#if USING_TIMER
void __attribute__((interrupt(0))) Timer0_A0_ISR(void) {
#else
void __attribute__((interrupt(0))) Port_3_ISR(void) {
  P3IFG &= ~BIT5;
#endif
  value_t temp;
  lookup_e_count++;
  _numEvents++;
  // Only change init if we can nab the lock
  if(!GV(init_key_lock).state) {
    // Change init key value
    temp = GV(init_key);
    temp = (temp << 1);
    // Impose some bounds for our sanity
    if(temp > 0xFF)
      temp = 0x10;
    GV(init_key) = temp;
  }
  wrote_key = 1;
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  value_t temp_key;
  fingerprint_t temp_fp;
  index_t temp_ind1, temp_ind2;
  // Gen key based on current key.
  temp_key = ((GV(key) + 1) << 4 ) + GV(key);
  // Update key
  GV(key) = temp_key;
  // Calc fingerprint
  temp_fp = hash_to_fingerprint(temp_key);
  // Calc index1
  temp_ind1 = hash_to_index(temp_key);
  // Calc index2
  temp_ind2 = temp_ind1^hash_to_index(temp_fp);
  // Read from filter and check location
	if (!GV(filter, temp_ind1)) {
	  // Add to filter directly
    GV(filter, temp_ind1) = temp_fp;
	} else {
    // Check the second possible index
		if (!GV(filter, temp_ind2)) {
			GV(filter, temp_ind2) = temp_fp;
      GV(inserted_count) =  GV(inserted_count) + 1;
      GV(insert_ev_count) = GV(insert_ev_count) + 1;
		} else { // evict one of the two entries
			fingerprint_t fp_victim;
			index_t index_victim;
			if (rand() % 2) {
				index_victim = temp_ind1;
				fp_victim = GV(filter, temp_ind1);
			} else {
				index_victim = temp_ind2;
				fp_victim = GV(filter, temp_ind2);
			}
			// Evict the victim
			GV(filter, index_victim) = temp_fp;
      // Relocate the victim
      if(victim_relocate_ISR(index_victim, fp_victim) == 0) {
        GV(inserted_count) =  GV(inserted_count) + 1;
        GV(insert_ev_count) = GV(insert_ev_count) + 1;
      }
      else {
        reloc_overflow++;
      }
		}
	}
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  wrote_key = 0;
}

#if USING_TIMER
__attribute__((section("__interrupt_vector_timer0_a0"),aligned(2)))
void(*__vector_timer0_a0)(void) = Timer0_A0_ISR;
#else
__attribute__((section("__interrupt_vector_port3"),aligned(2)))
void(*__vector_port3)(void) = Port_3_ISR;
#endif

void task_pre_init() {
  disable();
  capybara_transition(0);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
	unsigned i;
	for (i = 0; i < NUM_BUCKETS ; ++i) {
		GV(filter, i) =0;
	}
	GV(insert_count) = 0;
	GV(lookup_count) = 0;
	GV(inserted_count) = 0;
	GV(insert_tsk_count) = 0;
	GV(member_count) = 0;
	GV(key) = GV(init_key);
  void* temptask = TASK_REF(task_insert);
	GV(next_task) = temptask;
  temptask = TASK_REF(task_generate_key);
	GV(after_synch_task) = temptask;
  PRINTF("Starting inserts!! %x opcfg[1] = %u\r\n", GV(key),
                        pwr_configs[1].opcfg->banks);
	disable();{TRANSITION_TO(task_acquire)};
}

void task_init()
{ disable(); capybara_transition(0);
	unsigned i;
	for (i = 0; i < NUM_BUCKETS ; ++i) {
		GV(filter, i) = 0;
	}
	GV(insert_count) = 0;
	GV(insert_ev_count) = 0;
	GV(insert_tsk_count) = 0;
	GV(lookup_count) = 0;
	GV(inserted_count) = 0;
	GV(member_count) = 0;
	GV(key) = GV(init_key);
	GV(next_task) = TASK_REF(task_insert);
	LOG("init end!!\r\n");
	disable();{TRANSITION_TO(task_generate_key)};
}


void task_generate_key() { enable();

	LOG("generate key start\r\n");

	// insert pseufo-random integers, for testing
	// If we use consecutive ints, they hash to consecutive DJB hashes...
	// NOTE: we are not using rand(), to have the sequence available to verify
	// that that are no false negatives (and avoid having to save the values).
	//GV(key) = (GV(key) + 1) * 17;
	GV(key) = ((GV(key) + 1) << 4 ) + GV(key);
	LOG("generate_key: key: %x\r\n", GV(key));
  // Subbed in from libalpaca
  context_t *next_ctx;
	next_ctx = (curctx == &context_0 ? &context_1 : &context_0);
	next_ctx->task = GV(next_task);
	next_ctx->numRollback = 0;
	curctx = next_ctx;
	//transition_to(GV(next_task));
}

void task_calc_indexes() { enable();

	GV(fingerprint) = hash_to_fingerprint(GV(key));
	LOG("calc indexes: fingerprint: key %04x fp %04x\r\n", GV(key), GV(fingerprint));

	GV(index1) = hash_to_index(GV(key));
	LOG("calc indexes: index1: key %04x idx1 %u\r\n", GV(key), GV(index1));

	index_t fp_hash = hash_to_index(GV(fingerprint));
	GV(index2) = GV(index1) ^ fp_hash;

	LOG("calc indexes: index2: fp hash: %04x idx1 %u idx2 %u\r\n",
			fp_hash, GV(index1), GV(index2));
  context_t *next_ctx;
	next_ctx = (curctx == &context_0 ? &context_1 : &context_0);
	next_ctx->task = GV(next_task);
	next_ctx->numRollback = 0;
	curctx = next_ctx;
	//transition_to(GV(next_task));
}

// This task is redundant.
// Alpaca never needs this but since Chain code had it, leaving it for fair comparison.
void task_insert() { enable();

	LOG("insert: key %04x, init_key %04x\r\n", GV(key), GV(init_key));
	GV(next_task) = TASK_REF(task_add);
	disable();{TRANSITION_TO(task_calc_indexes)};
}

void task_add() { enable();

	// Fingerprint being inserted
	LOG("add: fp %04x\r\n", GV(fingerprint));

	// index1,fp1 and index2,fp2 are the two alternative buckets
	LOG("add: idx1 %u fp1 %04x\r\n", GV(index1), GV(filter, _global_index1));

	if (!GV(filter, _global_index1)) {
		LOG("add: filled empty slot at idx1 %u\r\n", GV(index1));

		GV(success) = true;
		GV(filter, _global_index1) = GV(fingerprint);
		disable();{TRANSITION_TO(task_insert_done)};
	} else {
		LOG("add: fp2 %04x\r\n", GV(filter, _global_index2));
		if (!GV(filter, _global_index2)) {
			LOG("add: filled empty slot at idx2 %u\r\n", GV(index2));

			GV(success) = true;
			GV(filter, _global_index2) = GV(fingerprint);
			disable();{TRANSITION_TO(task_insert_done)};
		} else { // evict one of the two entries
			fingerprint_t fp_victim;
			index_t index_victim;

			if (rand() % 2) {
				index_victim = GV(index1);
				fp_victim = GV(filter, _global_index1);
			} else {
				index_victim = GV(index2);
				fp_victim = GV(filter, _global_index2);
			}

			LOG("add: evict [%u] = %04x\r\n", index_victim, fp_victim);

			// Evict the victim
			GV(filter, index_victim) = GV(fingerprint);
			GV(index1) = index_victim;
			GV(fingerprint) = fp_victim;
			GV(relocation_count) = 0;

			disable();{TRANSITION_TO(task_relocate)};
		}
	}
}

void task_relocate() { enable();

	fingerprint_t fp_victim = GV(fingerprint);
	index_t fp_hash_victim = hash_to_index(fp_victim);
	index_t index2_victim = GV(index1) ^ fp_hash_victim;

	LOG("relocate: victim fp hash %04x idx1 %u idx2 %u\r\n",
			fp_hash_victim, GV(index1), index2_victim);

	LOG("relocate: next victim fp %04x\r\n", GV(filter, index2_victim));


	if (!GV(filter, index2_victim)) { // slot was free
		GV(success) = true;
		GV(filter, index2_victim) = fp_victim;
		disable();{TRANSITION_TO(task_insert_done)};
	} else { // slot was occupied, rellocate the next victim

		LOG("relocate: relocs %u\r\n", GV(relocation_count));

		if (GV(relocation_count) >= MAX_RELOCATIONS) { // insert failed
			PRINTF("relocate: max relocs reached: %u, evicting %04x\r\n", 
                            GV(relocation_count), fp_victim);
			GV(success) = false;
			disable();{TRANSITION_TO(task_insert_done)};
		}

		++GV(relocation_count);
		GV(index1) = index2_victim;
		GV(fingerprint) = GV(filter, index2_victim);
		GV(filter, index2_victim) = fp_victim;
		disable();{TRANSITION_TO(task_relocate)};
	}
}

void task_insert_done() { enable();

#if VERBOSE > 0
	unsigned i;

	LOG("insert done: filter:\r\n");
	for (i = 0; i < NUM_BUCKETS; ++i) {

		LOG("%04x ", GV(filter, i));
		if (i > 0 && (i + 1) % 8 == 0)
			LOG("\r\n");
	}
	LOG("\r\n");
#endif

	++GV(insert_count);
	disable();
  GV(inserted_count) += GV(success);
  GV(insert_tsk_count) += GV(success);
  enable();
  PRINTF("insert done: insert tsk %u , insert ev %u inserted total %u\r\n",
                GV(insert_tsk_count),GV(insert_ev_count),GV(inserted_count));
  if((GV(insert_tsk_count) + GV(insert_ev_count)) != GV(inserted_count)) {
    disable();
    PRINTF("Error! counts don't match %u %u %u \r\n", GV(insert_tsk_count), 
                      GV(insert_ev_count), GV(inserted_count));
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    while(1);
  }

	if (GV(insert_count) < NUM_INSERTS) {
		GV(next_task) = TASK_REF(task_insert);
		disable();{TRANSITION_TO(task_generate_key)};
	} else {
    LOG("Going to lookups %x!\r\n", READ(key, value_t));
	  // Task to head to after releasing lock
    GV(after_synch_task) = TASK_REF(task_generate_key);	
    // Task to head to after generating key
		GV(next_task) = TASK_REF(task_lookup);
    // Set new key
		GV(key) = GV(init_key);
		disable();{TRANSITION_TO(task_release)};
	}
}

void task_lookup() { enable();

	PRINTF("lookup: key %04x init %04x\r\n", GV(key), GV(init_key));
  lookup_e_count = 0;
	GV(next_task) = TASK_REF(task_lookup_search);
	disable();{TRANSITION_TO(task_calc_indexes)};
}

void task_lookup_search() { enable();

	LOG("lookup search: fp %04x idx1 %u idx2 %u\r\n", GV(fingerprint), GV(index1), GV(index2));
	LOG("lookup search: fp1 %04x\r\n", GV(filter, _global_index1));

	if (GV(filter, _global_index1) == GV(fingerprint)) {
		GV(member) = true;
	} else {
		LOG("lookup search: fp2 %04x\r\n", GV(filter, _global_index2));

		if (GV(filter, _global_index2) == GV(fingerprint)) {
			GV(member) = true;
		}
		else {
			GV(member) = false;
		}
	}

	LOG("lookup search: fp %04x member %u\r\n", GV(fingerprint), GV(member));

	if (!GV(member)) {
		PRINTF("lookup: key %04x not member, %u events\r\n",
                                          GV(fingerprint), lookup_e_count);
	}

	disable();{TRANSITION_TO(task_lookup_done)};
}

void task_lookup_done() { enable();

	++GV(lookup_count);

	GV(member_count) += GV(member);
	LOG("lookup done: lookups %u members %u\r\n", GV(lookup_count), GV(member_count));

	if (GV(lookup_count) < NUM_LOOKUPS) {
		GV(next_task) = TASK_REF(task_lookup);
		disable();{TRANSITION_TO(task_generate_key)};
	} else {
		disable();{TRANSITION_TO(task_print_stats)};
	}
}

void task_print_stats()
{ disable();
  //PRINTF("Changing power levels!!! to %u\r\n", pwr_configs[1].opcfg->banks);
  PRINTF("Run %u:\r\n", GV(iter));
  //capybara_transition(1);
  unsigned i;
  /*
	BLOCK_PRINTF_BEGIN();
	//BLOCK_PRINTF("filter:\r\n");
  // Only print a section
	for (i = 0; i < NUM_BUCKETS/4; ++i) {
		BLOCK_PRINTF("%04x ", GV(filter, i));
		if (i > 0 && (i + 1) % 8 == 0){
			BLOCK_PRINTF("\r\n");
		}
	}
	BLOCK_PRINTF_END();*/
  PRINTF("Cuckoo run %u\r\n",GV(iter));
	PRINTF("stats: inserts %u members %u total %u, with %u events\r\n",
			GV(inserted_count), GV(member_count), NUM_INSERTS, _numEvents);
  if(reloc_overflow) {
    PRINTF("%04x was last evicted, total evictions = %u\r\n",
                                          isr_victim,reloc_overflow);
  }
  isr_victim = 0x0;
  reloc_overflow = 0;
	disable();{TRANSITION_TO(task_done)};
}

void task_done() { enable();
  GV(iter)++;
	if(GV(iter) > MAX_ITER){
    disable();
    capybara_transition(0);
    PRINTF("Alpaca cuckoo final: %u events\r\n",_numEvents);
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    while(1);
	}
  else {
    void*temptask = TASK_REF(task_init);
    GV(after_synch_task) = temptask;
    disable();{TRANSITION_TO(task_acquire)};
  }
}

#if USING_TIMER
void init()
{
  capybara_init();
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  //TA0CCR0 = 700;
  /*for(int i = 0; i < 1000; i++) {
    __delay_cycles(4000);
  }*/
  TA0CCR0 = 500;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
  //TA0CCTL0 |= CCIE;
	PRINTF(".%u.\r\n", curctx->task->idx);
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
  LOG("RSA\r\n");
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  if(wrote_key) {
    PRINTF("Error! partial updates to struct!\r\n");
    while(1);
  }
}


#endif

__attribute__((interrupt(0)))
void(*__vector_compe_e)(void) = COMP_VBANK_ISR;

ENTRY_TASK(task_pre_init)
INIT_FUNC(init)
