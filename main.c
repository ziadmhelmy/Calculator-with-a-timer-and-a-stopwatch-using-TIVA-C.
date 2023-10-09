
#include "tm4c123gh6pm.h"
#include "stdio.h"
#include "stdlib.h"


// Globals

int current_state = 0; // state variable
int current_app = 0;  // app variable
int num1 = 0;         // first number in calculator
int num2 = 0;         // second number in calculator
char op = 'x';        // operation sign
char counting = 0;    // counting variable
char changed = 1;     // changed state in timer variable
char timerdisplay[4] = "0000";  // array for displaying time 
char index = 0;                 // index of array in displaying time
int timerSecs = 0;              
int stopwatchSecs = 0;
char stopwatchcounting = 0;     // stopwatch counting variable

void GPIOF_Handler(void);
void GPIOA_Handler(void);

////////////////////////////////////////////////////////////////////////////////

// Delays Configuration

void delay_milli(int n)
{
 int i , j;
 for(i=0;i<n;i++)
 {
 for(j=0;j<3180;j++)
 {}
 }
}

void delay_micro(int n)
{
 int i , j;
 for(i=0;i<n;i++)
 {
 for(j=0;j<3;j++)
 {}
 }
}

///////////////////////

// Timers and Interrupts Configuration

void Timer0IntHandler(){
  if(TIMER0_MIS_R & 0x1){
    if(stopwatchcounting){
      stopwatchSecs++;
      changed = 1;  
    }
    TIMER0_ICR_R = 0x1;
  }
  

}

void init_Timer0(){
  
  SYSCTL_RCGCTIMER_R |= 0x1;
  TIMER0_CTL_R = 0x0;
  TIMER0_CFG_R = 0x4;
  TIMER0_TAMR_R = 0x02;
  TIMER0_TAPR_R = 249;
  TIMER0_TAILR_R = 0xffff;
  TIMER0_ICR_R = 0x1;  
  TIMER0_IMR_R |= 1;
  TIMER0_CTL_R = 0x1;
  NVIC_EN0_R = 1<<19;
}

void Timer0_delay(){
 
  init_Timer0();
  __asm("WFI\n");
}


void Timer1IntHandler(){
  if(TIMER1_MIS_R & 0x1){
    TIMER1_ICR_R = 0x1;
  }
}

void Timer5IntHandler(){ 
}
void init_Timer5(){
  NVIC_ST_CTRL_R = 0;
  NVIC_ST_CURRENT_R = 0;
  NVIC_ST_RELOAD_R= 0xFFFFFF;//getticks(millisecs);
  //NVIC_ST_CTRL_R = flag;
}
void WideTimer0IntHandler(){
}

// Systick Configuration

void systickHandler(){
  int flag = NVIC_ST_CTRL_R ;
  
    if(counting){
      timerSecs--;
      changed = 1;
      if(timerSecs == 0){
        *(GPIO_PORTF_DATA_BITS_R + 0x2) ^= 0x2;
      }
    }
  
}
int getticks(int millisecs){
  int val = millisecs * (16*1000000) - 1;
  return val;
}

void init_systick(int flag,int millisecs){
  NVIC_ST_CTRL_R = 0;
  NVIC_ST_CURRENT_R = 0;
  NVIC_ST_RELOAD_R= 0xFFFFFF;//getticks(millisecs);
  NVIC_ST_CTRL_R = flag;
}
void reset_systick(){
  NVIC_ST_CTRL_R = 0;
}
void delay_timer(int millisec){

  int systick_flag = (NVIC_ST_CTRL_ENABLE | NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_INTEN);
  init_systick(systick_flag,millisec);
  __asm("WFI\n");
   
}
// Tiva Ports Configuration

void initiGPIOF(){
  SYSCTL_RCGCGPIO_R |= 0x20;
  GPIO_PORTF_DEN_R |= 0xE;
  GPIO_PORTF_DIR_R |= 0xE;
}

//////////////////////////////////


// LCD Configuration

