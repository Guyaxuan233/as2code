/*
 * microwaveAssessment.c
 * Author : <Your Name + Student ID>
 */ 

#include <stdbool.h>
#include <stdint.h>
#include <avr/interrupt.h>

/* Internal Library Includes */
//#include "serialio.h"
//#include "terminalio.h"
//#include "ledmatrix.h"

#define F_CPU 8000000UL 
#include <util/delay.h>

// ENUMs / 'enumerations' basically give names to integer constants; by default values start at 0 and auto-increment.
// This lets you use readable labels (like MODE_QUICK, MODE_POPCORN) instead of magic numbers (0,1,2,...), though under the hood they are still integers
// Cooking mode enumeration.
typedef enum {
	MODE_QUICK,
	MODE_POPCORN,
	MODE_BEVERAGE,
	MODE_DEFROST
} Mode;

// Power level enumeration.
typedef enum {
	POWER_10,
	POWER_50,
	POWER_100
} PowerLevel;

void led4_light(){
	PORTD |= (1<<PIND2);
}

void led4_dark(){
	PORTD &= (~(1<<PIND2));
}

static volatile uint8_t led4_should_light;

void led4_work(){
	if (led4_should_light == 1)
	{
		led4_light();
	}else if (led4_should_light == 0)
	{
		led4_dark();
	}
}

static uint8_t fb[16];
static const uint8_t font5x8_digit_0[5] = {0x3C,0x46,0x4A,0x52,0x3C};
static const uint8_t font5x8_digit_1[5] = {0x00,0x22,0x7E,0x02,0x00};
static const uint8_t font5x8_digit_2[5] = {0x26,0x46,0x4A,0x4A,0x32};
static const uint8_t font5x8_digit_3[5] = {0x44,0x52,0x52,0x52,0x2C};
static const uint8_t font5x8_digit_4[5] = {0x0C,0x14,0x24,0x7E,0x04};
static const uint8_t font5x8_digit_5[5] = {0x74,0x52,0x52,0x52,0x4C};
static const uint8_t font5x8_digit_6[5] = {0x3C,0x52,0x52,0x52,0x0C};
static const uint8_t font5x8_digit_7[5] = {0x40,0x46,0x48,0x50,0x60};
static const uint8_t font5x8_digit_8[5] = {0x2C,0x52,0x52,0x52,0x2C};
static const uint8_t font5x8_digit_9[5] = {0x30,0x4A,0x4A,0x4A,0x3C};

static const uint8_t font5x8_letter_b[5] = {0x7E,0x12,0x12,0x12,0x0C};
static const uint8_t font5x8_letter_d[5] = {0x0C,0x12,0x12,0x12,0x7E};
static const uint8_t font5x8_letter_p[5] = {0x7E,0x48,0x48,0x48,0x30};
static const uint8_t font5x8_letter_q[5] = {0x30,0x48,0x48,0x48,0x7E};

	
static inline const uint8_t* font5x8_get_digit_u8(uint8_t time_remaining) {
	switch (time_remaining) {
		case 0: return font5x8_digit_0;
		case 1: return font5x8_digit_1;
		case 2: return font5x8_digit_2;
		case 3: return font5x8_digit_3;
		case 4: return font5x8_digit_4;
		case 5: return font5x8_digit_5;
		case 6: return font5x8_digit_6;
		case 7: return font5x8_digit_7;
		case 8: return font5x8_digit_8;
		case 9: return font5x8_digit_9;
		default: return 0;  /* 防御：不支持的数字 */
	}
}

static inline const uint8_t* font5x8_get_modeindex_u8(uint8_t index_mi) {
	switch (index_mi) {
		case 0: return font5x8_letter_q;  /* MODE_QUICK */
		case 1: return font5x8_letter_p;  /* MODE_POPCORN */
		case 2: return font5x8_letter_b;  /* MODE_BEVERAGE */
		case 3: return font5x8_letter_d;  /* MODE_DEFROST */
		default: return 0;             /* 防御：不支持的模式索引 */
	}
}

