// This app demonstrates how activity recognition would run if it were pulling
// its data from a sensor. To keep things deterministic we use a timer, but you
// get the idea. Data is only ever collected via the event. We wrap a couple
// crucial tasks as a transaction so that the event can't overwrite the data
// they're working with. This file is called ar_nonid because the event, if it
// were actually a plain interrupt handler, would not be idempotent and could
// corrupt state if it were only partially written. Realistically this file
// should be called ar_atomic because the major problem is that a non-atomic
// update of the variables modified by the event would cause the program to
// crash (thanks to the "done" flag in every buffer slot).
// App for coati eval
#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <libmspware/driverlib.h>

#include <libcoatigcc/coati.h>
#include <libcoatigcc/tx.h>
#include <libcoatigcc/filter.h>
#include <libcoatigcc/event.h>

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

#ifdef CONFIG_LIBEDB_PRINTF
#include <libedb/edb.h>
#endif
#include "pins.h"

#define USING_TIMER 0
// Number of samples to discard before recording training set
#define NUM_WARMUP_SAMPLES 3

#define ACCEL_WINDOW_SIZE 3
//#define MODEL_SIZE 8
#define MODEL_SIZE 16
#define SAMPLE_NOISE_FLOOR 10 // TODO: made up value

// Number of classifications to complete in one experiment
#define SAMPLES_TO_COLLECT 4

typedef struct diy_three_axis {
  uint16_t x;
  uint16_t y;
  uint16_t z;
  uint8_t read;
  uint8_t done;
  uint16_t timestamp;
} mythreeAxis_t;

unsigned volatile *timer = &TBCTL;
typedef mythreeAxis_t accelReading;
typedef accelReading accelWindow[ACCEL_WINDOW_SIZE];

typedef struct {
	unsigned meanmag;
	unsigned stddevmag;
} features_t;


typedef enum {
	CLASS_STATIONARY,
	CLASS_MOVING,
} class_t;

unsigned overflow=0;
#if 0
__attribute__((interrupt(51))) 
	void TimerB1_ISR(void){
		TBCTL &= ~(0x0002);
		if(TBCTL && 0x0001){
			overflow++;
			TBCTL |= 0x0004;
			TBCTL |= (0x0002);
			TBCTL &= ~(0x0001);	
		}
	}
__attribute__((section("__interrupt_vector_timer0_b1"),aligned(2)))
void(*__vector_timer0_b1)(void) = TimerB1_ISR;
#endif

typedef enum {
	// MODE_IDLE = (BIT(PIN_AUX_1) | BIT(PIN_AUX_2)),
	//  MODE_TRAIN_STATIONARY = BIT(PIN_AUX_1),
	//  MODE_TRAIN_MOVING = BIT(PIN_AUX_2),
	MODE_IDLE = 3,
	MODE_TRAIN_STATIONARY = 2,
	MODE_TRAIN_MOVING = 1,
	MODE_RECOGNIZE = 0, // default
} run_mode_t;

TASK(1, task_init)
TASK(2, task_selectMode)
TASK(3, task_resetStats)
TASK(4, task_sample)
TASK(5, task_transform)
TASK(6, task_featurize)
TASK(7, task_classify)
TASK(8, task_stats)
TASK(9, task_warmup)
TASK(10, task_train)
TASK(11, task_idle)
EVENT(12, event_timer)

// For instrumentation only
__nv unsigned  _numEvents = 0;
__nv unsigned complete = 0;
__nv unsigned test_siw;
__nv unsigned delayed = 0;

__nv uint16_t pinState;
__nv unsigned discardedSamplesCount;
__nv run_mode_t class;
__nv unsigned totalCount;
__nv unsigned movingCount;
__nv unsigned stationaryCount;
__nv accelReading window[ACCEL_WINDOW_SIZE];
__nv accelReading fillwindow[ACCEL_WINDOW_SIZE];
__nv features_t features;
__nv features_t model_stationary[MODEL_SIZE];
__nv features_t model_moving[MODEL_SIZE];
__nv unsigned trainingSetSize;
__nv unsigned samplesInWindow;
__nv run_mode_t mode;
__nv unsigned seed;
__nv unsigned count;
__nv samplingIndex;

