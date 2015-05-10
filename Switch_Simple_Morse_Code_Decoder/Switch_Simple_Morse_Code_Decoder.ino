/*    Modified CW Decoder for Ten Tec Patriot
   Use an ATmeg328P on prototype board inside Patriot
   1) Removed an lcd references
   2) Sent the translated code to the Patriot from Serial port
   3) Received on the Patriot over Serial 1.
   4) Pin 35 is toggled by the patriot to turn on decoding
      Read on Pin 3
   5) Analog read changed to A2 due to proximity A6 on Patriot
   
   Original source code is found http://www.skovholm.com/cwdecoder
   
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address tp 0x27

///////////////////////////////////////////////////////////////////////
// CW Decoder made by Hjalmar Skovholm Hansen OZ1JHM  VER 1.01       //
// Feel free to change, copy or what ever you like but respect       //
// that license is http://www.gnu.org/copyleft/gpl.html              //
// Discuss and give great ideas on                                   //
// https://groups.yahoo.com/neo/groups/oz1jhm/conversations/messages //
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// Read more here http://en.wikipedia.org/wiki/Goertzel_algorithm        //
// if you want to know about FFT the http://www.dspguide.com/pdfbook.htm //
///////////////////////////////////////////////////////////////////////////


int audioInPin = A2;
int audioOutPin = 10;
int ledPin = 13;
int switchPin = 3;
float magnitude ;
int magnitudelimit = 100;
int magnitudelimit_low = 100;
int realstate = LOW;
int realstatebefore = LOW;
int filteredstate = LOW;
int filteredstatebefore = LOW;


///////////////////////////////////////////////////////////
// The sampling frq will be 8928 on a 16 mhz             //
// without any prescaler etc                             //
// because we need the tone in the center of the bins    //
// you can set the tone to 496, 558, 744 or 992          //
// then n the number of samples which give the bandwidth //
// can be (8928 / tone) * 1 or 2 or 3 or 4 etc           //
// init is 8928/558 = 16 *4 = 64 samples                 //
// try to take n = 96 or 128 ;o)                         //
// 48 will give you a bandwidth around 186 hz            //
// 64 will give you a bandwidth around 140 hz            //
// 96 will give you a bandwidth around 94 hz             //
// 128 will give you a bandwidth around 70 hz            //
// BUT remember that high n take a lot of time           //
// so you have to find the compromice - i use 48         //
///////////////////////////////////////////////////////////

float coeff;
float Q1 = 0;
float Q2 = 0;
float sine;
float cosine;  
float sampling_freq=8928.0;
//float target_freq=558.0; /// adjust for your needs see above
float target_freq=700.0; /// adjust for your needs see above
float n=48.0;  //// if you change  her please change next line also 
int testData[48];

//////////////////////////////
// Noise Blanker time which //
// shall be computed so     //
// this is initial          //
//////////////////////////////
int nbtime = 6;  /// ms noise blanker         

long starttimehigh;
long highduration;
long lasthighduration;
long hightimesavg;
long lowtimesavg;
long startttimelow;
long lowduration;
long laststarttime = 0;

char code[20];
int stop = LOW;
int wpm;

////////////////
// init setup //
////////////////
void setup() {

////////////////////////////////////
// The basic goertzel calculation //
////////////////////////////////////
  int	k;
  float	omega;
  k = (int) (0.5 + ((n * target_freq) / sampling_freq));
  omega = (2.0 * PI * k) / n;
  sine = sin(omega);
  cosine = cos(omega);
  coeff = 2.0 * cosine;
 Serial.begin(38400); 
 pinMode(ledPin, OUTPUT);
 pinMode(switchPin, INPUT);
 digitalWrite(switchPin, HIGH);
  lcd.begin(20, 4);                           // 20 chars 4 lines
  lcd.clear();                                // WD9GYM modification adjusted by W8CQD

 
}

///////////////
// main loop //
///////////////
 void loop() {
  int switchState =    digitalRead(switchPin);
  if (switchState == -1) switchState = 1;
 while(switchState == HIGH) {
   switchState =    digitalRead(switchPin);
  ///////////////////////////////////// 
  // The basic where we get the tone //
  /////////////////////////////////////
  
  for (char index = 0; index < n; index++)
  {
    testData[index] = analogRead(audioInPin);
  }
  for (char index = 0; index < n; index++){
	  float Q0;
	  Q0 = coeff * Q1 - Q2 + (float) testData[index];
	  Q2 = Q1;
	  Q1 = Q0;	
  }
  float magnitudeSquared = (Q1*Q1)+(Q2*Q2)-Q1*Q2*coeff;  // we do only need the real part //
  magnitude = sqrt(magnitudeSquared);
  Q2 = 0;
  Q1 = 0;

  
  /////////////////////////////////////////////////////////// 
  // here we will try to set the magnitude limit automatic //
  ///////////////////////////////////////////////////////////
  
  if (magnitude > magnitudelimit_low){
    magnitudelimit = (magnitudelimit +((magnitude - magnitudelimit)/6));  /// moving average filter
  }
 
  if (magnitudelimit < magnitudelimit_low)
	magnitudelimit = magnitudelimit_low;
  
  ////////////////////////////////////
  // now we check for the magnitude //
  ////////////////////////////////////

  if(magnitude > magnitudelimit*0.6) // just to have some space up 
     realstate = HIGH; 
  else
    realstate = LOW; 
  
  ///////////////////////////////////////////////////// 
  // here we clean up the state with a noise blanker //
  /////////////////////////////////////////////////////
 
  if (realstate != realstatebefore){
	laststarttime = millis();
  }
  if ((millis()-laststarttime)> nbtime){
	if (realstate != filteredstate){
		filteredstate = realstate;
	}
  }
 
 ////////////////////////////////////////////////////////////
 // Then we do want to have some durations on high and low //
 ////////////////////////////////////////////////////////////
 
 if (filteredstate != filteredstatebefore){
	if (filteredstate == HIGH){
		starttimehigh = millis();
		lowduration = (millis() - startttimelow);
	}

	if (filteredstate == LOW){
		startttimelow = millis();
		highduration = (millis() - starttimehigh);
        if (highduration < (2*hightimesavg) || hightimesavg == 0){
			hightimesavg = (highduration+hightimesavg+hightimesavg)/3;     // now we know avg dit time ( rolling 3 avg)
		}
		if (highduration > (5*hightimesavg) ){
			hightimesavg = highduration+hightimesavg;     // if speed decrease fast ..
		}
	}
  }

 ///////////////////////////////////////////////////////////////
 // now we will check which kind of baud we have - dit or dah //
 // and what kind of pause we do have 1 - 3 or 7 pause        //
 // we think that hightimeavg = 1 bit                         //
 ///////////////////////////////////////////////////////////////
 
 if (filteredstate != filteredstatebefore){
  stop = LOW;
  if (filteredstate == LOW){  //// we did end a HIGH
   if (highduration < (hightimesavg*2) && highduration > (hightimesavg*0.6)){ /// 0.6 filter out false dits
	strcat(code,".");
//	Serial.print(".");
   }
   if (highduration > (hightimesavg*2) && highduration < (hightimesavg*6)){ 
	strcat(code,"-");
//	Serial.print("-");
	wpm = (wpm + (1200/((highduration)/3)))/2;  //// the most precise we can do ;o)
   }
  }
 
   if (filteredstate == HIGH){  //// we did end a LOW
   
   float lacktime = 1;
   if(wpm > 25)lacktime=1.0; ///  when high speeds we have to have a little more pause before new letter or new word 
   if(wpm > 30)lacktime=1.2;
   if(wpm > 35)lacktime=1.5;
   
   if (lowduration > (hightimesavg*(2*lacktime)) && lowduration < hightimesavg*(5*lacktime)){ // letter space
    docode();
	code[0] = '\0';
//	Serial.print("/");
   }
   if (lowduration >= hightimesavg*(5*lacktime)){ // word space
    docode();
	code[0] = '\0';
	printascii(32);
        Serial.print(" ");
//	Serial.println();
   }
  }
 }
 
 //////////////////////////////
 // write if no more letters //
 //////////////////////////////

  if ((millis() - startttimelow) > (highduration * 6) && stop == LOW){
   docode();
   code[0] = '\0';
   stop = HIGH;
  }

 /////////////////////////////////////
 // we will turn on and off the LED //
 // and the speaker                 //
 /////////////////////////////////////
 
   if(filteredstate == HIGH){ 
     digitalWrite(ledPin, HIGH);
	 tone(audioOutPin,target_freq);
   }
   else{
     digitalWrite(ledPin, LOW);
	 noTone(audioOutPin);
   }
 
 //////////////////////////////////
 // the end of main loop clean up//
 /////////////////////////////////
 updateinfolinelcd();
 realstatebefore = realstate;
 lasthighduration = highduration;
 filteredstatebefore = filteredstate;
 }
 }

////////////////////////////////
// translate cw code to ascii //
////////////////////////////////

void docode(){
    if (strcmp(code,".-") == 0){
        printascii(65);
        Serial.print("A");
    }
    if (strcmp(code,"-...") == 0){
        printascii(66);
        Serial.print("B");
    }
    if (strcmp(code,"-.-.") == 0){
        printascii(67);
        Serial.print("C");
    }
    if (strcmp(code,"-..") == 0) {
        printascii(68);
        Serial.print("D");
    }
    if (strcmp(code,".") == 0) {
        printascii(69);
        Serial.print("E");
    }
    if (strcmp(code,"..-.") == 0) {
        printascii(70);
        Serial.print("F");
    }
    if (strcmp(code,"--.") == 0) {
        printascii(71);
        Serial.print("G");
    }
    if (strcmp(code,"....") == 0) {
        printascii(72);
        Serial.print("H");
    }
    if (strcmp(code,"..") == 0) {
        printascii(73);
        Serial.print("I");
    }
    if (strcmp(code,".---") == 0) {
        printascii(74);
        Serial.print("J");
    }
    if (strcmp(code,"-.-") == 0) {
        printascii(75);
        Serial.print("K");
    }
    if (strcmp(code,".-..") == 0) {
        printascii(76);
        Serial.print("L");
    }
    if (strcmp(code,"--") == 0) {
        printascii(77);
        Serial.print("M");
    }
    if (strcmp(code,"-.") == 0) {
        printascii(78);
        Serial.print("N");
    }
    if (strcmp(code,"---") == 0) {
        printascii(79);
        Serial.print("O");
    }
    if (strcmp(code,".--.") == 0) {
        printascii(80);
        Serial.print("P");
    }
    if (strcmp(code,"--.-") == 0) {
        printascii(81);
        Serial.print("Q");
    }
    if (strcmp(code,".-.") == 0) {
        printascii(82);
        Serial.print("R");
    }
    if (strcmp(code,"...") == 0) {
        printascii(83);
        Serial.print("S");
    }
    if (strcmp(code,"-") == 0) {
        printascii(84);
        Serial.print("T");
    }
    if (strcmp(code,"..-") == 0) {
        printascii(85);
        Serial.print("U");
    }
    if (strcmp(code,"...-") == 0) {
        printascii(86);
        Serial.print("V");
    }
    if (strcmp(code,".--") == 0) {
        printascii(87);
        Serial.print("W");
    }
    if (strcmp(code,"-..-") == 0) {
        printascii(88);
        Serial.print("X");
    }
    if (strcmp(code,"-.--") == 0) {
        printascii(89);
        Serial.print("Y");
    }
    if (strcmp(code,"--..") == 0) {
        printascii(90);
        Serial.print("Z");
    }

    if (strcmp(code,".----") == 0) {
        printascii(49);
        Serial.print("1");
    }
    if (strcmp(code,"..---") == 0) {
        printascii(50);
        Serial.print("2");
    }
    if (strcmp(code,"...--") == 0) {
        printascii(51);
        Serial.print("3");
    }
    if (strcmp(code,"....-") == 0) {
        printascii(52);
        Serial.print("4");
    }
    if (strcmp(code,".....") == 0) {
        printascii(53);
        Serial.print("5");
    }
    if (strcmp(code,"-....") == 0) {
        printascii(54);
        Serial.print("6");
    }
    if (strcmp(code,"--...") == 0) {
        printascii(55);
        Serial.print("7");
    }
    if (strcmp(code,"---..") == 0) {
        printascii(56);
        Serial.print("8");
    }
    if (strcmp(code,"----.") == 0) {
        printascii(57);
        Serial.print("9");
    }
    if (strcmp(code,"-----") == 0) {
        printascii(48);
        Serial.print("0");
    }

    if (strcmp(code,"..--..") == 0) {
        printascii(63);
        Serial.print("?");
    }
    if (strcmp(code,".-.-.-") == 0) {
        printascii(46);
        Serial.print(".");
    }
    if (strcmp(code,"--..--") == 0) {
        printascii(44);
        Serial.print(",");
    }
    if (strcmp(code,"-.-.--") == 0) {
        printascii(33);
        Serial.print("!");
    }
    if (strcmp(code,".--.-.") == 0) {
        printascii(64);
        Serial.print("@");
    }
    if (strcmp(code,"---...") == 0) {
        printascii(58);
        Serial.print(":");
    }
    if (strcmp(code,"-....-") == 0) {
        printascii(45);
        Serial.print("-");
    }
    if (strcmp(code,"-..-.") == 0) {
        printascii(47);
        Serial.print("/");
    }

    if (strcmp(code,"-.--.") == 0) {
        printascii(40);
        Serial.print("(");
    }
    if (strcmp(code,"-.--.-") == 0) {
        printascii(41);
        Serial.print(")");
    }
    if (strcmp(code,".-...") == 0) {
        printascii(95);
        Serial.print("_");
    }
    if (strcmp(code,"...-..-") == 0) {
        printascii(36);
        Serial.print("$");
    }
    if (strcmp(code,"...-.-") == 0) {
        printascii(62);
        Serial.print(">");
    }
    if (strcmp(code,".-.-.") == 0) {
        printascii(60);
        Serial.print("<");
    }
    if (strcmp(code,"...-.") == 0) {
        printascii(126);
        Serial.print("~");
    }
	//////////////////
	// The specials //
	//////////////////
    if (strcmp(code,".-.-") == 0) {
        printascii(3);
        Serial.print("+");
    }
    if (strcmp(code,"---.") == 0) {
        printascii(4);
        Serial.print("*");
    }
    if (strcmp(code,".--.-") == 0) {
        printascii(6);
        Serial.print("r");
    }

}

/////////////////////////////////////
// print the ascii code to the lcd //
// one a time so we can generate   //
// special letters                 //
/////////////////////////////////////
void printascii(int asciinumber){
    lcd.setCursor(0,0);
    lcd.print(asciinumber);
}

void updateinfolinelcd(){
}