static inline void fb_clear(void){
	for(uint8_t x=0; x<16; x++) fb[x] = 0x00;
}


static inline void fb_draw_glyph5x8(uint8_t x0, const uint8_t *g){
	if(!g) return;
	for(uint8_t i=0;i<5;i++){
		uint8_t xi = (uint8_t)(x0 + i);
		if(xi >= 16) break;
		fb[xi] = g[i];                 /* 直接覆盖写：1bit/像素 */
	}
}

extern uint8_t spi_send_byte(uint8_t b);

static inline void ledmx_flush(void){
	PORTB &= ~(1<<PINB4);		//拉低ss输送数据
	spi_send_byte(0x00);            // 照课堂例子：先发一个帧起始/命令字节

	for(uint8_t y=0; y<8; y++){     // 8 行
		for(uint8_t x=0; x<16; x++){// 16 列 → 8*16=128 字节
			uint8_t on = (fb[x] >> y) & 0x01;   // 取 (x,y) 像素
			spi_send_byte(on ? 0x0F : 0x00);    // 1bit 展开为 1 字节亮度
		}
	}
	PORTB |= (1<<PINB4);      //拉高ss
}


void ledmx_render_and_flush_indices(uint8_t index_mi, uint8_t time_remaining){
	fb_clear();
	fb_draw_glyph5x8(2,  font5x8_get_modeindex_u8(index_mi));  /* 左字符：q/p/b/d */
	fb_draw_glyph5x8(9, font5x8_get_digit_u8(time_remaining));/* 右字符：0..9   */
	ledmx_flush();
}

static volatile uint8_t id_printed = 0;
static const char student_id[] = "49843895\r\n";

// GLOBAL FLAGS - these can be accessed from anywhere in the file, making them useful for storing global state information: 
// These global variables are here to help you track the state of your microwave, feel free to add more as you see fit.
Mode mode;   // Quick, Popcorn, Beverage or Defrost mode.
PowerLevel power_level; // 10% / 50% / 100%.
bool is_running; // Tracks whether the microwave is counting down.
bool is_paused;

void b0_debounce_1ms(void);
void b1_debounce_1ms(void);

volatile uint8_t power_cont = 0;
uint8_t p_0 = 0b100;
uint8_t p_1 = 0b110;
uint8_t p_2 = 0b111;

volatile uint16_t howmany_ms_forbuzz = 0;
volatile uint8_t  reach_250ms = 0;

// ========== JINGLE ????????? ==========
// ????????: ????????????????
typedef struct {
    uint16_t ocr_value;  // Timer1??OCR1A?(???????????)
    uint16_t duration;   // ???????(????)
} Note;

// Jingle????: ????3????????????
// ??????????Щ????????????????
const Note jingle_melody[] = {
    {220, 250},  // ????1: ?????, 250ms
    {180, 250},  // ????2: ????, 250ms  
    {150, 250}   // ????3: ?????, 250ms
};

#define JINGLE_LENGTH 3 // jingle??4??????

volatile uint8_t jingle_playing = 0;      // ??????????jingle
volatile uint8_t jingle_note_index = 0;   // ?????????????????
volatile uint16_t jingle_note_timer = 0;  // ????????????????(ms)
// ========== JINGLE ???????? ==========

ISR(INT2_vect){								//?????ж?B2??????isr????????B2?????????仯??????????????????????????????power_cont++
	power_cont++;
	
	// ???jingle?????????????
	if(jingle_playing) {
		jingle_playing = 0;
		jingle_note_index = 0;
		jingle_note_timer = 0;
	}
	
	TCCR1A |= (1<<COM1B1);
	OCR1A = 195;
	OCR1B = 124;
	reach_250ms = 0;
	howmany_ms_forbuzz = 0;
}

static volatile uint8_t cc;    //?????cc??????????????β????????θ?isr
volatile uint16_t howmany_ms = 0;   // ????????????   ??????uint16_t?????п????interrupt???? ?????ù??????ж?
volatile uint8_t  reach_1s = 0;   // "?? 1 ????"??С???????

