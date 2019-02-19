// benchmark to demonstrate worst case transaction overheads
#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <libmspware/driverlib.h>

#include <libcoatigcc/coati.h>
#include <libcoatigcc/tx.h>
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


#define USING_TIMER 0
#ifdef CONFIG_LIBEDB_PRINTF
#include <libedb/edb.h>
#endif

#include "pins.h"

// #define VERBOSE

//#include "../data/keysize.h"
//#define MAX_ITER 25
#define MAX_ITER 48
#define KEY_SIZE_BITS    64
#define DIGIT_BITS       8 // arithmetic ops take 8-bit args produce 16-bit result
#define DIGIT_MASK       0x00ff
#define NUM_DIGITS       (KEY_SIZE_BITS / DIGIT_BITS)
#define INPUT_ARR_SIZE 16

/** @brief Type large enough to store a product of two digits */
typedef uint16_t digit_t;
//typedef uint8_t digit_t;

typedef struct {
	uint8_t n[NUM_DIGITS]; // modulus
	digit_t e;  // exponent
} pubkey_t;

#if NUM_DIGITS < 2
#error The modular reduction implementation requires at least 2 digits
#endif

#define LED1 (1 << 0)
#define LED2 (1 << 1)

#define SEC_TO_CYCLES 4000000 /* 4 MHz */

#define BLINK_DURATION_BOOT (5 * SEC_TO_CYCLES)
#define BLINK_DURATION_TASK SEC_TO_CYCLES
#define BLINK_BLOCK_DONE    (1 * SEC_TO_CYCLES)
#define BLINK_MESSAGE_DONE  (2 * SEC_TO_CYCLES)

#define PRINT_HEX_ASCII_COLS 8
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
// #define SHOW_PROGRESS_ON_LED
// #define SHOW_COARSE_PROGRESS_ON_LED

// Blocks are padded with these digits (on the MSD side). Padding value must be
// chosen such that block value is less than the modulus. This is accomplished
// by any value below 0x80, because the modulus is restricted to be above
// 0x80 (see comments below).
static __ro_nv const unsigned char VAL_ARRAY[INPUT_ARR_SIZE + 1][12] = {
  #include "../../../../data/extra_inputs.txt"
};

static const uint8_t PAD_DIGITS[] = { 0x01 };
#define NUM_PAD_DIGITS (sizeof(PAD_DIGITS) / sizeof(PAD_DIGITS[0]))

// To generate a key pair: see scripts/

// modulus: byte order: LSB to MSB, constraint MSB>=0x80
static __ro_nv const pubkey_t pubkey = {
#include "../../../../data/key64.txt"
};

//static __ro_nv const unsigned char PLAINTEXT[] =
// Easy way (hopefully) to initialize PLAINTEXT
__nv char PLAINTEXT[12] =
#include "../../../../data/plaintext.txt"
;

#define NUM_PLAINTEXT_BLOCKS (sizeof(PLAINTEXT) / (NUM_DIGITS - NUM_PAD_DIGITS) + 1)
#define CYPHERTEXT_SIZE (NUM_PLAINTEXT_BLOCKS * NUM_DIGITS)


TASK(1,  task_init)
TASK(2,  task_pad)
TASK(3,  task_exp)
TASK(4,  task_mult_block)
TASK(5,  task_mult_block_get_result)
TASK(6,  task_square_base)
TASK(7,  task_square_base_get_result)
TASK(8,  task_print_cyphertext)
TASK(9,  task_mult_mod)
TASK(10, task_mult)
TASK(11, task_reduce_digits)
TASK(12, task_reduce_normalizable)
TASK(13, task_reduce_normalize)
TASK(14, task_reduce_n_divisor)
TASK(15, task_reduce_quotient)
TASK(16, task_reduce_multiply)
TASK(17, task_reduce_compare)
TASK(18, task_reduce_add)
TASK(19, task_reduce_subtract)
TASK(20, task_print_product)
EVENT(21, event_timer)
TASK(22, task_loop_entry)
TASK(23, task_sleep);

// For instrumentation
__nv unsigned _numEvents = 0;
__nv unsigned _numEvents_missed = 0;
__nv unsigned test_ready = 0;
__nv unsigned complete = 0;
__nv unsigned delayed = 0;


__nv unsigned iter;
__nv digit_t product[32];
__nv digit_t exponent;
__nv digit_t exponent_next;
__nv unsigned block_offset;
__nv unsigned message_length;
__nv unsigned cyphertext_len;
__nv digit_t base[32];
__nv digit_t modulus[NUM_DIGITS];
__nv digit_t digit;
__nv digit_t carry;
__nv unsigned reduce;
__nv digit_t cyphertext[CYPHERTEXT_SIZE];
__nv unsigned offset;
__nv digit_t n_div;
__nv task_t* next_task;
__nv digit_t product2[32];
__nv task_t* next_task_print;
__nv digit_t block[32];
__nv unsigned quotient;
__nv uint8_t print_which;
__nv unsigned ready;



