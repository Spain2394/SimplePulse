#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);


#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16

#define Sampling_Time 5
#define Num_Samples 300
#define Peak_Threshold_Factor 75
#define Minimum_Range 50
#define Minimum_Peak_Separation 50  // 50*5=250 ms
#define DC_Added 10
#define Samples_to_Display 600
#define Display_Sampling 2
//This example is for a 128x32 size display using I2C to communicate
#define x_axis_length 128
#define y_axis_length 30
#define Moving_Average_Num 5


/* Project : IoT Heart Rate Monitor
   Written by: Raj Bhatt (02/25/2017)

   Modified by Allen Spain (03/16/2018)

*/

unsigned long startTime= 0;
unsigned long currentTime = 0;

const int analogInPin = A1;  // Analog input pin that the potentiometer is attached to
int sensorValue = 0;

int ADC_Samples[Num_Samples], Index1, Index2, Index3, Peak1, Peak2, Peak3;
long Pulse_Rate, Temp1 = 1L, PR1, PR2;
int Peak_Magnitude, Peak_Threshold, Minima, Range;

void setup() {
  
  char  ch;
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  Serial.begin(9600);

  // Clear the buffer.
  display.clearDisplay();
  delay(500);
 
  // Inittial phrase
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("Running... ");
  display.display();
  display.clearDisplay();
  delay(50);

}  // End setup loop

// This is the main method where data gets pushed to the Google sheet
void loop() {

  
  //  currentTime = (int)(millis()/1000);
  Read_ADC_Samples(); 
  Serial.println("Sample Read Finished ");
  Remove_DC();
  Serial.println("DC component subtracted ");
  Scale_Data();
  Serial.println("Data scaled ");
  Serial.print("Minimum_Range = "); Serial.println(Minimum_Range);

  if (Range >= Minimum_Range) { // ADC range is > 50, otherwise increse gain
    Filter_Data();
    Serial.println("Data Filtered ");
    Compute_Pulse_Rate();
    Serial.println("Pulse rate computed ");
    Display_Samples();
    Serial.println("Sample displayed ");
    // Now print heart rate
    } 
  
  Serial.print(currentTime);
  Serial.println(" currentTime");

//  displayBPM();
//  currentTime =0;


}
void Read_ADC_Samples() {
  for (int i = 0; i < Num_Samples; i++) {
    //ADC_Samples[i] = 1023-analogRead(A1);
    ADC_Samples[i] = analogRead(A0);
    delay(5);

  }
}

void Remove_DC() {
  Find_Minima(0);
  Serial.print("Minima = ");
  Serial.println(Minima);
  // Subtract DC (minima)
  for (int i = 0; i < Num_Samples; i++) {
    ADC_Samples[i] = ADC_Samples[i] - Minima;
  }
  Minima = 0;  // New minima is zero
}

void Scale_Data() {
  // Find peak value
  
  sensorValue = analogRead(analogInPin);
  // map it to the range of the analog out:
  Find_Peak(0);
  Serial.print("Peak = ");
  Serial.println(Peak_Magnitude);
  Range = Peak_Magnitude - Minima;
  Serial.print("Range = ");
  Serial.println(Range);
  // Scale from 1 to 1023
  if (Range > Minimum_Range) {
    for (int i = 0; i < Num_Samples; i++) {
      ADC_Samples[i] = 1 + ((ADC_Samples[i] - Minima) * 1022) / Range;

    }
    Find_Peak(0);
    Find_Minima(0);
    Serial.print("Scaled Peak = ");
    Serial.println(Peak_Magnitude);
    Serial.print("Scaled Minima = ");
    Serial.println(Minima);
  }
}


void Filter_Data() {
  int Num_Points = 2 * Moving_Average_Num + 1;
  for (int i = Moving_Average_Num; i < Num_Samples - Moving_Average_Num; i++) {
    int Sum_Points = 0;
    for (int k = 0; k < Num_Points; k++) {
      Sum_Points = Sum_Points + ADC_Samples[i - Moving_Average_Num + k];
    }
    ADC_Samples[i] = Sum_Points / Num_Points;
  }
}