uint8_t index_mi = 0;
uint8_t s0_s1;
uint8_t time_remaining = 5;
uint8_t cooking_time[10] = { 63,6,91,79,102,109,125,7,127,111 };
uint8_t init_time[4] = { 5,7,3,9 };
uint8_t mode_identify[4] = { 103,115,124,94 };

volatile uint8_t led_dirty = 1;

void display_digit(uint8_t time_remaining, uint8_t index_mi, uint8_t cc);

volatile uint8_t howmany_ms_forled4;

ISR(TIMER2_COMPA_vect){
	cc = !cc;
	display_digit(time_remaining,index_mi,cc);									//此处是对led矩阵对ssd在idle状态下闪烁的debugging
	b0_debounce_1ms();				//???B0??B1???????????????????
	b1_debounce_1ms();
	howmany_ms++;
	howmany_ms_forbuzz++;
	if(led4_should_light == 1){
		howmany_ms_forled4++;
	}
	
	// ========== JINGLE ??????? ==========
	if(jingle_playing) {
		jingle_note_timer++;
		
		// ????????????????
		if(jingle_note_timer >= jingle_melody[jingle_note_index].duration) {
			jingle_note_index++;      // ??????????????
			jingle_note_timer = 0;    // ???????????????
			
			// ????????????????????????
			if(jingle_note_index >= JINGLE_LENGTH) {
				// Jingle???????
				jingle_playing = 0;
				jingle_note_index = 0;
				TCCR1A &= ~(1<<COM1B1);  // ???buzzer???
			} else {
				// ????????????????????????????????
				OCR1A = jingle_melody[jingle_note_index].ocr_value;
			}
		}
	}
	// ========== JINGLE ??????? ==========
	
	while (howmany_ms>=1000){
		reach_1s++;							//???????????1??
		howmany_ms = 0;
	}										// ??1000ms??1??											
	while (howmany_ms_forbuzz>=250){
		reach_250ms++;						//??250ms???????1??
		howmany_ms_forbuzz = 0;
	}
	while(howmany_ms_forled4 == 100){
		led4_should_light = 0;
		howmany_ms_forled4 = 0;
	}
}

void spi_setup_master(void) {
	// Set up SPI communication as a master
	// Make the SS, MOSI and SCK pins outputs. These are pins
	// 4, 5 and 7 of port B on the ATmega324A
	DDRB = 0b10110000;/* <YOUR CODE HERE> */
	
	// Set the slave select (SS) line high
	PORTB |= (1<<PINB4);
	
	// Set up the SPI control registers SPCR0 and SPSR0
	// - SPE0 bit = 1 (SPI is enabled)
	// - MSTR0 bit = 1 (Master Mode)
	SPCR0 = (1<<SPE0)|(1<<MSTR0);/* <YOUR CODE HERE> */
	
	// Set SPR00 and SPR01 bits in SPCR0 and SPI2X0 bit in SPSR0
	// based on a clock divider of 128
	SPSR0 &= ~(1<<SPI2X0);/* <YOUR CODE HERE> */
	SPCR0 |= (1<<SPR00)|(1<<SPR10); /* <YOUR CODE HERE> */
	
	// Take SS (slave select) line low
	//PORTB &= ~(1<<PINB4);
}

uint8_t spi_send_byte(uint8_t byte) {
	// Write out the byte to the SPDR0 register. This will initiate
	// the transfer. We then wait until the most significant byte of
	// SPSR0 (SPIF0 bit) is set - this indicates that the transfer is
	// complete. (The final read of SPSR0 followed by a read of SPDR0
	// will cause the SPIF0 bit to be reset to 0. See page 173 of the
	// ATmega324A datasheet (2018 version).)
	SPDR0 = byte;
	while((SPSR0 & (1<<SPIF0)) == 0) {
		; // wait
	}
	return SPDR0;
}

void set_USART(void) {
	UBRR0 = 25;                   // 8MHz / (16*19200) - 1 ≈ 25
	UCSR0A = 0;                    // U2X0=0 正常速率
	UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<RXCIE0);  // 使能收发+接收中断
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);              // 8位数据, 1位停止, 无校验
}