void lcd_data(unsigned char data)
 {
 GPIO_PORTA_DATA_R = 0x20 ;
 GPIO_PORTA_DATA_R |= 0x80; 
 GPIO_PORTB_DATA_R = data;
 delay_micro(10);
 GPIO_PORTA_DATA_R = 0x00;
 delay_micro(100);
}
void lcd_write4bits (unsigned char data , unsigned char control)
{
 data &=0xf0;
 control &=0x0f;
 GPIO_PORTB_DATA_R = data|control;
 GPIO_PORTB_DATA_R = data|control| 4;
 delay_micro(0);
 GPIO_PORTB_DATA_R = data;
}
void lcd_4bits_cmd (unsigned char command)
{
 lcd_write4bits(command & 0xf0 , 0);
 lcd_write4bits(command << 4 , 0);
 if (command < 4)
 delay_milli(2);
 else
 delay_micro(40);
 }
void lcd_4bits_data (unsigned char data) // print 1 char
{
 lcd_write4bits(data & 0xf0 , 1);
 lcd_write4bits(data << 4 , 1);
 delay_micro(40);
}
void lcd_init ()
{
 SYSCTL_RCGCGPIO_R |= 0x2;
 while((SYSCTL_PRGPIO_R &= 0x2)==0) ;
 GPIO_PORTB_DIR_R |= 0xff;
 GPIO_PORTB_DEN_R |=0xff;
 GPIO_PORTB_PCTL_R &=~ 0xffffffff;
 GPIO_PORTB_LOCK_R=0x4C4F434B;
 lcd_4bits_cmd(0x28);
 lcd_4bits_cmd(0x06);
 lcd_4bits_cmd(0x01); // clear
 lcd_4bits_cmd(0x0f);
}
void LCD_String (char *str)
{
 int i;
 for(i=0;str[i]!=0;i++) /* Send each char of string till the NULL */
 {
 if (i==16)
 {
 lcd_4bits_cmd(0xc0); // break
 }
 lcd_4bits_data(str[i]); /* Call LCD data write */
 }
}

void LCD_int(int val){
  char* arr ; 
  arr = (char*)malloc ( 2 * sizeof (char));
  arr[1] = 0;
  if(val != 0){
    arr[0] = (val % 10)+'0';
    LCD_int(val/10);
    LCD_String(arr);
  }
}

/////////////////////////////////////////////////////////////////


// Keypad Configuration 

void keypad_init()
{
 SYSCTL_RCGCGPIO_R |= 0x4;
 while((SYSCTL_PRGPIO_R &= 0x4)==0) ; 
 SYSCTL_RCGCGPIO_R |= 0x10;
 while((SYSCTL_PRGPIO_R &= 0x10)==0) ; 
 GPIO_PORTE_DIR_R |=0xf;
 GPIO_PORTE_DEN_R |=0xf;
 GPIO_PORTE_ODR_R |=0XF;
 GPIO_PORTC_DIR_R &=~ 0xf0;
 GPIO_PORTC_DEN_R |= 0xf0;
 GPIO_PORTC_PUR_R |= 0xf0;
}

unsigned char get_key ()
{
 const unsigned char keypad [4][4]={ {'1','2','3','+'},
  {'4','5','6','-'},
  {'7','8','9','*'},
  {'c','0','#','/'} };
  unsigned int row , col ;
 GPIO_PORTE_DATA_R &=~ 0xf;
 col = GPIO_PORTC_DATA_R & 0xf0;
 if (col == 0xf0)
 return 0 ;
 while(1)
 {
 row=0 ;
 GPIO_PORTE_DATA_R = 0xe;
 delay_micro(10);
 col = GPIO_PORTC_DATA_R &0xf0;
 if(col != 0xf0)
 break;
 row=1 ;
 GPIO_PORTE_DATA_R = 0xd;
 delay_micro(10);
 col = GPIO_PORTC_DATA_R &0xf0;
 if(col != 0xf0)
 break;
 row=2 ;
 GPIO_PORTE_DATA_R = 0xb;
 delay_micro(10);
 col = GPIO_PORTC_DATA_R &0xf0;
 if(col != 0xf0)
 break;
 row=3 ;
 GPIO_PORTE_DATA_R = 0x7;
 delay_micro(10);
 col = GPIO_PORTC_DATA_R &0xf0;
 if(col != 0xf0)
 break;
 return 0;
 }
 if(col == 0xe0)
 return keypad[row][0];
 if(col == 0x0d0)
 return keypad[row][1];
 if(col == 0x0b0)
 return keypad[row][2];
 if(col == 0x070)
 return keypad[row][3];
 return 0 ;
}

