// This is the valid version for eval!!
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

// Number of samples to discard before recording training set
#define NUM_WARMUP_SAMPLES 3

#define USING_TIMER 0
#define ACCEL_WINDOW_SIZE 3
#define MODEL_SIZE 16
#define SAMPLE_NOISE_FLOOR 10 // TODO: made up value

// Number of classifications to complete in one experiment
#define SAMPLES_TO_COLLECT 16

typedef struct diy_three_axis {
  uint16_t x;
  uint16_t y;
  uint16_t z;
  uint8_t read;
  uint8_t done;
  uint16_t timestamp;
} mythreeAxis_t;

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

__attribute__((interrupt(0))) 
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

TASK(task_init)
TASK(task_selectMode)
TASK(task_resetStats)
TASK(task_sample)
TASK(task_transform)
TASK(task_featurize)
TASK(task_classify)
TASK(task_stats)
TASK(task_warmup)
TASK( task_train)
TASK( task_idle)
TASK( task_delay)
TASK( task_acquire)
TASK( task_release)
// For instrumentation only
__nv unsigned _numEvents = 0;
__nv unsigned _numEvents_missed = 0;
__nv unsigned test_siw = 0;
__nv unsigned complete = 0;
__nv unsigned delayed = 0;

typedef struct lock_ {
  uint8_t id;
  uint8_t state;
} lock_t;
GLOBAL_SB(lock_t, l1);

GLOBAL_SB(uint16_t, pinState);
GLOBAL_SB(unsigned, discardedSamplesCount);
GLOBAL_SB(run_mode_t, class);
GLOBAL_SB(unsigned, totalCount);
GLOBAL_SB(unsigned, movingCount);
GLOBAL_SB(unsigned, stationaryCount);
GLOBAL_SB(accelReading, window, ACCEL_WINDOW_SIZE);
GLOBAL_SB(features_t, features);
GLOBAL_SB(features_t, model_stationary, MODEL_SIZE);
GLOBAL_SB(features_t, model_moving, MODEL_SIZE);
GLOBAL_SB(unsigned, trainingSetSize);
GLOBAL_SB(unsigned, samplesInWindow);
GLOBAL_SB(run_mode_t, mode);
GLOBAL_SB(unsigned, seed);
GLOBAL_SB(unsigned, count);
GLOBAL_SB(unsigned, samplingIndex);
//GLOBAL_SB(task_t *, after_synch_task);
GLOBAL_SB(void *, after_synch_task);

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

void task_acquire() {
  enable();
  GV(l1).state = 1;
  disable();
  LOG("acquire!\r\n");
  // Subbed in from libalpaca
  context_t *next_ctx;
	next_ctx = (curctx == &context_0 ? &context_1 : &context_0);
	next_ctx->task = GV(after_synch_task);
	next_ctx->numRollback = 0;
	curctx = next_ctx;

  //transition_to(GV(after_synch_task));
}

void task_release () {
  enable();
  GV(l1).state = 0;
  disable();
  // Subbed in from libalpaca
  context_t *next_ctx;
	next_ctx = (curctx == &context_0 ? &context_1 : &context_0);
	next_ctx->task = GV(after_synch_task);
	next_ctx->numRollback = 0;
	curctx = next_ctx;
  //transition_to(GV(after_synch_task));
}


void ACCEL_singleSample_(mythreeAxis_t* result){
	result->x = (GV(seed)*17)%85;
	result->y = (GV(seed)*17*17)%85;
	result->z = (GV(seed)*17*17*17)%85;
	++GV(seed);
  LOG("%u %u %u %u\r\n",GV(seed),
                          result->x, result->y,result->z);
}