void transmit_onebyte(char a){
	while(!(UCSR0A & (1<<UDRE0)));       // 等发送缓冲区空
	UDR0 = a;                             // 写入即开始发送
}

void transmit_chars(const char *s){
	while(*s) transmit_onebyte(*s++);          // 逐字符发送到遇到'\0'
}



void initialise_hardware(void) {
	DDRA = 0xFF;			
	DDRC |= (1<<DDRC7)|(1<<DDRC3)|(1<<DDRC2)|(1<<DDRC1)|(1<<DDRC0);	
	DDRD |= (1<<DDRD7)|(1<<DDRD6)|(1<<DDRD5)|(1<<DDRD4)|(1<<DDRD2);
	_delay_ms(100);					//?????delay???????????????????????????ssd??led??????
	TCCR2A = (1<<WGM21);
	TCCR2B = (1<<CS22);
	TIMSK2 = (1<<OCIE2A);
	OCR2A = 124;
	//sei();
	EICRA |= (1<<ISC21);
	EIFR |= (1<<INTF2);
	EIMSK |= (1<<INT2);
	TCCR1A = (1<<WGM11)|(1<<WGM10);											//????timer1??????????????????OC1B????????
	TCCR1B = (1<<CS11)|(1<<CS10)|(1<<WGM12)|(1<<WGM13);
	OCR1A = 255;
	OCR1B = 124;
	spi_setup_master();
	set_USART();
} 

void power_led_light(PowerLevel power_level){
	if (power_level==POWER_10){
		PORTD = (PORTD & ~((1<<PIND5)|(1<<PIND6)|(1<<PIND7)))  | ((p_0 & 0x07) << PIND5);
	}
	else if (power_level==POWER_50){
		PORTD = (PORTD & ~((1<<PIND5)|(1<<PIND6)|(1<<PIND7)))  | ((p_1 & 0x07) << PIND5);
	}
	else{
		PORTD = (PORTD & ~((1<<PIND5)|(1<<PIND6)|(1<<PIND7)))  | ((p_2 & 0x07) << PIND5);
	}
}

void show_power(){
	uint8_t power_mode = power_cont%3;
	power_level = power_mode;
	power_led_light(power_level);
}

	
void mode_selection(void){
	s0_s1 = ((PINC>>5)&0x03);											//???????????pin????
	uint8_t mask_code = 1<<(s0_s1);
	PORTC = (PORTC&0xF0)|mask_code;										//??????λ??s0??s1????????λ???????????????????????????
	index_mi = s0_s1;
	time_remaining = init_time[s0_s1];
	led_dirty = 1;
}

void display_digit(uint8_t time_remaining, uint8_t index_mi, uint8_t cc)
{
	PORTC = (0x7F&PINC)|((0x01&cc)<<7);         //??λ?cc?????c?????
	if (cc==0){									//??cc=0????????????cc=1???????????
		PORTA = cooking_time[time_remaining];	
	} 
	else{
		PORTA = mode_identify[index_mi];
	}
}

volatile uint8_t b0_released=0;
volatile uint8_t b1_released=0;

static inline uint8_t read_b0(void){
	return ((PINB&0x01)?1:0);	
}

void b0_debounce_1ms(void){
	static uint8_t candidate_0 = 0;
	static uint8_t reported_0  = 0;
	static uint8_t cnt_0 = 0;

	uint8_t now = read_b0();

	if(now == candidate_0){ if(cnt_0 < 255) cnt_0++; }
	else { cnt_0 = 0; candidate_0 = now; }

	if(cnt_0 >= 25){
		if(reported_0 == 1 && candidate_0 == 0){
			if(b0_released < 255) b0_released++;
		}
		reported_0 = candidate_0;
	}
}				

static inline uint8_t read_b1(void){
	return (PINB & (1<<PINB1)) ? 1 : 0;
}