/////////////////////////////////////////////////////////////////

// Calculator Handling 

int apply_op(char op){
  
  int val = 0;
  
  if (op == '+'){
    val =  num1 + num2;
  }else if(op == '-'){
    val = num1 - num2;
  }else if(op == '*'){
    val = num1 * num2;
  }else if(op == '/'){
    val = num1 / num2;
  }
  if (val < 0){
    return -1*val;
  }
  return val;
}

char* itoa(int num ){


}

void handleCalculator(char val){
  if(current_state == 1){
    if(val >= '0' && val <= '9' ){
      num1  = num1*10 + (val - '0');
      changed = 1;
    }else if ( (val == '+' || val == '-' || val == '*' || val == '/') && num1 > 0 ){ 
      op = val ;
      num2 = 0;
      current_state = 4;
      changed = 1;
    } 
  }else if(current_state == 4){
    if(val >= '0' && val <= '9' ){
      num2  = num2*10 + (val - '0');
      changed = 1;
    }else if (val == '+' || val == '-' || val == '*' || val == '/' ){
       num1 = apply_op(op);
       num2 = 0;
       op =  val;
       changed = 1;
    }else if (val == '#'){
      current_state = 5;
      num1 = apply_op(op);
      num2 = 0;
      op = 'x';
      changed = 1;
    }
  }else if (current_state == 5){
    if (val == '+' || val == '-' || val == '*' || val == '/' ){
        current_state = 4;
        op =  val;
        changed = 1;
    }
  }
  
}

/////////////////////////////////////////////////////////////////

// Timer Handling 

void timerIntDisplay(int x){
  int i,minutes, minutes1, minutes2, seconds, seconds1, 
  seconds2, maxSeconds;
  int res=0;
  
  seconds = x % 60;
  seconds1 = seconds / 10;
  seconds2 = seconds % 10;
  
  minutes = x / 60;
  minutes1 = minutes / 10;
  minutes2 = minutes % 10;
  
  lcd_4bits_data((char) (minutes1+'0'));
  lcd_4bits_data((char) (minutes2+'0'));
  lcd_4bits_data(':');
  lcd_4bits_data((char) (seconds1+'0'));
  lcd_4bits_data((char) (seconds2+'0'));
  
}

void handleTimerDisplay(char val){
  char j = index;
  while(j > 0){
    timerdisplay[j] = timerdisplay[j-1];
    j -= 1;
  }
  timerdisplay[0] = val;
  if (index < 3){
    index += 1;
  }
}

void initSecs(){

  int minutes1, minutes2, seconds, seconds1, 
  seconds2;
  
  minutes1 = timerdisplay[3] - '0';
  minutes2 = timerdisplay[2] - '0';
  seconds1 = timerdisplay[1] - '0';
  seconds2 = timerdisplay[0] - '0';
  
  timerSecs = (minutes1*10 + minutes2)*60 + seconds1*10 + seconds2;
  
}


void handleTimer(char val){
  
  if(current_state == 2){
    if(val >= '0' && val <= '9' ){
      handleTimerDisplay(val);
      changed = 1;
    }
    else if (val == '#'){
      current_state = 6;
      counting = 1;
      initSecs();
      delay_timer(1000);
    }else if( val == '-'){
      timerdisplay[3] = '0';
      timerdisplay[2] = '0';
      timerdisplay[1] = '0';
      timerdisplay[0] = '0';
      index = 0;
      changed = 1;
    }
  }else if(current_state == 6){
    if(val == '+'){
      counting ^= 0x1;
    }
    else if(val == '-' ){
      timerSecs = 0;
      
      reset_systick();
      
      timerdisplay[3] = '0';
      timerdisplay[2] = '0';
      timerdisplay[1] = '0';
      timerdisplay[0] = '0';
      index = 0;
      current_state = 2;
      changed = 1;
  }
  
  if(timerSecs == 0){
      timerSecs = 0;
      
      reset_systick();
      timerdisplay[3] = '0';
      timerdisplay[2] = '0';
      timerdisplay[1] = '0';
      timerdisplay[0] = '0';
      index = 0;
      current_state = 2;
      changed = 1;
  }
  }
}

/////////////////////////////////////////////////////////////////
 
