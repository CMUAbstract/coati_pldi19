// This is the one we used for eval!!
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


TASK(1,  task_init)
TASK(2,  task_generate_key)
TASK(6,  task_generate_key_in_tx)
TASK(3,  task_insert)
TASK(4,  task_calc_indexes)
TASK(5,  task_calc_indexes_in_tx)
TASK(7,  task_add) // TODO: rename: add 'insert' prefix
TASK(8,  task_relocate)
TASK(9,  task_insert_done)
TASK(10, task_lookup)
TASK(11, task_lookup_search)
TASK(12, task_lookup_done)
TASK(13, task_print_stats)
TASK(14, task_done)
DEFERRED_EVENT(15, event_timer)
TASK(18, task_pre_init)

// For instrumentation
__nv unsigned _numEvents = 0;
__nv unsigned _numEvents_missed = 0;
__nv unsigned wrote_key = 0;

__nv fingerprint_t filter[NUM_BUCKETS];
__nv index_t index;
__nv value_t key;
__nv task_t* after_synch_task;
__nv fingerprint_t fingerprint;
__nv value_t index1;
__nv value_t index2;
__nv value_t relocation_count;
__nv value_t insert_ev_count;
__nv value_t insert_tsk_count;
__nv value_t insert_count;
__nv value_t inserted_count;
__nv value_t lookup_count;
__nv value_t member_count;
__nv uint8_t success;
__nv uint8_t member;
__nv value_t iter = 0;
__nv value_t init_key = 0x0001; // seeds the pseudo-random sequence of keys
__nv lock_t init_key_lock;

#if USING_TIMER

void enable() {
  TA0CCTL0 |= CCIE;
  return;
}

void disable() {
  TA0CCTL0 &= ~CCIE;
  return;
}