#if USING_TIMER
void init()
{
  capybara_init();
  TA0CCR0 = 500;
  //TA0CCR0 = 800;
  TA0CTL = TASSEL__ACLK | MC__UP | ID_2;
#ifdef CONFIG_EDB
	edb_init();
#endif
  PRINTF("rsa\r\n");
}

// TODO change these to a pin with a comparator
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

#endif

#if USING_TIMER
void __attribute__((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR(void) {
#else
void __attribute__((interrupt(PORT3_VECTOR))) Port_3_ISR(void) {
  P3IFG &= ~BIT5;
#endif
  disable();
  EV_TIMER_START;
  SET_EV_TRANS
  TRANS_TIMER_START
  event_handler(CONTEXT_REF(event_timer));
}

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

void task_init()
{ disable();
  capybara_transition(0);
  P1OUT |= BIT5;
  P1DIR |= BIT5;
  P1OUT &= ~BIT5;
	int i;
	unsigned locmessage_length = sizeof(PLAINTEXT) - 1; // skip the terminating null byte
  /*for( i = 0; i < 100; i++) {
    __delay_cycles(40000);
  }*/

	LOG("init: out modulus\r\n");

	// TODO: consider passing pubkey as a structure type
	for (i = 0; i < NUM_DIGITS; ++i) {
    NI_WRITE(modulus[i], pubkey.n[i], digit_t, 0);
	}

	LOG("init: out exp\r\n");
  NI_WRITE(message_length, locmessage_length, unsigned, 0);
	NI_WRITE(block_offset, 0, unsigned, 0);
	NI_WRITE(cyphertext_len, 0, unsigned, 0);
  NI_WRITE(iter, 0, unsigned, 0);

	LOG("init: done\r\n");

	NI_TRANSITION_TO(task_pad);
}

void task_sleep() {
  LOG("Sleepy\r\n");
  if(READ(ready, unsigned) == 0) {
    __bis_SR_register(LPM3_bits + GIE);
  }
  NI_TRANSITION_TO(task_loop_entry);
}


void task_loop_entry() {
  disable();
  capybara_transition(0);
  enable();
  TX_BEGIN
  LOG2("Buf levels: %u %u \r\n",tsk_buf_level, tx_buf_level);
  LOG2("Tx begin %u, ready: %u\r\n", TX_READ(ready, unsigned), ready);
  LOG2("Enter loop iter addr: %x\r\n", &iter);
  // Check if data are ready
  if(TX_READ(ready,unsigned) == 0) {
    LOG("Waiting!\r\n");
    WAIT_TIMER_START;
    // Cut out of here for now because the hacky checks fail if there's
    // transaction abort and then these flags through false errors.
    /*if(delayed && complete && (TX_READ(ready, unsigned) != test_ready)) {
      P1OUT |= BIT5;
      P1DIR |= BIT5;
      P1OUT &= ~BIT5;
      PRINTF("Erorr! masked update in task_warmup! %u %u\r\n",
                        TX_READ(ready, unsigned), test_ready);
      while(1);
    }*/
    delayed = 1;
    //PRINTF("go to sleep\r\n");
    NI_TX_END_TRANSITION_TO(task_sleep);
  }
  WAIT_TIMER_STOP;
  complete = 0;
  delayed = 0;

  //WRITE(iter, READ(iter, unsigned) + 1, unsigned, 0);
	int i;
	unsigned locmessage_length = sizeof(PLAINTEXT) - 1; // skip the terminating null byte

  LOG("init: out modulus %u\r\n", TX_READ(iter, unsigned));

	// TODO: consider passing pubkey as a structure type
	for (i = 0; i < NUM_DIGITS; ++i) {
    TX_WRITE(modulus[i], pubkey.n[i], digit_t, 0);
	}
  unsigned temp_iter = TX_READ(iter,unsigned);
  temp_iter++;
  TX_WRITE(iter, temp_iter, unsigned, 0);
  TX_WRITE(ready, 0, unsigned, 0);

	LOG("init: out exp\r\n");
  TX_WRITE(message_length, locmessage_length, unsigned, 0);
	TX_WRITE(block_offset, 0, unsigned, 0);
	TX_WRITE(cyphertext_len, 0, unsigned, 0);
  // Clear ready
  //WRITE(ready, 0, unsigned, 0);

  TX_TRANSITION_TO(task_pad);
}

void task_pad()
{ //P3OUT |= BIT5;
  //P3DIR |= BIT5;
  //P3OUT &= ~BIT5;
  TX_BEGIN
	int i;

	LOG("pad: size=%u len=%u offset=%u\r\n",sizeof(PLAINTEXT),
                                   TX_READ(message_length,unsigned),
                                   TX_READ(block_offset,unsigned));

	if (TX_READ(block_offset,unsigned) >= TX_READ(message_length,unsigned)) {
		LOG("pad: message done\r\n");
		TX_TRANSITION_TO(task_print_cyphertext);
	}

	LOG("process block: padded block at offset=%u: ", TX_READ(block_offset, unsigned));
	for (i = 0; i < NUM_PAD_DIGITS; ++i)
		LOG("%x ", PAD_DIGITS[i]);
	LOG("'");
	for (i = NUM_DIGITS - NUM_PAD_DIGITS - 1; i >= 0; --i)
		LOG("%x ", TX_READ(PLAINTEXT[TX_READ(block_offset,unsigned) + i], char));
	LOG("\r\n");

	for (i = 0; i < NUM_DIGITS - NUM_PAD_DIGITS; ++i) {
    digit_t temp = TX_READ(block_offset, unsigned) + i;
    digit_t temp2 = (temp < TX_READ(message_length, unsigned)) ?
                                                TX_READ(PLAINTEXT[temp],char) : 0xFF;
    TX_WRITE(base[i], temp2, digit_t, 0);
	}
	for (i = NUM_DIGITS - NUM_PAD_DIGITS; i < NUM_DIGITS; ++i) {
		TX_WRITE(base[i], 1, digit_t, 0);
	}
  TX_WRITE(block[0], 1, digit_t, 0);
	for (i = 1; i < NUM_DIGITS; ++i) {
		TX_WRITE(block[i], 0, digit_t, 0);
  }

  TX_WRITE(exponent, pubkey.e, digit_t, 0);
  TX_WRITE(block_offset, TX_READ(block_offset,unsigned) + NUM_DIGITS - NUM_PAD_DIGITS,
                                                                  unsigned, 0);

	TX_TRANSITION_TO(task_exp);
}

void task_exp()
{
	LOG("exp: e=%x\r\n", TX_READ(exponent, digit_t));

  if (TX_READ(exponent, digit_t) & 0x1) {
		TX_WRITE(exponent, TX_READ(exponent, digit_t) >> 1, digit_t, 0);
		TX_TRANSITION_TO(task_mult_block);
	} else {
		TX_WRITE(exponent, TX_READ(exponent, digit_t) >> 1, digit_t, 0);
		TX_TRANSITION_TO(task_square_base);
	}
}

// TODO: is this task strictly necessary? it only makes a call. Can this call
// be rolled into task_exp?
void task_mult_block()
{
	LOG("mult block\r\n");

	// TODO: pass args to mult: message * base
  task_t *temp = TASK_REF(task_mult_block_get_result);
  TX_WRITE(next_task, temp, uint16_t, 0);
	TX_TRANSITION_TO(task_mult_mod);
}

void task_mult_block_get_result()
{
	int i;

	LOG("mult block get result: block: ");
	for (i = NUM_DIGITS - 1; i >= 0; --i) { // reverse for printing
		TX_WRITE(block[i], TX_READ(product[i], digit_t), digit_t, 0);
		LOG("%x ", TX_READ(product[i],digit_t));
	}
	LOG("\r\n");

	// On last iteration we don't need to square base
	if (TX_READ(exponent, digit_t) > 0) {

		// TODO: current implementation restricts us to send only to the next instantiation
		// of self, so for now, as a workaround, we proxy the value in every instantiation

		TX_TRANSITION_TO(task_square_base);

	} else { // block is finished, save it
		LOG("mult block get result: cyphertext len=%u\r\n",
                                              TX_READ(cyphertext_len,unsigned));

		if (TX_READ(cyphertext_len, unsigned) + NUM_DIGITS <= CYPHERTEXT_SIZE) {

			for (i = 0; i < NUM_DIGITS; ++i) { // reverse for printing
				// TODO: we could save this read by rolling this loop into the
				// above loop, by paying with an extra conditional in the
				// above-loop.
        unsigned index = TX_READ(cyphertext_len, unsigned);
        TX_WRITE(cyphertext[index], TX_READ(product[i], digit_t), digit_t, 0);
				TX_WRITE(cyphertext_len, index + 1, unsigned, 0);
			}

		} else {
			printf("WARN: block dropped: cyphertext overlow [%u > %u]\r\n",
					TX_READ(cyphertext_len, unsigned) + NUM_DIGITS, CYPHERTEXT_SIZE);
			// carry on encoding, though
		}

		// TODO: implementation limitation: cannot multicast and send to self
		// in the same macro

		LOG("mult block get results: block done, cyphertext_len=%u\r\n",
                                            TX_READ(cyphertext_len, unsigned));
		TX_TRANSITION_TO(task_pad);
	}

}

// TODO: is this task necessary? it seems to act as nothing but a proxy
// TODO: is there opportunity for special zero-copy optimization here
void task_square_base()
{
	LOG("square base\r\n");
  task_t *temp = TASK_REF(task_square_base_get_result);
  TX_WRITE(next_task, temp, uint16_t, 0);
	TX_TRANSITION_TO(task_mult_mod);
}

// TODO: is there opportunity for special zero-copy optimization here
void task_square_base_get_result()
{
	int i;

	LOG("square base get result\r\n");

	for (i = 0; i < NUM_DIGITS; ++i) {
		LOG("square base get result: base[%u]=%x\r\n", i, TX_READ(product[i], digit_t));
    TX_WRITE(base[i], TX_READ(product[i], digit_t), digit_t, 0);
	}

	TX_TRANSITION_TO(task_exp);
}

void task_print_cyphertext()
{
	int i, j = 0;
	digit_t c;
	char line[PRINT_HEX_ASCII_COLS];

	LOG("print cyphertext: len=%u\r\n", TX_READ(cyphertext_len, unsigned));

  PRINTF("Tx iter %i \r\n",TX_READ(iter,unsigned));
	BLOCK_PRINTF_BEGIN();
	BLOCK_PRINTF("Cyphertext:\r\n");
	for (i = 0; i < TX_READ(cyphertext_len, unsigned); ++i) {
		c = TX_READ(cyphertext[i], digit_t);
		BLOCK_PRINTF("%02x ", c);
		line[j++] = c;
		if ((i + 1) % PRINT_HEX_ASCII_COLS == 0) {
			BLOCK_PRINTF(" ");
			for (j = 0; j < PRINT_HEX_ASCII_COLS; ++j) {
				c = line[j];
				if (!(32 <= c && c <= 127)) // not printable
					c = '.';
				BLOCK_PRINTF("%c", c);
			}
			j = 0;
			BLOCK_PRINTF("\r\n");
		}
	}
	BLOCK_PRINTF_END();
  for(int i = 0; i < TX_READ(message_length,unsigned);i++) {
    PRINTF("%c",TX_READ(PLAINTEXT[i], char));
  }
  PRINTF("\r\n");
  if(TX_READ(iter, unsigned) > MAX_ITER) {
    disable();
	  PRINTF("%u events %u missed\r\n",_numEvents, _numEvents_uncommitted);
	  PRINTF("Done! %u\r\n", TX_READ(iter, unsigned));
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    disable();
    APP_FINISHED;
    while(1);
  }
	//TRANSITION_TO(task_init);
  /*P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  */
  //PRINTF("Tx done\r\n");
  TX_END_TRANSITION_TO(task_loop_entry);
}

// TODO: this task also looks like a proxy: is it avoidable?
void task_mult_mod()
{
	LOG("mult mod\r\n");
  TX_WRITE(digit, 0, digit_t, 0);
  TX_WRITE(carry, 0, digit_t, 0);

	TX_TRANSITION_TO(task_mult);
}

void task_mult()
{
	int i;
	digit_t a, b, c;
	digit_t dp, p;
	LOG("mult: digit=%u carry=%x\r\n", TX_READ(digit,digit_t), TX_READ(carry,digit_t));

	p = TX_READ(carry, digit_t);
	c = 0;
	digit_t temp = TX_READ(digit, digit_t);
  LOG("Reading digit: %u\r\n", TX_READ(digit, digit_t));
	for (i = 0; i < NUM_DIGITS; ++i) {
		if (temp - i >= 0 && temp - i < NUM_DIGITS) {
      a = TX_READ(base[temp-i], digit_t);
			b = TX_READ(block[i], digit_t);
			dp = a * b;

			c += dp >> DIGIT_BITS;
			p += dp & DIGIT_MASK;

			LOG("mult: i=%u a=%x b=%x p=%x\r\n", i, a, b, p);
		}
	}

	c += p >> DIGIT_BITS;
	p &= DIGIT_MASK;

	LOG("mult: c=%x p=%x\r\n", c, p);
  TX_WRITE(product[temp], p, digit_t, 0);
	TX_WRITE(print_which, 0, uint8_t, 0);
	TX_WRITE(digit, temp + 1, digit_t, 0);

	if (TX_READ(digit, digit_t) < NUM_DIGITS * 2) {
		TX_WRITE(carry, c, digit_t, 0);
		TX_TRANSITION_TO(task_mult);
	} else {
		task_t *temptask = TASK_REF(task_reduce_digits);
    TX_WRITE(next_task_print, temptask, uint16_t, 0);
		TX_TRANSITION_TO(task_print_product);
	}
}

void task_reduce_digits()
{
	int d;

	LOG("reduce: digits\r\n");

	// Start reduction loop at most significant non-zero digit
	d = 2 * NUM_DIGITS;
	do {
		d--;
		LOG("reduce digits: p[%u]=%x\r\n", d, TX_READ(product[d], digit_t));
	} while (TX_READ(product[d], digit_t) == 0 && d > 0);

	if (TX_READ(product[d],digit_t) == 0) {
		LOG("reduce: digits: all digits of message are zero\r\n");
		TX_TRANSITION_TO(task_init);
	}
	LOG("reduce: digits: d = %u\r\n", d);
  TX_WRITE(reduce, d, unsigned, 0);

	TX_TRANSITION_TO(task_reduce_normalizable);
}

void task_reduce_normalizable()
{
	int i;
	bool normalizable = true;

	LOG("reduce: normalizable\r\n");

	// Variables:
	//   m: message
	//   n: modulus
	//   b: digit base (2**8)
	//   l: number of digits in the product (2 * NUM_DIGITS)
	//   k: number of digits in the modulus (NUM_DIGITS)
	//
	// if (m > n b^(l-k)
	//     m = m - n b^(l-k)
	//
	// NOTE: It's temptimg to do the subtraction opportunistically, and if
	// the result is negative, then the condition must have been false.
	// However, we can't do that because under this approach, we must
	// write to the output channel zero digits for digits that turn
	// out to be equal, but if a later digit pair is such that condition
	// is false (p < m), then those rights are invalid, but we have no
	// good way of exluding them from being picked up by the later
	// task. One possiblity is to transmit a flag to that task that
	// tells it whether to include our output channel into its input sync
	// statement. However, this seems less elegant than splitting the
	// normalization into two tasks: the condition and the subtraction.
	//
	// Multiplication by a power of radix (b) is a shift, so when we do the
	// comparison/subtraction of the digits, we offset the index into the
	// product digits by (l-k) = NUM_DIGITS.

	//	d = *TX_READ(GV(reduce));
  TX_WRITE(offset, TX_READ(reduce, unsigned) + 1 - NUM_DIGITS, unsigned, 0);
	LOG("reduce: normalizable: d=%u offset=%u\r\n", TX_READ(reduce, unsigned),
                                                  TX_READ(offset, unsigned));

	for (i = TX_READ(reduce, unsigned); i >= 0; --i) {
    unsigned temp = TX_READ(offset, unsigned);
		LOG("normalizable: m[%u]=%x n[%u]=%x\r\n", i, TX_READ(product[i], digit_t),
            i - temp , TX_READ(modulus[i - temp], digit_t));

		if (TX_READ(product[i], digit_t) > TX_READ(modulus[i - temp], digit_t)) {
			break;
		} else if (TX_READ(product[i], digit_t) < TX_READ(modulus[i - temp], digit_t)) {
			normalizable = false;
			break;
		}
	}

	if (!normalizable && TX_READ(reduce, unsigned) == NUM_DIGITS - 1) {
		LOG("reduce: normalizable: reduction done: message < modulus\r\n");

		// TODO: is this copy avoidable? a 'mult mod done' task doesn't help
		// because we need to ship the data to it.
		curctx->commit_state = TSK_PH1;
    transition_to((task_t *)TX_READ(next_task, uint16_t));
	}

	LOG("normalizable: %u\r\n", normalizable);

	if (normalizable) {
		TX_TRANSITION_TO(task_reduce_normalize);
	} else {
		TX_TRANSITION_TO(task_reduce_n_divisor);
	}
}

// TODO: consider decomposing into subtasks
void task_reduce_normalize()
{
	digit_t m, n, d, s;
	unsigned borrow;

	LOG("normalize\r\n");

	int i;
	// To call the print task, we need to proxy the values we don't touch
	TX_WRITE(print_which, 0, uint8_t, 0);

	borrow = 0;
	unsigned index = TX_READ(offset, unsigned);

	for (i = 0; i < NUM_DIGITS; ++i) {
    m = TX_READ(product[i + index], digit_t);
		n = TX_READ(modulus[i], digit_t);

		s = n + borrow;
		if (m < s) {
			m += 1 << DIGIT_BITS;
			borrow = 1;
		} else {
			borrow = 0;
		}
		d = m - s;

		LOG("normalize: m[%u]=%x n[%u]=%x b=%u d=%x\r\n",
				i + index, m, i, n, borrow, d);

		TX_WRITE(product[i + index], d, digit_t, 0);
	}

	// To call the print task, we need to proxy the values we don't touch

  if (TX_READ(offset, unsigned) > 0) { // l-1 > k-1 (loop bounds), where
                                    //offset=l-k, where l=|m|,k=|n|
    task_t * temp = TASK_REF(task_reduce_n_divisor);
    TX_WRITE(next_task_print, temp, uint16_t, 0);
	} else {
		LOG("reduce: normalize: reduction done: no digits to reduce\r\n");
		// TODO: is this copy avoidable?
		TX_WRITE(next_task_print, TX_READ(next_task, uint16_t), uint16_t, 0);
	}
	TX_TRANSITION_TO(task_print_product);
}

void task_reduce_n_divisor()
{
	LOG("reduce: n divisor\r\n");

	// Divisor, derived from modulus, for refining quotient guess into exact value
	digit_t mod1 = TX_READ(modulus[NUM_DIGITS - 1], digit_t);
  digit_t mod2 = TX_READ(modulus[NUM_DIGITS - 2], digit_t);
  TX_WRITE(n_div, (mod1<< DIGIT_BITS) + mod2, digit_t, 0);

	LOG("reduce: n divisor: n[1]=%x n[0]=%x n_div=%x\r\n", mod1, mod2,
                                                      TX_READ(n_div, digit_t));

	TRANSITION_TO(task_reduce_quotient);
}

void task_reduce_quotient()
{
	uint32_t qn, n_q; // must hold at least 3 digits
  unsigned temp = TX_READ(reduce, unsigned);
	LOG("reduce: quotient: d=%u\r\n", temp);

	// NOTE: we asserted that NUM_DIGITS >= 2, so p[d-2] is safe

	LOG("reduce: quotient: m_n=%x m[d]=%x\r\n",
                                    TX_READ(modulus[NUM_DIGITS - 1], digit_t),
                                            TX_READ(product[temp], digit_t));

	// Choose an initial guess for quotient
	if (TX_READ(product[temp], digit_t) == TX_READ(modulus[NUM_DIGITS - 1], digit_t)) {
		TX_WRITE(quotient, (1 << DIGIT_BITS) - 1, unsigned, 0);
	} else {
		digit_t prod = TX_READ(product[temp], digit_t);
		digit_t prod1 = TX_READ(product[temp - 1], digit_t);
    digit_t mod = TX_READ(modulus[NUM_DIGITS - 1], digit_t);
    TX_WRITE(quotient, ((prod << DIGIT_BITS) + prod1) / mod, unsigned, 0);
	}

	// Refine quotient guess

	// NOTE: An alternative to composing the digits into one variable, is to
	// have a loop that does the comparison digit by digit to implement the
	// condition of the while loop below.
  digit_t prod = TX_READ(product[temp], digit_t);
  digit_t prod1 = TX_READ(product[temp - 1], digit_t);
  digit_t prod2 = TX_READ(product[temp - 2], digit_t);
  n_q = ((uint32_t)prod << (2 * DIGIT_BITS)) + (prod1 << DIGIT_BITS) + prod2;

	LOG("reduce: quotient: m[d]=%x m[d-1]=%x m[d-2]=%x n_q=%x%x\r\n",prod, prod1,
            prod2, (uint16_t)((n_q >> 16) & 0xffff), (uint16_t)(n_q & 0xffff));

	LOG("reduce: quotient: n_div=%x q0=%x\r\n", TX_READ(n_div, digit_t),
                                              TX_READ(quotient, unsigned));

  TX_WRITE(quotient, TX_READ(quotient, unsigned) + 1, unsigned, 0);
	do {
	  TX_WRITE(quotient, TX_READ(quotient, unsigned) - 1, unsigned, 0);
		digit_t temp1 = TX_READ(n_div, digit_t);
    unsigned temp2 = TX_READ(quotient,unsigned);
    qn = mult16(temp1, temp2);
		//qn = GV(n_div) * GV(quotient);
		LOG("QN1 = %x\r\n", (uint16_t)((qn >> 16) & 0xffff));
		LOG("QN0 = %x\r\n", (uint16_t)(qn & 0xffff));
		LOG("reduce: quotient: q=%x qn=%x%x\r\n", TX_READ(quotient, unsigned),
				(uint16_t)((qn >> 16) & 0xffff), (uint16_t)(qn & 0xffff));
	} while (qn > n_q);
	// This is still not the final quotient, it may be off by one,
	// which we determine and fix in the 'compare' and 'add' steps.
	LOG("reduce: quotient: q=%x\r\n", TX_READ(quotient, unsigned));
  TX_WRITE(reduce, TX_READ(reduce, unsigned) - 1, unsigned, 0);

	TX_TRANSITION_TO(task_reduce_multiply);
}

// NOTE: this is multiplication by one digit, hence not re-using mult task
void task_reduce_multiply()
{
	int i;
	digit_t m, n;
	unsigned c, locoffset;

	LOG("reduce: multiply: d=%x q=%x\r\n", TX_READ(reduce, unsigned) + 1,
                                              TX_READ(quotient,unsigned));

	// As part of this task, we also perform the left-shifting of the q*m
	// product by radix^(digit-NUM_DIGITS), where NUM_DIGITS is the number
	// of digits in the modulus. We implement this by fetching the digits
	// of number being reduced at that offset.
	locoffset = TX_READ(reduce, unsigned) + 1 - NUM_DIGITS;
	LOG("reduce: multiply: offset=%u\r\n", locoffset);

	// For calling the print task we need to proxy to it values that
	// we do not modify
	for (i = 0; i < locoffset; ++i) {
		TX_WRITE(product2[i], 0, digit_t, 0);
	}

	// TODO: could convert the loop into a self-edge
	c = 0;
	for (i = locoffset; i < 2 * NUM_DIGITS; ++i) {

		// This condition creates the left-shifted zeros.
		// TODO: consider adding number of digits to go along with the 'product' field,
		// then we would not have to zero out the MSDs
		m = c;
		if (i < locoffset + NUM_DIGITS) {
			n = TX_READ(modulus[i - locoffset], digit_t);
			//MC_IN_CH(ch_modulus, task_init, task_reduce_multiply));
			m += TX_READ(quotient, unsigned) * n;
		} else {
			n = 0;
			// TODO: could break out of the loop  in this case (after TX_WRITE)
		}

		LOG("reduce: multiply: n[%u]=%x q=%x c=%x m[%u]=%x\r\n",
				i - locoffset, n, TX_READ(quotient, unsigned), c, i, m);

		c = m >> DIGIT_BITS;
		m &= DIGIT_MASK;
    TX_WRITE(product2[i], m, digit_t, 0);

	}
	TX_WRITE(print_which, 1, uint8_t, 0);
	task_t *temp = TASK_REF(task_reduce_compare);
  TX_WRITE(next_task_print, temp, uint16_t, 0);
	TX_TRANSITION_TO(task_print_product);
}

void task_reduce_compare()
{
	int i;
	char relation = '=';

	LOG("reduce: compare\r\n");

	// TODO: could transform this loop into a self-edge
	// TODO: this loop might not have to go down to zero, but to NUM_DIGITS
	// TODO: consider adding number of digits to go along with the 'product' field
	for (i = NUM_DIGITS * 2 - 1; i >= 0; --i) {
		LOG("reduce: compare: m[%u]=%x qn[%u]=%x\r\n", i, TX_READ(product[i], digit_t),
                                                   i, TX_READ(product2[i], digit_t));

		if (TX_READ(product[i], digit_t) > TX_READ(product2[i],digit_t)) {
			relation = '>';
			break;
		} else if (TX_READ(product[i], digit_t) < TX_READ(product2[i], digit_t)) {
			relation = '<';
			break;
		}
	}

	LOG("reduce: compare: relation %c\r\n", relation);

	if (relation == '<') {
		TX_TRANSITION_TO(task_reduce_add);
	} else {
		TX_TRANSITION_TO(task_reduce_subtract);
	}
}

// TODO: this addition and subtraction can probably be collapsed
// into one loop that always subtracts the digits, but, conditionally, also
// adds depending on the result from the 'compare' task. For now,
// we keep them separate for clarity.

void task_reduce_add()
{
	int i, j;
	digit_t m, n, c;
	unsigned locoffset;

	// Part of this task is to shift modulus by radix^(digit - NUM_DIGITS)
	locoffset = TX_READ(reduce,digit_t) + 1 - NUM_DIGITS;

	LOG("reduce: add: d=%u offset=%u\r\n", TX_READ(reduce, digit_t) + 1, locoffset);

	// For calling the print task we need to proxy to it values that
	// we do not modify

	// TODO: coult transform this loop into a self-edge
	c = 0;
	for (i = locoffset; i < 2 * NUM_DIGITS; ++i) {
		m = TX_READ(product[i], digit_t);

		// Shifted index of the modulus digit
		j = i - locoffset;

		if (i < locoffset + NUM_DIGITS) {
			n = TX_READ(modulus[j], digit_t);
		} else {
			n = 0;
			j = 0; // a bit ugly, we want 'nan', but ok, since for output only
			// TODO: could break out of the loop in this case (after TX_WRITE)
		}
    TX_WRITE(product[i], c + m + n, digit_t, 0);

		LOG("reduce: add: m[%u]=%x n[%u]=%x c=%x r=%x\r\n", i, m, j, n, c,
                                                TX_READ(product[i], digit_t));

		c = TX_READ(product[i], digit_t) >> DIGIT_BITS;
    TX_WRITE(product[i], TX_READ(product[i], digit_t) & DIGIT_MASK, digit_t, 0);
	}
  TX_WRITE(print_which, 0, uint8_t, 0);
	task_t *temp = TASK_REF(task_reduce_subtract);
  TX_WRITE(next_task_print, temp, uint16_t, 0);
	TX_TRANSITION_TO(task_print_product);
}

// TODO: re-use task_reduce_normalize?
void task_reduce_subtract()
{
	LOG("subtract entered!!");
	int i;
	digit_t m, s, qn;
	unsigned borrow, locoffset;
  // TODO get rid of this
  #if 0
  PRINTF("tsk table:");
  for(int i = 0; i < NUM_BINS; i++) {
    if(!(i & 0x7)) {
      PRINTF("\r\n");
    }
    PRINTF("%u ",tsk_table.bucket_len[i]);
  }
  PRINTF("\r\ntx table:");
  for(int i = 0; i < NUM_BINS; i++) {
    if(!(i & 0x7)) {
      PRINTF("\r\n");
    }
    PRINTF("%u ",tx_table.bucket_len[i]);
  }
  #endif
	// The qn product had been shifted by this locoffset, no need to subtract the zeros
	locoffset = TX_READ(reduce, unsigned) + 1 - NUM_DIGITS;

	LOG("reduce: subtract: d=%u locoffset=%u\r\n", TX_READ(reduce, unsigned) + 1, locoffset);

	// For calling the print task we need to proxy to it values that
	// we do not modify

	// TODO: could transform this loop into a self-edge
	borrow = 0;
  /*for(int i = 0; i < 2 * NUM_DIGITS; i++) {
    PRINTF("%u ", TX_READ(product[i], digit_t));
  }
  PRINTF("\r\n");
  for(int i = 0; i < 2 * NUM_DIGITS; i++) {
    PRINTF("%u ", product[i]);
  }*/
	for (i = 0; i < 2 * NUM_DIGITS; ++i) {
		m = TX_READ(product[i], digit_t);
    //TODO clean out entirely
    //LOG("m = %u i = %u\r\n",m, i);

		// For calling the print task we need to proxy to it values that we do not modify
		if (i >= locoffset) {
			qn = TX_READ(product2[i], digit_t);

			s = qn + borrow;
			if (m < s) {
				m += 1 << DIGIT_BITS;
				borrow = 1;
			} else {
				borrow = 0;
			}
      TX_WRITE(product[i], m - s, digit_t, 0);

			LOG("reduce: subtract: m[%u]=%x qn[%u]=%x b=%u r=%x\r\n",
					i, m, i, qn, borrow, TX_READ(product[i], digit_t));

		}
	}
  TX_WRITE(print_which, 0, uint8_t, 0);

	if (TX_READ(reduce, unsigned) + 1 > NUM_DIGITS) {
    task_t *temp = TASK_REF(task_reduce_quotient);
    TX_WRITE(next_task_print, temp, uint16_t, 0);
	} else { // reduction finished: exit from the reduce hypertask (after print)
		LOG("reduce: subtract: reduction done\r\n");

		// TODO: Is it ok to get the next task directly from call channel?
		//       If not, all we have to do is have reduce task proxy it.
		//       Also, do we need a dedicated epilogue task?
		TX_WRITE(next_task_print, TX_READ(next_task, uint16_t), uint16_t, 0);
	}
	TX_TRANSITION_TO(task_print_product);
}

// TODO: eliminate from control graph when not verbose
void task_print_product()
{
#if 1
	int i;

	LOG("Print: P=");
	for (i = (NUM_DIGITS * 2) - 1; i >= 0; --i) {
		if(TX_READ(print_which, uint8_t)){
			LOG("%x ", TX_READ(product2[i], digit_t));
		}
		else{
			LOG("%x ", TX_READ(product[i], digit_t));
		}
	}
	LOG("\r\n");
#endif
	curctx->commit_state = TSK_PH1;
  transition_to((task_t *)TX_READ(next_task_print, uint16_t));
}

// Should we use a counter other than iter for this? Then we could decouple what
// the interrupt is bringing in and what the processing loop sees?
void event_timer() {
  _numEvents++;
  complete = 0;
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  unsigned cur_iter = EV_READ(iter,unsigned);
  //TODO take this out
  LOG("Ev! %u\r\n", cur_iter);
  while(cur_iter >= INPUT_ARR_SIZE) {
    cur_iter -= INPUT_ARR_SIZE;
  }
  // Copy entry from prewritten array into PLAINTEXT
  for(int i = 0; i < EV_READ(message_length, unsigned); i++) {
    EV_WRITE(PLAINTEXT[i], VAL_ARRAY[cur_iter][i], char, 0);
  }
  // Set ready
  EV_WRITE(ready, 1, unsigned, 0);
  // Writing to plaintext from array of data pulled from ROM.
  //test_ready = EV_READ(ready,unsigned);
  complete = 1;
  EVENT_RETURN();
}

INIT_FUNC(init)
ENTRY_TASK(task_init)
EVENT_ENABLE_FUNC(enable)
EVENT_DISABLE_FUNC(disable)