void b1_debounce_1ms(void){
	static uint8_t candidate_1 = 0;
	static uint8_t reported_1  = 0;
	static uint8_t cnt_1 = 0;

	uint8_t now = read_b1();

	if(now == candidate_1){ if(cnt_1 < 255) cnt_1++; }
	else { cnt_1 = 0; candidate_1 = now; }

	if(cnt_1 >= 25){
		if(reported_1 == 1 && candidate_1 == 0){
			if(b1_released < 255) b1_released++;
		}
		reported_1 = candidate_1;
	}
}

// ========== ????JINGLE????? ==========
void start_jingle(void) {
	// ????κ?????????beep
	reach_250ms = 0;
	howmany_ms_forbuzz = 0;
	
	// ?????jingle??
	jingle_playing = 1;
	jingle_note_index = 0;
	jingle_note_timer = 0;
	
	// ????buzzer??????????????
	TCCR1A |= (1<<COM1B1);
	OCR1A = jingle_melody[0].ocr_value;
	OCR1B = 124;
}
// ========== JINGLE ???????? ==========


static uint8_t last_s0_s1;
uint8_t now_s0_s1;
volatile uint8_t jingle_played = 0;  // ??????????????jingle


void run_microwave() {
	if (!is_running && !is_paused) {
		show_power();
		mode_selection();
		last_s0_s1 = ((PINC>>5)&0x03);
		//display_digit(time_remaining,index_mi,cc);					//??????????????????
		jingle_played = 0;  // ????jingle??????
		
		// ??????????jingle?????beep
		if(reach_250ms && !jingle_playing){
			reach_250ms = 0;
			howmany_ms_forbuzz =0;
			TCCR1A = TCCR1A&0b11011111;
		}
		
		while (b0_released){
			b0_released = 0;
			
			// ???jingle?????????????
			if(jingle_playing) {
				jingle_playing = 0;
				jingle_note_index = 0;
				jingle_note_timer = 0;
			}
			
			TCCR1A |= (1<<COM1B1);
			OCR1A = 255;
			OCR1B = 124;
			cli();												//?????uint16_t???п????interrupt???
			reach_250ms = 0;
			howmany_ms_forbuzz = 0;								
			reach_1s   = 0;										// ????????? 1?????
			howmany_ms = 0;									    
			sei();
			is_running = true;									
			is_paused = false;
		}
	}
	else if (is_running) {
		show_power();
		//display_digit(time_remaining,index_mi,cc);
		
		if(reach_250ms && !jingle_playing){
			reach_250ms = 0;
			howmany_ms_forbuzz =0;
			TCCR1A = TCCR1A&0b11011111;
		}
		
		if (reach_1s) {
			reach_1s = 0;  
			if (time_remaining > 0) {
				time_remaining--;
				led_dirty = 1;		
			} else {
				if(!jingle_played && !jingle_playing) {
					start_jingle();
					jingle_played = 1;  // ????????
				}
				// ========== JINGLE ???????? ==========
				
				now_s0_s1 = ((PINC>>5)&0x03);
				if (now_s0_s1 != last_s0_s1){
					last_s0_s1 = now_s0_s1;
					is_running = false;
					// ???????л????????jingle
					if(jingle_playing) {
						jingle_playing = 0;
						jingle_note_index = 0;
						jingle_note_timer = 0;
						TCCR1A &= ~(1<<COM1B1);
					}
				}
			}
		}
		
		while(b0_released){
			show_power();
			b0_released = 0;
			
			// ???jingle?????????????
			if(jingle_playing) {
				jingle_playing = 0;
				jingle_note_index = 0;
				jingle_note_timer = 0;
			}
			
			howmany_ms = 0;
			TCCR1A |= (1<<COM1B1);
			OCR1A = 255;
			OCR1B = 124;
			howmany_ms_forbuzz = 0;
			reach_250ms = 0;
			
			if (time_remaining < 9) {
				time_remaining = time_remaining + 1;
				led_dirty = 1;
			} else {
				time_remaining = 9;
				led_dirty = 1;
			}
		}  
		
		while (b1_released) {
			show_power();
			b1_released = 0;
			
			// ???jingle?????????????
			if(jingle_playing) {
				jingle_playing = 0;
				jingle_note_index = 0;
				jingle_note_timer = 0;
			}
			
			howmany_ms = 0;
			TCCR1A |= (1<<COM1B1);
			OCR1A = 155;
			OCR1B = 124;
			howmany_ms_forbuzz = 0;
			reach_250ms = 0;
			if(time_remaining == 0){
				is_running = false;
				is_paused = false;
			}
			else{
				is_paused = true;
				is_running = false;
			}
		}
	}
	else if (is_paused) {
		show_power();
		//display_digit(time_remaining,index_mi,cc);
		
		if(reach_250ms && !jingle_playing){
			reach_250ms = 0;
			howmany_ms_forbuzz =0;
			TCCR1A = TCCR1A&0b11011111;
		}
		
		while(b1_released){
			show_power();
			b1_released = 0;
			
			// ???jingle?????????????
			if(jingle_playing) {
				jingle_playing = 0;
				jingle_note_index = 0;
				jingle_note_timer = 0;
			}
			
			howmany_ms = 0;
			TCCR1A |= (1<<COM1B1);
			OCR1A = 155;
			OCR1B = 124;
			howmany_ms_forbuzz = 0;
			reach_250ms = 0;
			is_paused = false;
			is_running = false;
		}
		
		while(b0_released){
			show_power();
			b0_released = 0;
			
			// ???jingle?????????????
			if(jingle_playing) {
				jingle_playing = 0;
				jingle_note_index = 0;
				jingle_note_timer = 0;
			}
			
			howmany_ms = 0;
			TCCR1A |= (1<<COM1B1);
			OCR1A = 255;
			OCR1B = 124;
			howmany_ms_forbuzz = 0;
			reach_250ms = 0;
			is_paused = false;
			is_running = true;
		}
	} 
}