void ACCEL_singleSample_(mythreeAxis_t* result){
	result->x = (EV_READ(seed, unsigned)*17)%85;
	result->y = (EV_READ(seed, unsigned)*17*17)%85;
	result->z = (EV_READ(seed, unsigned)*17*17*17)%85;
  EV_WRITE(seed, EV_READ(seed, unsigned) + 1, unsigned, 0);
  LOG2("%u %u %u %u\r\n",EV_READ(seed, unsigned),
                          result->x, result->y,result->z);
}

#if USING_TIMER
void init()
{
	capybara_init();
  mythreeAxis_t accelID = {0};
  TA0CCR0 = 500;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
  TIMER_INIT

  PRINTF("ar\r\n",curctx->task->idx);
  /*for(int i=0;i < 256;i ++) {
    __delay_cycles(0xFFFF);
  }*/
}

void disable() {
  TA0CCTL0 &= ~CCIE;
  return;
}

void enable() {
  TA0CCTL0 |= CCIE;
  return;
}

void __attribute__((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR(void) {
  event_handler(CONTEXT_REF(event_timer));
}
#else
void init()
{
	capybara_init();
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  mythreeAxis_t accelID = {0};
  P3SEL1 &= ~BIT5; // Configure for GPIO
  P3SEL0 &= ~BIT5;
  P3OUT &= ~BIT5;	// Set P3.5 as  pull down
  P3DIR &= ~BIT5; // Set P3.5 as input
  P3REN |= BIT5; // enable input pull up/down

  P3IES &= ~BIT5; // Set IFG on high-->low
  P3IFG &= ~BIT5; // Clear flag bit
  TIMER_INIT
  /*for(int i=0;i < 256;i ++) {
    __delay_cycles(0xFFFF);
  }*/
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
  EV_TIMER_START
  
 SET_EV_TRANS  TRANS_TIMER_START
  event_handler(CONTEXT_REF(event_timer));
}
#endif

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

// Zero out counters and seed, set pin state to IDLE
void task_init()
{ disable();
  capybara_transition(0);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
  LOG("samplesInWindow = %x\r\n",&samplesInWindow);
  LOG("init and %u %u\r\n",sizeof(class_t),sizeof(run_mode_t));
	NI_WRITE(pinState, MODE_IDLE, uint16_t, 0);
  NI_WRITE(count, 0, unsigned, 0);
  NI_WRITE(seed, 1, unsigned, 0);
  NI_WRITE(samplesInWindow, 0, unsigned, 0);
  NI_WRITE(samplingIndex, 0, unsigned, 0);
	NI_TRANSITION_TO(task_selectMode);
}

// Bizarre mode selection routine, we set pin_state based on how many times
// we've run classification, I think
void task_selectMode()
{ disable();capybara_transition(0);enable();
	run_mode_t locpin_state=MODE_TRAIN_MOVING;
  unsigned temp_count = READ(count,unsigned);
  temp_count++;
	WRITE(count, temp_count, unsigned, 0);
	LOG("count: %u\r\n",READ(count,unsigned));
  if(temp_count >= 3) locpin_state=MODE_TRAIN_STATIONARY;
	if(temp_count >=5) locpin_state=MODE_RECOGNIZE;
	if (temp_count >= 7) {
	  PRINTF("%u events %u missed\r\n",_numEvents, _numEvents_uncommitted);
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    disable();
		APP_FINISHED;
    while(1);
		//TRANSITION_TO(task_init);
	}
	// uint16_t locpin_state = GPIO(PORT_AUX, IN) & (BIT(PIN_AUX_1) | BIT(PIN_AUX_2));

	// Don't re-launch training after finishing training
	if ((locpin_state == MODE_TRAIN_STATIONARY ||
				locpin_state == MODE_TRAIN_MOVING) &&
			locpin_state == READ(pinState,uint16_t)) {
		locpin_state = MODE_IDLE;
	} else {
    WRITE(pinState, locpin_state, uint16_t, 0);
	}
	switch(locpin_state) {
		case MODE_TRAIN_STATIONARY:
      WRITE(discardedSamplesCount, 0, unsigned, 0);
      // TODO checkt that run_mode_t is 8 or 16 bits
      WRITE(mode, MODE_TRAIN_STATIONARY, run_mode_t, 0);
      WRITE(class, CLASS_STATIONARY, run_mode_t, 0);
			WRITE(samplesInWindow, 0, unsigned, 0);

			TRANSITION_TO(task_warmup);
			break;

		case MODE_TRAIN_MOVING:
      //while(1) {
      LOG("Caught\r\n");
      /*for(int i=0;i < 10;i ++) {
        __delay_cycles(0xFFFF);
      }*/
      WRITE(discardedSamplesCount, 0, unsigned, 0);
      // TODO checkt that run_mode_t is 8 or 16 bits
      LOG("wrote\r\n");
      WRITE(mode, MODE_TRAIN_MOVING, run_mode_t, 0);
      WRITE(class, CLASS_MOVING, run_mode_t, 0);
			WRITE(samplesInWindow, 0, unsigned, 0);
      LOG("Done, %u\r\n", READ(mode, run_mode_t));
      //}
			TRANSITION_TO(task_warmup);
			break;

		case MODE_RECOGNIZE:
      WRITE(mode, MODE_RECOGNIZE, run_mode_t, 0);

			TRANSITION_TO(task_resetStats);
			break;

		default:
			TRANSITION_TO(task_idle);
	}
}

void task_resetStats()
{
	// NOTE: could roll this into selectMode task, but no compelling reason
  disable();capybara_transition(0);enable();
	LOG("resetStats\r\n");

	// NOTE: not combined into one struct because not all code paths use both
	WRITE(movingCount, 0, unsigned, 0);
	WRITE(stationaryCount, 0, unsigned, 0);
	WRITE(totalCount, 0, unsigned, 0);
	WRITE(samplesInWindow, 0, unsigned, 0);
  WRITE(samplingIndex, 0, unsigned, 0);
	TRANSITION_TO(task_sample);
}

void task_sample()
{
  #if USING_TIMER
  P3OUT |= BIT5;
  P3DIR |= BIT5;
  P3OUT &= ~BIT5;
  #endif
  TX_BEGIN
  disable();capybara_transition(0);enable();
  LOG("tsk sample\r\n");
  if(complete && delayed &&
    !(((ev_state *)(curctx->extra_ev_state))->ev_need_commit) &&
    (TX_READ(samplesInWindow, unsigned) != test_siw)) {
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    PRINTF("Erorr! masked update in task_sample! %u %u",
                        TX_READ(samplesInWindow, unsigned), test_siw);
    while(1);
  }
  complete = 0;
  delayed = 0;
	accelReading sample;
  // Need to write these separately because coati doesn't support structs
  //TODO check that this doesn't change anything!!!
  unsigned numSamps = TX_READ(samplesInWindow, unsigned);
  unsigned sampIndex = TX_READ(samplingIndex, unsigned);
  LOG("got %u samples\r\n",sampIndex);
  if(numSamps == ACCEL_WINDOW_SIZE) {
    WAIT_TIMER_STOP;
    LOG("writing to sample %u\r\n",sampIndex);
    sample.x = TX_READ(window[sampIndex].x, uint16_t);
    sample.y = TX_READ(window[sampIndex].y, uint16_t);
    sample.z = TX_READ(window[sampIndex].z, uint16_t);
    LOG("%u %u %u on seed %u\r\n",sample.x, sample.y, sample.z,
                                            TX_READ(seed,unsigned));
    if(TX_READ(window[sampIndex].done, uint8_t) == 0) {
      P1OUT |= BIT5;
      P1DIR |= BIT5;
      P1OUT &= ~BIT5;
      PRINTF("Error! Incomplete entry.\r\n");
      while(1);
    }
    else {
      TX_WRITE(samplingIndex,sampIndex + 1, unsigned, 0);
      LOG("sample: sample %u %u %u window %u\r\n",
          sample.x, sample.y, sample.z, sampIndex);
    }
  }
  // If we need more samples, end tx and start again
	if (sampIndex < ACCEL_WINDOW_SIZE - 1) {
    delayed = 1;
    WAIT_TIMER_START;
    __delay_cycles(4000);
	  NI_TX_END_TRANSITION_TO(task_sample);
	} else {
    TX_WRITE(samplingIndex, 0, unsigned, 0);
		TX_TRANSITION_TO(task_transform);
	}
}

void task_transform()
{
  disable();capybara_transition(0);enable();
	unsigned i;

	LOG("transform\r\n");

	for (i = 0; i < ACCEL_WINDOW_SIZE; i++) {
    uint16_t x = TX_READ(window[i].x, uint16_t);
    uint16_t y = TX_READ(window[i].y, uint16_t);
    uint16_t z = TX_READ(window[i].z, uint16_t);
    LOG("transform got %u %u %u\r\n",x,y,z);
		if (x < SAMPLE_NOISE_FLOOR ||
				y < SAMPLE_NOISE_FLOOR ||
				z < SAMPLE_NOISE_FLOOR) {
      LOG("Running transform on %u %u %u\r\n",x,y,z);
      if(x > SAMPLE_NOISE_FLOOR) {
        TX_WRITE(window[i].x, x, uint16_t, 0);
      }
      else {
        TX_WRITE(window[i].x, 0, uint16_t, 0);
      }

      if(y > SAMPLE_NOISE_FLOOR) {
        TX_WRITE(window[i].y, y, uint16_t, 0);
      }
      else {
        TX_WRITE(window[i].y, 0, uint16_t, 0);
      }

      if(z > SAMPLE_NOISE_FLOOR) {
        TX_WRITE(window[i].z, z, uint16_t, 0);
      }
      else {
        TX_WRITE(window[i].z, 0, uint16_t, 0);
      }
		}
	}
	TX_TRANSITION_TO(task_featurize);
}

void task_featurize()
{
  disable();capybara_transition(0);enable();
	accelReading mean, stddev;
	mean.x = mean.y = mean.z = 0;
	stddev.x = stddev.y = stddev.z = 0;
	features_t locfeatures;

	LOG("featurize\r\n");

	int i;
	for (i = 0; i < ACCEL_WINDOW_SIZE; i++) {
		LOG("featurize: features: x %u y %u z %u \r\n", TX_READ(window[i].x, uint16_t),
                                                    TX_READ(window[i].y, uint16_t),
                                                    TX_READ(window[i].z, uint16_t));
		mean.x += TX_READ(window[i].x, uint16_t);
		mean.y += TX_READ(window[i].y, uint16_t);
		mean.z += TX_READ(window[i].z, uint16_t);
	}
	mean.x >>= 2;
	mean.y >>= 2;
	mean.z >>= 2;
  stddev.x = 0;
  stddev.y = 0;
  stddev.z = 0;
	LOG("featurize: features: mx %u my %u mz %u \r\n", mean.x,mean.y,mean.z);
	for (i = 0; i < ACCEL_WINDOW_SIZE; i++) {
    uint16_t x, y, z;
    x = TX_READ(window[i].x, uint16_t);
    y = TX_READ(window[i].y, uint16_t);
    z = TX_READ(window[i].z, uint16_t);
    LOG("Using %u %u %u\r\n",x,y,z);
		stddev.x += x > mean.x ? x - mean.x	: mean.x - x;
		stddev.y += y > mean.y ? y - mean.y	: mean.y - y;
		stddev.z += z > mean.z ? z - mean.z	: mean.z - z;
    LOG("stddev.x: %u stddev.y: %u stddev.z: %u\r\n",stddev.x, stddev.y,
                                                                  stddev.z);
	}
	stddev.x >>= 2;
	stddev.y >>= 2;
	stddev.z >>= 2;

	unsigned meanmag = mean.x*mean.x + mean.y*mean.y + mean.z*mean.z;
	unsigned stddevmag = stddev.x*stddev.x + stddev.y*stddev.y + stddev.z*stddev.z;
	locfeatures.meanmag   = sqrt16(meanmag);
	locfeatures.stddevmag = sqrt16(stddevmag);
  LOG("featurize: stddevmag = %u, meanmag = %u\r\n", stddevmag, meanmag);
	LOG("featurize: features: mean %u stddev %u\r\n",
			locfeatures.meanmag, locfeatures.stddevmag);
  LOG("mode = %u\r\n",TX_READ(mode,run_mode_t));
	switch (TX_READ(mode, run_mode_t)) {
		case MODE_TRAIN_STATIONARY:
		case MODE_TRAIN_MOVING:
			TX_WRITE(features.meanmag, locfeatures.meanmag, unsigned, 0);
			TX_WRITE(features.stddevmag, locfeatures.stddevmag, unsigned, 0);
			LOG("Going to train!\r\n");
      TX_WRITE(samplesInWindow, 0, unsigned, 0);
      TX_END_TRANSITION_TO(task_train);
			break;
		case MODE_RECOGNIZE:
			TX_WRITE(features.meanmag, locfeatures.meanmag, unsigned, 0);
			TX_WRITE(features.stddevmag, locfeatures.stddevmag, unsigned, 0);
      TX_WRITE(samplesInWindow, 0, unsigned, 0);
			TX_END_TRANSITION_TO(task_classify);
			break;
		default:
			// TODO: abort
			break;
	}
}

void task_classify() {
  disable();capybara_transition(0);enable();
	int move_less_error = 0;
	int stat_less_error = 0;
	int i;
	long int meanmag;
	long int stddevmag;
	LOG("classify\r\n");
	meanmag = READ(features.meanmag, unsigned);
	stddevmag = READ(features.stddevmag, unsigned);
	LOG("classify: mean: %u\r\n", meanmag);
	LOG("classify: stddev: %u\r\n", stddevmag);

	for (i = 0; i < MODEL_SIZE; ++i) {
    unsigned temp_stat_mag, temp_stat_std, temp_mov_mag, temp_mov_std;
    temp_stat_mag = READ(model_stationary[i].meanmag, unsigned);
    temp_stat_std = READ(model_stationary[i].stddevmag, unsigned);
		long int stat_mean_err = (temp_stat_mag > meanmag) ? temp_stat_mag - meanmag
                                                      : meanmag - temp_stat_mag;

		long int stat_sd_err = (temp_stat_std > stddevmag) ? temp_stat_std - stddevmag
                                                    : stddevmag - temp_stat_std;
		LOG("classify: model_mean: %u\r\n", temp_stat_mag);
		LOG("classify: model_stddev: %u\r\n", temp_stat_std);
		LOG("classify: stat_mean_err: %u\r\n", stat_mean_err);
		LOG("classify: stat_stddev_err: %u\r\n", stat_sd_err);
    temp_mov_mag = READ(model_moving[i].meanmag, unsigned);
    temp_mov_std = READ(model_moving[i].stddevmag, unsigned);
		long int move_mean_err = (temp_mov_mag > meanmag) ? temp_mov_mag - meanmag
                                                      : meanmag - temp_mov_mag;

		long int move_sd_err = (temp_mov_std > stddevmag) ? temp_mov_std - stddevmag
                                                    : stddevmag - temp_mov_std;

		LOG("classify: model_mean: %u\r\n", temp_mov_mag);
		LOG("classify: model_stddev: %u\r\n",temp_mov_std);
		LOG("classify: move_mean_err: %u\r\n", move_mean_err);
		LOG("classify: move_stddev_err: %u\r\n", move_sd_err);
		if (move_mean_err < stat_mean_err) {
			move_less_error++;
		} else {
			stat_less_error++;
		}

		if (move_sd_err < stat_sd_err) {
			move_less_error++;
		} else {
			stat_less_error++;
		}
	}
  // Make sure class_t is an 8 or 16 bit number
	class_t temp = (move_less_error > stat_less_error) ? CLASS_MOVING : CLASS_STATIONARY;
  WRITE(class, temp, class_t, 0);
	LOG("classify: class 0x%x, stat_err = %u, move_err = %u\r\n", temp,
                                                     move_less_error, stat_less_error);

	TRANSITION_TO(task_stats);
}

void task_stats()
{
  disable();capybara_transition(0);enable();

	LOG("stats\r\n");
  WRITE(totalCount, READ(totalCount, unsigned) + 1, unsigned, 0);
  PRINTF("stats: total %u\r\n", READ(totalCount, unsigned));

	switch (READ(class, class_t)) {
		case CLASS_MOVING:

      WRITE(movingCount, READ(movingCount, unsigned) + 1, unsigned, 0);
			LOG("stats: moving %u\r\n",READ(movingCount,unsigned));
			break;
		case CLASS_STATIONARY:

      WRITE(stationaryCount, READ(stationaryCount, unsigned) + 1, unsigned, 0);
	    LOG("stats: stationary %u\r\n", READ(stationaryCount,unsigned));
			break;
	}

	if (READ(totalCount,unsigned) == SAMPLES_TO_COLLECT) {

		unsigned resultStationaryPct = READ(stationaryCount, unsigned) * 100
                                                    / READ(totalCount, unsigned);
		unsigned resultMovingPct = READ(movingCount, unsigned) * 100 /
                                                      READ(totalCount, unsigned);

		unsigned sum = READ(stationaryCount,unsigned) + READ(movingCount, unsigned);
    PRINTF("stats: s %u (%u%%) m %u (%u%%) sum/tot %u/%u: %c\r\n",
		       READ(stationaryCount, unsigned), resultStationaryPct,
		       READ(movingCount, unsigned), resultMovingPct,
		       READ(totalCount, unsigned), sum,
           sum == READ(totalCount, unsigned) ? 'V' : 'X');
		TRANSITION_TO(task_idle);
	} else {
    WRITE(samplingIndex, 0, unsigned, 0);
		TRANSITION_TO(task_sample);
	}
}

void task_warmup()
{
  disable();capybara_transition(0);enable();
	mythreeAxis_t sample;
  uint16_t temp = READ(mode, uint16_t);
	LOG2("warmup, %u\r\n", temp);
  if(complete && delayed && (READ(samplesInWindow, unsigned) != test_siw)) {
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    PRINTF("Erorr! masked update in task_warmup!");
    while(1);
  }
  complete = 0;
  delayed = 0;
  // Delay if we don't have enough samples
  if (READ(samplesInWindow, unsigned) < ACCEL_WINDOW_SIZE) {
    delayed = 1;
    WAIT_TIMER_START;
    while(READ(samplesInWindow, unsigned) < ACCEL_WINDOW_SIZE) {
      __bis_SR_register(LPM3_bits + GIE);
    }
    //__delay_cycles(40000);
    NI_TRANSITION_TO(task_warmup);
  }
	else {
    WAIT_TIMER_START;
    unsigned num_discard = READ(discardedSamplesCount, unsigned);
    // check if we've gotten enough warmup samples
    if (num_discard < NUM_WARMUP_SAMPLES) {
      WRITE(window[num_discard].read, 1, uint8_t, 0);
      WRITE(window[num_discard].done, 0, uint8_t, 0);
      WRITE(discardedSamplesCount, num_discard + 1, unsigned, 0);
      PRINTF("warmup: discarded %u\r\n", num_discard);
      TRANSITION_TO(task_warmup);
	  } else {
      PRINTF("warmup: done\r\n");
      WRITE(samplesInWindow, 0, unsigned, 0);
      WRITE(trainingSetSize, 0, unsigned, 0);
      WRITE(samplingIndex, 0, unsigned, 0);
      complete = 0;
      TRANSITION_TO(task_sample);
	  }
  }
}

void task_train()
{
  disable();capybara_transition(0);enable();
	LOG("train\r\n");
  unsigned index;
	switch (READ(class, class_t)) {
		case CLASS_STATIONARY:
      LOG("In stationary!\r\n");
      index = READ(trainingSetSize, unsigned);
      WRITE(model_stationary[index].meanmag, READ(features.meanmag, unsigned),
                                                                 unsigned, 0);
      WRITE(model_stationary[index].stddevmag, READ(features.stddevmag, unsigned),
                                                                 unsigned, 0);
			break;
		case CLASS_MOVING:
      index = READ(trainingSetSize, unsigned);
      WRITE(model_moving[index].meanmag, READ(features.meanmag, unsigned),
                                                                 unsigned, 0);
      WRITE(model_moving[index].stddevmag, READ(features.stddevmag, unsigned),
                                                                 unsigned, 0);
			break;
    default:
      PRINTF("Wrong type for class in trianing!\r\n");
      break;
	}
  WRITE(trainingSetSize, READ(trainingSetSize, unsigned) + 1, unsigned, 0);
	PRINTF("train: class %u count %u/%u\r\n", READ(class, class_t),
			READ(trainingSetSize, unsigned), MODEL_SIZE);

	if (READ(trainingSetSize, unsigned) < MODEL_SIZE) {
    WRITE(samplingIndex, 0, unsigned, 0);
		TRANSITION_TO(task_sample);
	} else {
		//        PRINTF("train: class %u done (mn %u sd %u)\r\n",
		//               class, features.meanmag, features.stddevmag);
		TRANSITION_TO(task_idle);
	}
}

void task_idle() {
  disable();capybara_transition(0);enable();
	LOG("idle\r\n");

	TRANSITION_TO(task_selectMode);
}

void event_timer() {
  _numEvents++;
  capybara_transition(0);
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  unsigned num_samps = EV_READ(samplesInWindow, unsigned);
  LOG("%u samples in window\r\n",num_samps);
  complete = 0;
  if(num_samps < ACCEL_WINDOW_SIZE) {
    LOG("Writing to sample %u\r\n",num_samps);
    mythreeAxis_t sample;
    ACCEL_singleSample_(&sample);
    EV_WRITE(window[num_samps].read, 0, uint8_t, 0);
    EV_WRITE(window[num_samps].x, sample.x, uint16_t, 0);
    EV_WRITE(window[num_samps].y, sample.y, uint16_t, 0);
    EV_WRITE(window[num_samps].z, sample.z, uint16_t, 0);
    EV_WRITE(window[num_samps].done, 1, uint8_t, 0);
    EV_WRITE(samplesInWindow,num_samps + 1, unsigned, 0);
    test_siw = num_samps + 1;
    complete = 1;
  }
  LOG("Now %u samps in window\r\n",EV_READ(samplesInWindow, unsigned));
  EVENT_RETURN();
}

INIT_FUNC(init)
ENTRY_TASK(task_init)
EVENT_DISABLE_FUNC(disable)
EVENT_ENABLE_FUNC(enable)