#if USING_TIMER
void __attribute__((interrupt(0))) Timer0_A0_ISR(void) {
#else
void __attribute__((interrupt(0))) Port_3_ISR(void) {
  P3IFG &= ~BIT5;
#endif
  _numEvents++;
  //unsigned num_samps = GV(samplesInWindow);
  unsigned num_samps = _global_samplesInWindow;
  //LOG("%x\r\n",num_samps);
  //PRINTF("%x\r\n",(uint16_t)curctx->task->func);
  if(_global_l1.state) {
    //LOG("Back\r\n");
  /*P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  */
    _numEvents_missed++;
    //return;
  }
  else {
    
    complete = 0;

    if(num_samps < ACCEL_WINDOW_SIZE) {
      mythreeAxis_t sample;
      GV(window, num_samps).done = 0;
      GV(samplesInWindow) = num_samps + 1;

      sample.x = (GV(seed)*17)%85;
      sample.y = (GV(seed)*17*17)%85;
      sample.z = (GV(seed)*17*17*17)%85;
      ++GV(seed);

      GV(window, num_samps).read = 0;
      GV(window, num_samps).x = sample.x;
      GV(window, num_samps).y = sample.y;
      GV(window, num_samps).z = sample.z;
      GV(window, num_samps).done = 1;
      test_siw = GV(samplesInWindow);
      complete = 1;
      P1OUT |= BIT4;
      P1DIR |= BIT4;
      P1OUT &= ~BIT4;
    }
  }
}

#if USING_TIMER
__attribute__((section("__interrupt_vector_timer0_a0"),aligned(2)))
void(*__vector_timer0_a0)(void) = Timer0_A0_ISR;

void initializeHardware()
{
	capybara_init();
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  mythreeAxis_t accelID = {0};
  /*for(int i = 0; i < 1000; i++) {
    __delay_cycles(4000);
  }*/
  TA0CCR0 = 500;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;

    LOG("ar\r\n");
}

#else
void initializeHardware()
{
	capybara_init();
  P1OUT |= BIT0;
  P1DIR |= BIT0;
  P1OUT &= ~BIT0;
  mythreeAxis_t accelID = {0};
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
  LOG("ar\r\n");
}

__attribute__((section("__interrupt_vector_port3"),aligned(2)))
void(*__vector_port3)(void) = Port_3_ISR;

#endif


void task_init()
{ disable();
  capybara_transition(1);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
	LOG("init\r\n");
	GV(pinState) = MODE_IDLE;
	GV(count) = 0;
	GV(seed) = 1;
  GV(samplesInWindow) = 0;
  GV(samplingIndex) = 0;
  GV(l1).id = 0;
  GV(l1.)state = 0;

  disable();{TRANSITION_TO(task_selectMode)};
}


void task_selectMode()
{ disable();capybara_transition(0);enable();
	uint16_t pin_state=1;
	LOG("count: %u\r\n",GV(count));
	++GV(count);
	LOG("count: %u\r\n",GV(count));
	if(GV(count) >= 3) pin_state=2;
	if(GV(count)>=5) pin_state=0;
	if (GV(count) >= 7) {
	  PRINTF("%u events %u missed\r\n",_numEvents, _numEvents_missed);
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    while(1);
		//disable();{TRANSITION_TO(task_init)};
	}
	run_mode_t mode;
	class_t class;

	// uint16_t pin_state = GPIO(PORT_AUX, IN) & (BIT(PIN_AUX_1) | BIT(PIN_AUX_2));

	// Don't re-launch training after finishing training
	if ((pin_state == MODE_TRAIN_STATIONARY ||
				pin_state == MODE_TRAIN_MOVING) &&
			pin_state == GV(pinState)) {
		pin_state = MODE_IDLE;
	} else {
		GV(pinState) = pin_state;
	}

	LOG("selectMode: 0x%x\r\n", pin_state);

	switch(pin_state) {
		case MODE_TRAIN_STATIONARY:
			GV(discardedSamplesCount) = 0;
			GV(mode) = MODE_TRAIN_STATIONARY;
			GV(class) = CLASS_STATIONARY;
			GV(samplesInWindow) = 0;
      disable();
			disable();{{TRANSITION_TO(task_warmup)}};
			break;

		case MODE_TRAIN_MOVING:
			GV(discardedSamplesCount) = 0;
			GV(mode) = MODE_TRAIN_MOVING;
			GV(class) = CLASS_MOVING;
			GV(samplesInWindow) = 0;
      disable();

			disable();{{TRANSITION_TO(task_warmup)}};
			break;

		case MODE_RECOGNIZE:
			GV(mode) = MODE_RECOGNIZE;

			disable();{TRANSITION_TO(task_resetStats)};
			break;

		default:
			disable();{TRANSITION_TO(task_idle)};
	}
}

void task_resetStats()
{ disable();capybara_transition(0);enable();
	// NOTE: could roll this into selectMode task, but no compelling reason

	LOG("resetStats\r\n");

	// NOTE: not combined into one struct because not all code paths use both
	GV(movingCount) = 0;
	GV(stationaryCount) = 0;
	GV(totalCount) = 0;

	GV(samplesInWindow) = 0;

  disable();
  
  uint16_t temp = TASK_REF(task_sample);
  GV(after_synch_task) = temp;
  disable();{TRANSITION_TO(task_acquire)};
}

void task_delay()
{
  disable();capybara_transition(0);enable();
  uint16_t temp;
  //__delay_cycles(40000);
  temp = TASK_REF(task_sample);
  GV(after_synch_task) = temp;
  disable();{TRANSITION_TO(task_acquire)};
}

void task_sample()
{
  #if USING_TIMER
  P3OUT |= BIT5;
  P3DIR |= BIT5;
  P3OUT &= ~BIT5;
  #endif
  disable();capybara_transition(0);enable();
	LOG("sample\r\n");
  accelReading sample;
  LOG("samp: got %u samples\r\n",GV(samplesInWindow));
  if(complete && delayed &&(GV(samplesInWindow) != test_siw)) {
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    PRINTF("Erorr! masked update in task_sample!");
    while(1);
  }
  complete = 0;
  delayed = 0;

  unsigned sampIndex = GV(samplingIndex);
  if(GV(samplesInWindow) == ACCEL_WINDOW_SIZE) {
    sample.x = GV(window, sampIndex).x;
    sample.y = GV(window, sampIndex).y;
    sample.z = GV(window, sampIndex).z;
    LOG("%u %u %u on seed %u\r\n",sample.x, sample.y, sample.z,
                                      GV(seed));
    if(GV(window, sampIndex).done == 0) {
      P1OUT |= BIT5;
      P1DIR |= BIT5;
      P1OUT &= ~BIT5;
      PRINTF("Error! Incomplete entry.\r\n");
      while(1);
    }
    else {
      GV(samplingIndex) = sampIndex + 1;
    }
  }
  // If we need more samples, release the locks and start again
	if (sampIndex < ACCEL_WINDOW_SIZE - 1) {
    LOG("Releasing\r\n");
    uint16_t temp = TASK_REF(task_delay);
    GV(after_synch_task) = temp;
    delayed = 1;
    disable();{TRANSITION_TO(task_release)};
	} else {
		GV(samplingIndex) = 0;
		disable();{TRANSITION_TO(task_transform)};
	}
}

void task_transform()
{ disable(0);capybara_transition(0);enable();
	unsigned i;

	LOG("transform\r\n");
	accelReading *sample;
	accelReading transformedSample;

	for (i = 0; i < ACCEL_WINDOW_SIZE; i++) {
		if (GV(window, i).x < SAMPLE_NOISE_FLOOR ||
				GV(window, i).y < SAMPLE_NOISE_FLOOR ||
				GV(window, i).z < SAMPLE_NOISE_FLOOR) {
      LOG("Running transform\r\n");
			GV(window, i).x = (GV(window, i).x > SAMPLE_NOISE_FLOOR)
				? GV(window, i).x : 0;
			GV(window, i).y = (GV(window, i).y > SAMPLE_NOISE_FLOOR)
				? GV(window, i).y : 0;
			GV(window, i).z = (GV(window, i).z > SAMPLE_NOISE_FLOOR)
				? GV(window, i).z : 0;
		}
	}
	disable();{TRANSITION_TO(task_featurize)};
}

void task_featurize()
{ disable();capybara_transition(0);enable();
	accelReading mean, stddev;
	mean.x = mean.y = mean.z = 0;
	stddev.x = stddev.y = stddev.z = 0;
	features_t features;

	LOG("featurize\r\n");

	int i;
	for (i = 0; i < ACCEL_WINDOW_SIZE; i++) {
		LOG("featurize: features: x %u y %u z %u \r\n", GV(window, i).x,
                                          GV(window, i).y,GV(window, i).z);
		mean.x += GV(window, i).x;
		mean.y += GV(window, i).y;
		mean.z += GV(window, i).z;
	}
	mean.x >>= 2;
	mean.y >>= 2;
	mean.z >>= 2;

  LOG("featurize: features: mx %u my %u mz %u \r\n", mean.x,mean.y,mean.z);
	for (i = 0; i < ACCEL_WINDOW_SIZE; i++) {
		LOG("Using %u %u %u\r\n",GV(window, i).x, GV(window, i).y,
                                                    GV(window,i).z);
    stddev.x += GV(window, i).x > mean.x ? GV(window, i).x - mean.x
			: mean.x - GV(window, i).x;
		stddev.y += GV(window, i).y > mean.y ? GV(window, i).y - mean.y
			: mean.y - GV(window, i).y;
		stddev.z += GV(window, i).z > mean.z ? GV(window, i).z - mean.z
			: mean.z - GV(window, i).z;
    LOG("stddev.x: %u stddev.y: %u stddev.z: %u\r\n",stddev.x, stddev.y,
                                                                  stddev.z);
	}
	stddev.x >>= 2;
	stddev.y >>= 2;
	stddev.z >>= 2;

	unsigned meanmag = mean.x*mean.x + mean.y*mean.y + mean.z*mean.z;
	unsigned stddevmag = stddev.x*stddev.x + stddev.y*stddev.y + stddev.z*stddev.z;
	features.meanmag   = sqrt16(meanmag);
	features.stddevmag = sqrt16(stddevmag);
  LOG("featurize: stddevmag = %u, meanmag = %u\r\n", stddevmag, meanmag);
  LOG("featurize: features: mean %u stddev %u\r\n",
			features.meanmag, features.stddevmag);
  LOG("mode = %u\r\n",GV(mode));
	switch (GV(mode)) {
		case MODE_TRAIN_STATIONARY:
		case MODE_TRAIN_MOVING:
			GV(features) = features;
      GV(samplesInWindow) = 0;
      P1OUT |= BIT0;
      P1DIR |= BIT0;
      P1OUT &= ~BIT0;
      LOG("Releasing\r\n");
      uint16_t temp = TASK_REF(task_train);
      GV(after_synch_task) = temp;
      disable();{TRANSITION_TO(task_release)};
			break;
		case MODE_RECOGNIZE:
			GV(features) = features;
      GV(samplesInWindow) = 0;
      P1OUT |= BIT0;
      P1DIR |= BIT0;
      P1OUT &= ~BIT0;
      LOG("Releasing\r\n");
      uint16_t temp1 = TASK_REF(task_classify);
      GV(after_synch_task) = temp1;
      disable();{TRANSITION_TO(task_release)};
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
	class_t class;
	long int meanmag;
	long int stddevmag;
	LOG("classify\r\n");
	meanmag = GV(features).meanmag;
	stddevmag = GV(features).stddevmag;
	LOG("classify: mean: %u\r\n", meanmag);
	LOG("classify: stddev: %u\r\n", stddevmag);

	for (i = 0; i < MODEL_SIZE; ++i) {
		long int stat_mean_err = (GV(model_stationary, i).meanmag > meanmag)
			? (GV(model_stationary, i).meanmag - meanmag)
			: (meanmag - GV(model_stationary, i).meanmag);

		long int stat_sd_err = (GV(model_stationary, i).stddevmag > stddevmag)
			? (GV(model_stationary, i).stddevmag - stddevmag)
			: (stddevmag - GV(model_stationary, i).stddevmag);
		LOG("classify: model_mean: %u\r\n", GV(model_stationary, i).meanmag);
		LOG("classify: model_stddev: %u\r\n", GV(model_stationary, i).stddevmag);
		LOG("classify: stat_mean_err: %u\r\n", stat_mean_err);
		LOG("classify: stat_stddev_err: %u\r\n", stat_sd_err);

		long int move_mean_err = (GV(model_moving, i).meanmag > meanmag)
			? (GV(model_moving, i).meanmag - meanmag)
			: (meanmag - GV(model_moving, i).meanmag);

		long int move_sd_err = (GV(model_moving, i).stddevmag > stddevmag)
			? (GV(model_moving, i).stddevmag - stddevmag)
			: (stddevmag - GV(model_moving, i).stddevmag);
		LOG("classify: model_mean: %u\r\n", GV(model_moving, i).meanmag);
		LOG("classify: model_stddev: %u\r\n", GV(model_moving, i).stddevmag);
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

	GV(class) = (move_less_error > stat_less_error) ? CLASS_MOVING : CLASS_STATIONARY;

	LOG("classify: class 0x%x, stat_err = %u, move_err = %u\r\n", GV(class),
                                                  move_less_error, stat_less_error);
	disable();{TRANSITION_TO(task_stats)};
}

void task_stats()
{ disable();capybara_transition(0);enable();
	unsigned movingCount = 0, stationaryCount = 0;

	LOG("stats\r\n");

	++GV(totalCount);
	PRINTF("stats: total %u\r\n", GV(totalCount));

	switch (GV(class)) {
		case CLASS_MOVING:

			++GV(movingCount);
			LOG("stats: moving %u\r\n", GV(movingCount));
			break;
		case CLASS_STATIONARY:

			++GV(stationaryCount);
			LOG("stats: stationary %u\r\n", GV(stationaryCount));
			break;
	}

	if (GV(totalCount) == SAMPLES_TO_COLLECT) {

		unsigned resultStationaryPct = GV(stationaryCount) * 100 / GV(totalCount);
		unsigned resultMovingPct = GV(movingCount) * 100 / GV(totalCount);

		unsigned sum = GV(stationaryCount) + GV(movingCount);
		PRINTF("stats: s %u (%u%%) m %u (%u%%) sum/tot %u/%u: %c\r\n",
		       GV(stationaryCount), resultStationaryPct,
		       GV(movingCount), resultMovingPct,
		       GV(totalCount), sum, sum == GV(totalCount) ? 'V' : 'X');
    disable();
		disable();{TRANSITION_TO(task_idle)};
	} else {
	  GV(samplingIndex) = 0;
    uint16_t temp = TASK_REF(task_sample);
    GV(after_synch_task) = temp;
    disable();{TRANSITION_TO(task_acquire)};
	}
}

void task_warmup()
{ disable();capybara_transition(0);enable();
	mythreeAxis_t sample;
	LOG("warmup: %u\r\n",GV(samplesInWindow));
  if(complete && delayed && (GV(samplesInWindow) != test_siw)) {
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    PRINTF("Erorr! masked update in task_warmup! %u %u\r\n",
                                      GV(samplesInWindow), test_siw);
    while(1);
  }
  complete = 0;
  delayed = 0;
  if (GV(samplesInWindow) < ACCEL_WINDOW_SIZE) {
    /*P3OUT |= BIT5;
    P3DIR |= BIT5;
    P3OUT &= ~BIT5;
    */
    LOG("Waiting!\r\n");
    delayed = 1;
    //__delay_cycles(40000);
    disable();{TRANSITION_TO(task_warmup)};
  }
  else {
    unsigned num_discard = GV(discardedSamplesCount);
    if (num_discard < NUM_WARMUP_SAMPLES) {
      GV(window, num_discard).read = 1;
      GV(window, num_discard).done = 0;
      ++GV(discardedSamplesCount);
      PRINTF("warmup: discarded %u\r\n", GV(discardedSamplesCount));
      //disable();
      disable();{TRANSITION_TO(task_warmup)};
    } else {
      PRINTF("warmup: done\r\n");
      GV(samplesInWindow) = 0;
      GV(trainingSetSize) = 0;
      GV(samplingIndex) = 0;
      //disable();
      uint16_t temp = TASK_REF(task_sample);
      GV(after_synch_task) = temp;
      disable();{TRANSITION_TO(task_acquire)};
    }
  }
}

void task_train()
{ disable();capybara_transition(0);enable();
	//unsigned trainingSetSize;;
	unsigned class;
	switch (GV(class)) {
		case CLASS_STATIONARY:
			GV(model_stationary, _global_trainingSetSize) = GV(features);
			break;
		case CLASS_MOVING:
			GV(model_moving, _global_trainingSetSize) = GV(features);
			break;
	}

	++GV(trainingSetSize);
	PRINTF("train: class %u count %u/%u\r\n", GV(class),
			GV(trainingSetSize), MODEL_SIZE);
	if (GV(trainingSetSize) < MODEL_SIZE) {
    disable();
      uint16_t temp = TASK_REF(task_sample);
      GV(after_synch_task) = temp;
      disable();{TRANSITION_TO(task_acquire)};
	} else {
		//        PRINTF("train: class %u done (mn %u sd %u)\r\n",
		//               class, features.meanmag, features.stddevmag);
		disable();{TRANSITION_TO(task_idle)};
	}
}

void task_idle() {
	 disable();capybara_transition(0);enable();
   LOG("idle\r\n");

	disable();{TRANSITION_TO(task_selectMode)};
}

__attribute__((interrupt(0)))
void(*__vector_compe_e)(void) = COMP_VBANK_ISR;

#if 0
__attribute__((interrupt(0)))
void(*__vector_timer0_b0)(void) = TIMER_ISR(LIBMSP_SLEEP_TIMER_TYPE, LIBMSP_SLEEP_TIMER_IDX, LIBMSP_SLEEP_TIMER_CC);
#endif
INIT_FUNC(initializeHardware)
ENTRY_TASK(task_init)