void Compute_Pulse_Rate() {
  // Detect Peak magnitude and minima
  Find_Peak(Moving_Average_Num);
  Find_Minima(Moving_Average_Num);
  Range = Peak_Magnitude - Minima;
  Peak_Threshold = Peak_Magnitude * Peak_Threshold_Factor;
  Peak_Threshold = Peak_Threshold / 100;

  // Now detect three peaks
  Peak1 = 0;
  Peak2 = 0;
  Peak3 = 0;
  Index1 = 0;
  Index2 = 0;
  Index3 = 0;
  // Find first peak
  for (int j = Moving_Average_Num; j < Num_Samples - Moving_Average_Num; j++) {
    if (ADC_Samples[j] >= ADC_Samples[j - 1] && ADC_Samples[j] > ADC_Samples[j + 1] &&
        ADC_Samples[j] > Peak_Threshold && Peak1 == 0) {
      Peak1 = ADC_Samples[j];
      Index1 = j;
    }

    // Search for second peak which is at least 10 sample time far
    if (Peak1 > 0 && j > (Index1 + Minimum_Peak_Separation) && Peak2 == 0) {
      if (ADC_Samples[j] >= ADC_Samples[j - 1] && ADC_Samples[j] > ADC_Samples[j + 1] &&
          ADC_Samples[j] > Peak_Threshold) {
        Peak2 = ADC_Samples[j];
        Index2 = j;
      }
    } // Peak1 > 0

    // Search for the third peak which is at least 10 sample time far
    if (Peak2 > 0 && j > (Index2 + Minimum_Peak_Separation) && Peak3 == 0) {
      if (ADC_Samples[j] >= ADC_Samples[j - 1] && ADC_Samples[j] > ADC_Samples[j + 1] &&
          ADC_Samples[j] > Peak_Threshold) {
        Peak3 = ADC_Samples[j];
        Index3 = j;
      }
    } // Peak2 > 0

  }
  Serial.print("Index1 = ");
  Serial.println(Index1);
  Serial.print("Index2 = ");
  Serial.println(Index2);
  Serial.print("Index3 = ");
  Serial.println(Index3);


  PR1 = (Index2 - Index1) * Sampling_Time; // In milliseconds
  PR2 = (Index3 - Index2) * Sampling_Time;
  Serial.print("PR1 = ");
  Serial.println(PR1);
  Serial.print("PR2 = ");
  Serial.println(PR2);
  if (PR1 > 0 && PR2 > 0) {
    Pulse_Rate = (PR1 + PR2) / 2;
    Pulse_Rate = 60000 / Pulse_Rate; // In BPM
    Serial.println("Pulse rate");
    Serial.println(Pulse_Rate);

  }
}

void Display_Samples() {
  Serial.println("DISPLAY SAMPLES");
  // tft.fillScreen(TFT_GREY);
  
//  y_axis_length = outputValue = map(sensorValue, 0, 1023, 0, 30);
  display.drawLine(0, 0, 0, y_axis_length, WHITE);
  display.drawLine(1, 0, 1, y_axis_length, WHITE);
  display.drawLine(0, y_axis_length, x_axis_length, y_axis_length, WHITE);
  display.drawLine(0, y_axis_length + 1, x_axis_length, y_axis_length + 1, WHITE);

  for (int i = Moving_Average_Num; i < Samples_to_Display; i += Display_Sampling) {
    int y_pos = 1 + ((ADC_Samples[i] - Minima) * (y_axis_length - 2)) / Range;
    y_pos = y_axis_length - y_pos;                                   
    display.drawPixel(i / Display_Sampling, y_pos, WHITE);
    display.drawPixel(i / Display_Sampling + 1, y_pos, WHITE);
    display.drawPixel(i / Display_Sampling, y_pos + 1, WHITE);
    display.drawPixel(i / Display_Sampling + 1, y_pos + 1, WHITE);
    display.drawPixel(i / Display_Sampling, y_pos - 1, WHITE);
  }

  // display after pixels have been set
  display.display();
  display.clearDisplay();
  delay(10);
  
  
}

void Find_Peak(int Num) {
  Peak_Magnitude = 0;
  for (int m = Num; m < Num_Samples - Num; m++) {
    if (Peak_Magnitude < ADC_Samples[m]) {
      Peak_Magnitude = ADC_Samples[m];
    }
  }
}

void Find_Minima(int Num) {
  Minima = 1024;
  for (int m = Num; m < Num_Samples - Num; m++) {
    if (Minima > ADC_Samples[m]) {
      Minima = ADC_Samples[m];
    }
  }
}


//void displayBPM()
//{
//
//  display.display();
//  
//  Serial.print("Inside displayBPM");
//  display.setTextColor(WHITE);
//  display.setCursor(50, 15);
//  display.setTextSize(2);
//  display.print(Pulse_Rate);
//  display.print(" BPM");
//  
//  display.display();
//  delay(2000);
//  display.clearDisplay();
//  delay(2000);
//
//
// }

