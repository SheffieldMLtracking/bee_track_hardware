#include <SPI.h>
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>

//LED stick:
// STICK        ARDUINO
//pin 1:Vcc -> 5V
//pin 3:SDI -> pin11 (MOSI)
//pin 5:CLK -> pin13
//pin 7:LE  -> pin10
//pin 2:OE  -> 10k resistor to ground
//pin 4:LEDV-> (forward voltage = 3.2V * 4 = 12.8V!)
//pin 6:GND -> GND

//GPS           Arduino
//pinTX     -> pin3
//pinRX     -> pin4
//pinGND    -> pinGND
//pinVcc    -> 5V
//pinPPS    -> pin2 (Interrupt)

SoftwareSerial mySerial(3,4);
Adafruit_GPS GPS(&mySerial);
int timer1_counter;
void setup() {
  //GPS Setup
  Serial.begin(115200);
  delay(1000);
  Serial.println("Adafruit GPS library basic test!");
  GPS.begin(9600); // 9600 NMEA is the default baud rate for many GPS units.
  Serial.println("configuring GPS");
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);//remove
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
  Serial.println("configuring GPS...");
  GPS.sendCommand(PGCMD_ANTENNA);  // Request updates on antenna status, comment out to keep quiet
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);//add
  Serial.println("configuring GPS complete");
  
  delay(1000);
  Serial.println("configuring GPS");
  mySerial.println(PMTK_Q_RELEASE);
  Serial.println("launching...");
  //LED Setup
  pinMode(10, OUTPUT); //LE control
  Serial.println("pin mode set");
  Serial.println("Starting SPI...");
  SPI.begin();
  SPI.beginTransaction(SPISettings(10000, MSBFIRST, SPI_MODE0)); 
  Serial.println("SPI started");
  //Blink the LEDs to show they're working.
  unsigned int j = 1;
  for (int k=0;k<5;k=k+1) {
    for (int i=0;i<7;i=i+1) { 
      Serial.println(j);
      SPI.transfer(j % 256);
      //delayMicroseconds(2500);
      //SPI.transfer(j / 256);
      delayMicroseconds(2500);
      digitalWrite(10,HIGH);
      delayMicroseconds(2500); 
      digitalWrite(10,LOW); 
      delay(50);
      if (k%2==0) {j = j * 2;} else {j = j / 2;}
    }
  }

  //Interrupt Config
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;

  //learning flight top speed 3~4m/s, accuracy needs to be to about 3-5cm, it takes 7.5ms-16ms to go that far. To be safe, let's tick every 4ms.
  // Set timer1_counter to the correct value for our interrupt interval
  //prescalar is 8, 16MHz -> 2MHz tick 2MHz/4000 = 500Hz. (2ms)
  //prescalar is 8, 16MHz -> 2MHz tick 2MHz/8000 = 250Hz. (4ms)
  timer1_counter = 8000; // this is what we need to subtract from the counter each time it goes through zero...
  
  TCNT1 = 65535-timer1_counter;   // preload timer
  TCCR1B |= (1 << CS11);    // CS12 -> 256 prescaler, CS11 -> 8 prescaler. CS10 -> 1 prescaler
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  interrupts();             // enable all interrupts

  attachInterrupt(0, gps_trig, RISING);
}

int temp = 0;
int res = 0;
unsigned int i = 0; //for LED milliseconds
unsigned int i_sec = 0; //for LED seconds
uint32_t timer = millis(); 
int change = 0;
int newchange = 0;
int good = 0;
bool LEDs_enabled = false;

void gps_trig() {
  TCNT1 = 65535-timer1_counter;
  Serial.println("!!!!");

  if (!LEDs_enabled) {
    //unsigned int newtime = (GPS.hour % 9)*3600+GPS.minute*60+GPS.seconds + 1;
    unsigned int newtime = (GPS.minute % 4)*60+GPS.seconds + 1;
  
    //we want to see a few sequential times in a row to test this data is correct.
    if (newtime == i_sec + 1) {
      good = good + 1;
      Serial.println(">>>");
      Serial.println(good);
      if (good>5) {
        Serial.println("five valid times in a row, shutting down GPS reading and starting LED");
        mySerial.end();
        LEDs_enabled = true;
      }
    }
    else
    {
      Serial.println("good=0");
      good = 0;
    }
    i_sec = newtime;
  }
  else
  {
    if (i>125) //250) //if it is nearer the end of the second than the start then we should increment i_sec as well as set i = 0.
    {
      i_sec = i_sec + 1;
      if (i_sec>=240) {i_sec=0;}
    }
  
  }
  
  Serial.println("...");
  Serial.println(i);
  Serial.println(i_sec);
  Serial.println("...");
  i = 0;
  res = i;
}

// function to convert binary integer to grey integer
unsigned int binarytoGray(unsigned int binary)
{
    unsigned int grey;
 
    // MSB of gray code is same as binary code
    grey = binary & 0x80;
 
    // Compute remaining bits, next bit is computed by
    // doing XOR of previous and current in Binary
    unsigned int lastbit = (binary & 0x80) >> 1;
    for (int s = 0x40; s > 0; s=s >> 1) {
      grey = grey + ((binary ^ lastbit) & s);
      lastbit = (binary & s) >> 1;

        // Concatenate XOR of previous bit
        // with current bit
    //    gray += xor_c(binary[i - 1], binary[i]);
    }
 
    return grey;
}

ISR(TIMER1_OVF_vect)        // interrupt service routine 
{
  //temp = TCNT1;
  TCNT1 = TCNT1 - timer1_counter;   // preload timer
  //TCNT1 = (65535 - timer1_counter) + temp;   // preload timer
  //SPI.transfer(i_sec % 256);
  //SPI.transfer(i_sec);
  //digitalWrite(10,HIGH);
  //digitalWrite(10,LOW);
  if (LEDs_enabled) {
    //SPI.transfer(1+4+16+64);
    //SPI.transfer(1+2+4+8);
    Serial.println("i");
    Serial.println(i);
    Serial.println("i_sec");
    Serial.println(i_sec);
    Serial.println("Sending...");

    unsigned int send;
    if (i_sec%2==1) {
      send = i_sec%128+(i/128)*128;
    }
    else
    { 
      send = (i%128)*2;
    }

    Serial.println(send);
    Serial.println("TO GREY");
    Serial.println(binarytoGray(send));
    SPI.transfer(send);


    delayMicroseconds(500);
    digitalWrite(10,HIGH);
    delayMicroseconds(500); 
    digitalWrite(10,LOW); 
  }
  //SPI.transfer((i/256)+(i_sec%64));
  //digitalWrite(10,HIGH);
  //digitalWrite(10,LOW);
  //if (temp>2000) {Serial.println("UHOH");Serial.println(temp);}
  i = i + 1;
  if (i>=250) { //500
    i=0;
    if (LEDs_enabled) {
      i_sec = i_sec + 1; //we have to increment i_sec ourselves.
      if (i_sec>=240) {i_sec=0;}
    }
  }
}

void loop() {    
  char c = GPS.read();
  if (GPS.newNMEAreceived()) {
    Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
    
  }
  /*if (millis() - timer > 200) {
    if (res!=-1) { Serial.println(res); }
    res = -1;
    timer = millis(); 
    
  }*/
}