// Stopwatch Handling

void handleStopWatch(char val){
  
  if(current_state == 3){
    if(val == '#'){
      current_state = 7;
      stopwatchcounting = 1;
      stopwatchSecs = 0;
      Timer0_delay();
    }
  }else if (current_state == 7){
    if(val=='-'){
      current_state = 3;
      stopwatchcounting = 0;
      stopwatchSecs = 0;
      changed = 1;
    }else if(val == '+'){
      stopwatchcounting ^= 1;
    }
  }

}

/////////////////////////////////////////////////////////////////

// Print Handling

void handlePrint(){
  if(current_state == 0){
    if (changed){
      lcd_4bits_cmd(0x01); //clear
      delay_milli(100);
      LCD_String("1.calc 2.ti 3.stp"); //print
      changed = 0;
    }
  }else if(current_state == 1){
    if (changed){
      lcd_4bits_cmd(0x01); //clear
      delay_milli(100);
      if(num1 == 0){
        LCD_String("enter rakm"); //print string
      }else{
        LCD_int(num1); //print integer
      }
      changed = 0;
    }
    
  }else if(current_state == 4){
    if (changed){
      lcd_4bits_cmd(0x01); //clear
      delay_milli(100);
      if(num2 == 0){
        LCD_String("enter rakm"); //print string
      }else{
        LCD_int(num2); //print integer
      }
      changed = 0;
    }
  }else if(current_state == 5){
    if (changed){
      lcd_4bits_cmd(0x01); //clear
      delay_milli(100);
      LCD_int(num1); //print integer
      changed = 0;
    }
  }else if (current_state == 2) {
    if (changed){
      lcd_4bits_cmd(0x01); //clear
      delay_milli(100);

      // printing characters

      lcd_4bits_data(timerdisplay[3]);
      lcd_4bits_data(timerdisplay[2]);
      lcd_4bits_data(':');
      lcd_4bits_data(timerdisplay[1]);
      lcd_4bits_data(timerdisplay[0]);

      ////////////////////////
      
      changed = 0;
      if((GPIO_PORTF_DATA_R & 0x2)!=0){
        delay_milli(1000);
        *(GPIO_PORTF_DATA_BITS_R + 0x2) ^= 0x2;
      }
    }     
  }else if(current_state == 6){
    if(changed){
        lcd_4bits_cmd(0x01); //clear
        timerIntDisplay(timerSecs);
        changed = 0;
    }
  }else if(current_state == 3){
    if(changed){
        lcd_4bits_cmd(0x01); //clear
        LCD_String("00:00"); //print string
        changed = 0;
    }
  }
  else if (current_state == 7){
    if(changed){
        lcd_4bits_cmd(0x01); //clear
        timerIntDisplay(stopwatchSecs);
        changed = 0;
    }
  }

}
///////////////////////////////////////////

// Reseting 

void reset(){
  
  current_app = 0;
  current_state = 0;
  reset_systick();
  changed = 1;
  timerdisplay[3] = '0';
  timerdisplay[2] = '0';
  timerdisplay[1] = '0';
  timerdisplay[0] = '0';
  index = 0;
  num1 = 0;
  num2 =0;
  op = 'x';
  counting = 0;
  changed = 1;

 
  timerSecs = 0;
  stopwatchSecs = 0;
  stopwatchcounting = 0;

}

///////////////////////////////////


// Main


int main()
{
  __asm("CPSIE I");

  lcd_init(); // Intializing LCD
  keypad_init(); // Intializing Keypad
  initiGPIOF();  // Intializing Tiva Ports
  
  
  char val = 0; // input key from keypad
  while (1){
    handlePrint();
    val = get_key();
    if(val == 'c'){
      reset();
    }
    else if(current_app == 0){
      if(val == '1'){
        current_app = 1;
        current_state = 1;
        changed = 1;
      }else if( val == '2'){
        current_app = 2;
        current_state = 2;
        changed = 1;
      }else if( val == '3'){
        current_app = 3;
        current_state = 3;
        changed = 1;
      }
    }
    else if (current_app == 1){
      handleCalculator(val);
    }else if (current_app == 2){
      handleTimer(val);
    }else if(current_app == 3){
      handleStopWatch(val);
    }
    delay_milli(200);
  }
  return 0;
}