void __attribute__((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR(void) {
  event_handler(CONTEXT_REF(event_timer));
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
  EV_TIMER_STOP
  
 SET_EV_TRANS  TRANS_TIMER_START
  TOP_HALF_RETURN(TASK_REF(event_timer));
  enable();
}

#endif



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


void task_pre_init()
{ TX_BEGIN
  disable();
  capybara_transition(0);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
	unsigned i;
	for (i = 0; i < NUM_BUCKETS ; ++i) {
		TX_WRITE(filter[i],0,fingerprint_t,0);
	}
	TX_WRITE(insert_count,0,value_t,0);
	TX_WRITE(lookup_count,0,value_t,0);
	TX_WRITE(insert_tsk_count,0,value_t,0);
	TX_WRITE(insert_ev_count,0,value_t,0);
	TX_WRITE(inserted_count,0,value_t,0);
	TX_WRITE(member_count,0,value_t,0);
	TX_WRITE(key,READ(init_key, value_t),value_t,0);
	TX_TRANSITION_TO(task_generate_key_in_tx);
}

void task_init()
{ TX_BEGIN
  disable();
  capybara_transition(0);
  enable();
	unsigned i;
	for (i = 0; i < NUM_BUCKETS ; ++i) {
		TX_WRITE(filter[i],0,fingerprint_t,0);
	}
	TX_WRITE(insert_count,0,value_t,0);
	TX_WRITE(insert_tsk_count,0,value_t,0);
	TX_WRITE(insert_ev_count,0,value_t,0);
	TX_WRITE(lookup_count,0,value_t,0);
	TX_WRITE(inserted_count,0,value_t,0);
	TX_WRITE(member_count,0,value_t,0);
	TX_WRITE(key,READ(init_key, value_t),value_t,0);
	LOG("Starting inserts!! %x\r\n", TX_READ(key, value_t));
	TX_TRANSITION_TO(task_generate_key_in_tx);
}


void task_generate_key()
{
	LOG("generate key %x\r\n",READ(init_key, value_t));

	// insert pseufo-random integers, for testing
	// If we use consecutive ints, they hash to consecutive DJB hashes...
	// NOTE: we are not using rand(), to have the sequence available to verify
	// that that are no false negatives (and avoid having to save the values).
	//GV(key) = (GV(key) + 1) * 17;
	WRITE(key, ((READ(key,value_t) + 1) << 4 ) + READ(key,value_t), value_t, 0);
	LOG("generate_key: key: %x\r\n", READ(key,value_t));
  TRANSITION_TO(task_lookup);
}

void task_generate_key_in_tx()
{
	LOG("generate key %x\r\n",TX_READ(init_key, value_t));

	// insert pseufo-random integers, for testing
	// If we use consecutive ints, they hash to consecutive DJB hashes...
	// NOTE: we are not using rand(), to have the sequence available to verify
	// that that are no false negatives (and avoid having to save the values).
	//GV(key) = (GV(key) + 1) * 17;
	TX_WRITE(key, ((TX_READ(key,value_t) + 1) << 4 ) + TX_READ(key,value_t), value_t, 0);
	LOG("generate_key in tx: key: %x\r\n", TX_READ(key,value_t));
  TX_TRANSITION_TO(task_insert);
}

void task_calc_indexes_in_tx()
{
	TX_WRITE(fingerprint,hash_to_fingerprint(TX_READ(key,value_t)),fingerprint_t,0);
	LOG("calc indexes in tx: fingerprint: key %04x fp %04x\r\n", TX_READ(key,value_t),
                                            TX_READ(fingerprint,fingerprint_t));

	TX_WRITE(index1,hash_to_index(TX_READ(key,value_t)),value_t,0);
	LOG("calc indexes in tx: index1: key %04x idx1 %u\r\n", TX_READ(key,value_t),
                                                  TX_READ(index1,value_t));
	index_t fp_hash = hash_to_index(TX_READ(fingerprint,fingerprint_t));
	TX_WRITE(index2,TX_READ(index1,value_t) ^ fp_hash,value_t,0);

	LOG("calc indexes in tx: index2: fp hash: %04x idx1 %u idx2 %u\r\n",
			fp_hash, TX_READ(index1,value_t), TX_READ(index2,value_t));
  TX_TRANSITION_TO(task_add);
}


void task_calc_indexes()
{
	WRITE(fingerprint,hash_to_fingerprint(READ(key,value_t)),fingerprint_t,0);
	LOG("calc indexes: fingerprint: key %04x fp %04x\r\n", READ(key,value_t),
                                            READ(fingerprint,fingerprint_t));

	WRITE(index1,hash_to_index(READ(key,value_t)),value_t,0);
	LOG("calc indexes: index1: key %04x idx1 %u\r\n", READ(key,value_t),
                                                  READ(index1,value_t));
	index_t fp_hash = hash_to_index(READ(fingerprint,fingerprint_t));
	WRITE(index2,READ(index1,value_t) ^ fp_hash,value_t,0);

	LOG("calc indexes: index2: fp hash: %04x idx1 %u idx2 %u\r\n",
			fp_hash, READ(index1,value_t), READ(index2,value_t));
  TRANSITION_TO(task_lookup_search);
}

// This task is redundant.
// Alpaca never needs this but since Chain code had it, leaving it for fair comparison.
void task_insert()
{
	LOG("insert: key %04x\r\n", TX_READ(key,value_t));
	TX_TRANSITION_TO(task_calc_indexes_in_tx);
}

void task_add()
{
	// Fingerprint being inserted
	LOG("add: fp %04x\r\n", TX_READ(fingerprint,fingerprint_t));
  index_t temp_ind1 = TX_READ(index1, value_t);
  index_t temp_ind2 = TX_READ(index2, value_t);
	// index1,fp1 and index2,fp2 are the two alternative buckets
	LOG("add: idx1 %u fp1 %04x\r\n", temp_ind1,
  TX_READ(filter[temp_ind1],fingerprint_t));

	if (!TX_READ(filter[temp_ind1], fingerprint_t)) {
		LOG("add: filled empty slot at idx1 %u\r\n", temp_ind1);

		TX_WRITE(success,true,uint8_t,0);
	  TX_WRITE(filter[temp_ind1],TX_READ(fingerprint,fingerprint_t),fingerprint_t,0);
		TX_TRANSITION_TO(task_insert_done);
	} else {
		LOG("add: fp2 %04x\r\n", TX_READ(filter[temp_ind2],fingerprint_t));
		if (!TX_READ(filter[temp_ind2],fingerprint_t)) {
			LOG("add: filled empty slot at idx2 %u\r\n", temp_ind2);

			TX_WRITE(success,true,uint8_t,0);
			TX_WRITE(filter[temp_ind2],TX_READ(fingerprint,fingerprint_t),fingerprint_t,0);
			TX_TRANSITION_TO(task_insert_done);
		} else { // evict one of the two entries
			fingerprint_t fp_victim;
			index_t index_victim;

			if (rand() % 2) {
				index_victim = temp_ind1;
				fp_victim = TX_READ(filter[temp_ind1],fingerprint_t);
			} else {
				index_victim = temp_ind2;
				fp_victim = TX_READ(filter[temp_ind2],fingerprint_t);
			}

			LOG("add: evict [%u] = %04x\r\n", index_victim, fp_victim);

			// Evict the victim
			TX_WRITE(filter[index_victim],TX_READ(fingerprint,fingerprint_t),fingerprint_t,0);
			TX_WRITE(index1,index_victim,value_t,0);
			TX_WRITE(fingerprint,fp_victim,fingerprint_t,0);
			TX_WRITE(relocation_count,0,value_t,0);

			TX_TRANSITION_TO(task_relocate);
		}
	}
}

void task_relocate()
{
	fingerprint_t fp_victim = TX_READ(fingerprint,fingerprint_t);
	index_t fp_hash_victim = hash_to_index(fp_victim);
	index_t index2_victim = TX_READ(index1,value_t) ^ fp_hash_victim;

	LOG("relocate: victim fp hash %04x idx1 %u idx2 %u\r\n",
			fp_hash_victim, TX_READ(index1,value_t), index2_victim);

	LOG("relocate: next victim fp %04x\r\n",
                      TX_READ(filter[index2_victim],fingerprint_t));


	if (!TX_READ(filter[index2_victim],fingerprint_t)) { // slot was free
		TX_WRITE(success,true,uint8_t,0);
		TX_WRITE(filter[index2_victim],fp_victim,fingerprint_t,0);
		TX_TRANSITION_TO(task_insert_done);
	} else { // slot was occupied, rellocate the next victim

		LOG("relocate: relocs %u\r\n", TX_READ(relocation_count, value_t));

		if (TX_READ(relocation_count, value_t) >= MAX_RELOCATIONS) { // insert failed
			PRINTF("relocate: max relocs reached: %u\r\n", TX_READ(relocation_count, value_t));
			TX_WRITE(success,false,uint8_t,0);
			TX_TRANSITION_TO(task_insert_done);
		}
    TX_WRITE(relocation_count,TX_READ(relocation_count, value_t) + 1, value_t,0);
		TX_WRITE(index1,index2_victim, value_t,0);
		TX_WRITE(fingerprint,TX_READ(filter[index2_victim],fingerprint_t),fingerprint_t,0);
		TX_WRITE(filter[index2_victim],fp_victim,fingerprint_t,0);
		TX_TRANSITION_TO(task_relocate);
	}
}

void task_insert_done()
{
#if VERBOSE > 2
	unsigned i;

	LOG("insert done: filter:\r\n");
	for (i = 0; i < NUM_BUCKETS; ++i) {

		LOG("%04x ", TX_READ(filter[i],fingerprint_t));
		if (i > 0 && (i + 1) % 8 == 0)
			LOG("\r\n");
	}
	LOG("\r\n");
#endif
  TX_WRITE(insert_count,TX_READ(insert_count, value_t)+1,value_t,0);
  value_t temp =  TX_READ(success, uint8_t);
	TX_WRITE(inserted_count,TX_READ(inserted_count, value_t) + temp, value_t, 0);
  TX_WRITE(insert_tsk_count, TX_READ(insert_tsk_count, value_t) + temp, value_t, 0);
  PRINTF("insert done: insert tsk %u , insert ev %u inserted total %u\r\n",
                TX_READ(insert_tsk_count, value_t),
                TX_READ(insert_ev_count, value_t),
                TX_READ(inserted_count, value_t));
  if(TX_READ(insert_tsk_count, value_t) + TX_READ(insert_ev_count, value_t) !=
                                          TX_READ(inserted_count, value_t)) {
    PRINTF("Error! Inconsistent tsk and ev insert counts\r\n");
    disable();
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    while(1);
  }
	if (TX_READ(insert_count, value_t) < NUM_INSERTS) {
		TX_TRANSITION_TO(task_generate_key_in_tx);
	} else {
		TX_WRITE(key,init_key, value_t,0);
    LOG("Going to lookups %x!\r\n", TX_READ(key, value_t));
		TX_END_TRANSITION_TO(task_generate_key);
	}
}

void task_lookup()
{
	LOG("lookup: key %04x\r\n", READ(key,value_t));
	TRANSITION_TO(task_calc_indexes);
}

void task_lookup_search()
{
	LOG("lookup search: fp %04x idx1 %u idx2 %u\r\n", READ(fingerprint,fingerprint_t),
                                        READ(index1,value_t), READ(index2,value_t));
	value_t temp1 = READ(index1, value_t);
	value_t temp2 = READ(index2, value_t);
  LOG("lookup search: fp1 %04x\r\n", READ(filter[temp1],fingerprint_t));

	if (READ(filter[temp1],fingerprint_t) == READ(fingerprint,fingerprint_t)) {
		WRITE(member,true,uint8_t,0);
	} else {
		LOG("lookup search: fp2 %04x\r\n", READ(filter[temp2],fingerprint_t));

		if (READ(filter[temp2],fingerprint_t) == READ(fingerprint,fingerprint_t)) {
			WRITE(member,true,uint8_t,0);
		}
		else {
			WRITE(member,false,uint8_t,0);
		}
	}

	LOG("lookup search: fp %04x member %u\r\n", READ(fingerprint,fingerprint_t),
                                              READ(member,uint8_t));

	if (!READ(member,uint8_t)) {
		PRINTF("lookup: key %04x not member\r\n", READ(fingerprint,fingerprint_t));
	}

	TRANSITION_TO(task_lookup_done);
}

void task_lookup_done()
{
  WRITE(lookup_count, READ(lookup_count,value_t) + 1, value_t, 0);
  WRITE(member_count, READ(member_count,value_t) + READ(member,uint8_t),
                                                            value_t, 0);
	LOG("lookup done: lookups %u members %u\r\n", READ(lookup_count,value_t),
                                                READ(member_count,value_t));

	if (READ(lookup_count,value_t) < NUM_LOOKUPS) {
		TRANSITION_TO(task_generate_key);
	} else {
		TRANSITION_TO(task_print_stats);
	}
}

void task_print_stats()
{ disable();
  //capybara_transition(1);
  unsigned i;
  PRINTF("Run %u:\r\n", READ(iter, value_t));
  /*
	BLOCK_PRINTF_BEGIN();
	for (i = 0; i < NUM_BUCKETS; ++i) {
		BLOCK_PRINTF("%04x ", READ(filter[i], fingerprint_t));
		if (i > 0 && (i + 1) % 8 == 0){
			BLOCK_PRINTF("\r\n");
		}
	}
	BLOCK_PRINTF_END();*/
  PRINTF("Cuckoo run %u\r\n",READ(iter,value_t));
	PRINTF("stats: inserts %u members %u total %u with %u events\r\n",
			READ(inserted_count, value_t), READ(member_count,value_t),
      NUM_INSERTS, _numEvents);
	TRANSITION_TO(task_done);
}

void task_done()
{
	WRITE(iter, READ(iter, value_t) + 1, unsigned, 0);
	if(READ(iter,value_t) > MAX_ITER){
    disable();
    capybara_transition(0);
    PRINTF("split cuckoo final: %u events %u missed\r\n", _numEvents,
                                                  _numEvents_missed);
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    APP_FINISHED;
    while(1);
	}
  else {
    TRANSITION_TO(task_init);
  }
}

#if USING_TIMER
void init()
{
	capybara_init();
  TA0CCR0 = 500;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
	PRINTF(".%u.\r\n", curctx->task->idx);
}
#else
void init()
{
	capybara_init();
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
  //LOG("Cuckoo\r\n");
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  if(wrote_key) {
    PRINTF("Error! partial updates to struct!\r\n");
    while(1);
  }
  TIMER_INIT
}
#endif

int event_victim_relocate(index_t ind1, fingerprint_t fp) {
	fingerprint_t fp_victim = fp;
	index_t fp_hash_victim = hash_to_index(fp_victim);
	index_t index2_victim = ind1 ^ fp_hash_victim;
  unsigned reloc_count = 0;
  // keep moving stuff until everything has found a home or we've hit our max
  // number of relocations
  while(1) {
    if (!EV_READ(filter[index2_victim],fingerprint_t)) {
      // slot was free
      EV_WRITE(filter[index2_victim],fp_victim,fingerprint_t,0);
      return 0;
    } else { // slot was occupied, rellocate the next victim
      if (reloc_count >= MAX_RELOCATIONS) {
        // insert failed
        PRINTF("relocate: max relocs reached: %u\r\n", reloc_count);
        return 1;
      }
      reloc_count++;
      ind1 = index2_victim;
      fp = EV_READ(filter[index2_victim],fingerprint_t);
      EV_WRITE(filter[index2_victim],fp_victim,fingerprint_t,0);
      // Update for next round
      fp_victim = fp;
      fp_hash_victim = hash_to_index(fp_victim);
      index2_victim = ind1 ^ fp_hash_victim;
    }
  }
  return 0;
}

void event_timer() {
  EV_TIMER_START;
  _numEvents++;
  LOG("EV\r\n");
  value_t temp;
  // Update init
  temp = EV_READ(init_key,value_t);
  temp = (temp << 1);
  // Impose some bounds for our sanity
  if(temp > 0xFF)
    temp = 0x10;
  EV_WRITE(init_key, temp, value_t, 0);
  EV_WRITE(wrote_key, 1, unsigned, 0);

  value_t temp_key;
  fingerprint_t temp_fp;
  index_t temp_ind1, temp_ind2;
  // Gen key based on current key.
  temp_key = ((EV_READ(key,value_t) + 1) << 4 ) + EV_READ(key,value_t);
  // Update key
  EV_WRITE(key, temp_key, value_t, 0);
  // Calc fingerprint
  temp_fp = hash_to_fingerprint(temp_key);
  // Calc index1
  temp_ind1 = hash_to_index(temp_key);
  // Calc index2
  temp_ind2 = temp_ind1^hash_to_index(temp_fp);
  // Read from filter and check location
	if (!EV_READ(filter[temp_ind1], fingerprint_t)) {
	  // Add to filter directly
    EV_WRITE(filter[temp_ind1],temp_fp,fingerprint_t,0);
	} else {
    // Check the second possible index
		if (!EV_READ(filter[temp_ind2],fingerprint_t)) {
			EV_WRITE(filter[temp_ind2],temp_fp,fingerprint_t,0);
      EV_WRITE(inserted_count, EV_READ(inserted_count, value_t) + 1, value_t,
      0);
      EV_WRITE(insert_ev_count, EV_READ(insert_ev_count, value_t) + 1, value_t,
      0);
      LOG("Inserted!\r\n");
		} else { // evict one of the two entries
			fingerprint_t fp_victim;
			index_t index_victim;
			if (rand() % 2) {
				index_victim = temp_ind1;
				fp_victim = EV_READ(filter[temp_ind1],fingerprint_t);
			} else {
				index_victim = temp_ind2;
				fp_victim = EV_READ(filter[temp_ind2],fingerprint_t);
			}
			// Evict the victim
			EV_WRITE(filter[index_victim],temp_fp,fingerprint_t,0);
      // Relocate the victim
      if(event_victim_relocate(index_victim, fp_victim) == 0) {
        EV_WRITE(inserted_count, EV_READ(inserted_count, value_t) + 1, value_t,
        0);
        EV_WRITE(insert_ev_count, EV_READ(insert_ev_count, value_t) + 1, value_t,
        0);
        LOG("Inserted!\r\n");
      }
		}
	}
  EV_WRITE(wrote_key, 0, unsigned, 0);
  EVENT_RETURN();
}


EVENT_ENABLE_FUNC(enable)
EVENT_DISABLE_FUNC(disable)
ENTRY_TASK(task_pre_init)
INIT_FUNC(init)