ISR(USART0_RX_vect) {
	uint8_t c = UDR0;                  // 读走数据，清 RXC 标志

	if (!id_printed && (c == '\r' || c == '\n')) {
		id_printed = 1;
		transmit_chars(student_id);    // 打印你的学号
		// 也可以在这里短亮 L4 表示收到命令
		// led_pulse_100ms();
		return;
	}
	if (c == 'p') {
		// cycle power level -> 等效于 B2
		power_cont++;
		show_power();
		// led_pulse_100ms();
		TCCR1A |= (1<<COM1B1);   // 开 OC1B
		OCR1A = 195;             // 音高（和 INT2 一致）
		OCR1B = 124;             // 占空比
		cli();                   // 防止与 1ms 中断并发
		reach_250ms = 0;
		howmany_ms_forbuzz = 0;  // 重新计 250ms
		sei();
		led4_should_light = 1;
		} else if (c == 'r') {
		// stop/reset timer -> 等效于 B1
		b1_released++;                 // 或直接调用你已有的处理逻辑
		TCCR1A |= (1<<COM1B1);
		OCR1A = 155;             // B1 的音高
		OCR1B = 124;
		cli();
		reach_250ms = 0;
		howmany_ms_forbuzz = 0;
		sei();
		// led_pulse_100ms();
		led4_should_light = 1;

		} else if (c == 's') {
		// start/resume -> 等效于 B0
		b0_released++;
		TCCR1A |= (1<<COM1B1);
		OCR1A = 255;             // B0 的音高
		OCR1B = 124;
		cli();
		reach_250ms = 0;
		howmany_ms_forbuzz = 0;
		sei();
		// led_pulse_100ms();
		led4_should_light = 1;

	}
}

int main(void)
{
	initialise_hardware();			
	is_running = false;				
	is_paused = false;
	static const char id[] = "49843895\r\n";
	transmit_chars(id);
	sei();   //注意修改回来（？

	while (true) {
		run_microwave();       
		if (led_dirty) {
			led_dirty = 0;  /* 先清标志，避免重复刷新 */
			ledmx_render_and_flush_indices(index_mi, time_remaining);
		}
		led4_work();
	}
}
