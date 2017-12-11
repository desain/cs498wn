#define INPUT_PIN 0
#define NUM_SAMPS 500
#define OUTPUT_LED_PIN 8
#define OUTPUT_LED_PIN_TWO 9
#define OUTPUT_LED_PIN_THREE 7
#define OUTPUT_LED_PIN_FOUR 10
#define OUTPUT_LED_PIN_FIVE 6
#define OUTPUT_LED_PIN_SIX 5
#define OUTPUT_LED_PIN_SEVEN 4
#define LEDS_ON 0


/*
 * Red on:
Min: 182 Max: 250
Mean: 211.36 STD: 16.86
Min: 180 Max: 240
Mean: 210.22 STD: 16.88

Red off:
Min: 157 Max: 219
Mean: 186.74 STD: 17.05
Min: 135 Max: 230
Mean: 167.76 STD: 28.84



 */

/*
 * Everything has to be natural, ambient light at this point
 */
int sample_count = 0;
int samples[NUM_SAMPS];
void setup() {
  delay(1000); 
  pinMode(INPUT_PIN, INPUT);
  Serial.begin(9600);
  Serial.println("CALIBRATING");
  pinMode(OUTPUT_LED_PIN, OUTPUT);
  pinMode(OUTPUT_LED_PIN_TWO, OUTPUT);
  pinMode(OUTPUT_LED_PIN_THREE, OUTPUT);
  pinMode(OUTPUT_LED_PIN_FOUR, OUTPUT);
  pinMode(OUTPUT_LED_PIN_FIVE, OUTPUT);
  pinMode(OUTPUT_LED_PIN_SIX, OUTPUT);
  pinMode(OUTPUT_LED_PIN_SEVEN, OUTPUT);

  #if LEDS_ON == 1
  digitalWrite(OUTPUT_LED_PIN, HIGH);
  digitalWrite(OUTPUT_LED_PIN_TWO, HIGH);
  digitalWrite(OUTPUT_LED_PIN_THREE, HIGH);
  digitalWrite(OUTPUT_LED_PIN_FOUR, HIGH);
  digitalWrite(OUTPUT_LED_PIN_FIVE, HIGH);
  digitalWrite(OUTPUT_LED_PIN_SIX, HIGH);
  digitalWrite(OUTPUT_LED_PIN_SEVEN, HIGH);
  #endif
}

float calculate_mean()
{
  int maximum = 0;
  int minimum = samples[0];
  float mean = 0;
  for (int i = 0; i < NUM_SAMPS; i++) {
    mean += samples[i];
    if (samples[i] > maximum)
      maximum = samples[i];
    if (samples[i] < minimum)
      minimum = samples[i];
  }
  Serial.print("Min: ");
  Serial.print(minimum);
  Serial.print(" Max: ");
  Serial.println(maximum);
  return mean / NUM_SAMPS;
}

float calculate_std(float mean)
{
  float sum = 0;
  float diff = 0.;
  for (int i = 0; i < NUM_SAMPS; i++) {
    diff = samples[i] - mean;
    sum += (diff * diff);
  }
  return (sqrt(sum/NUM_SAMPS));
}

void loop() {
  // put your main code here, to run repeatedly:
  if(sample_count < NUM_SAMPS) {
    int input =  analogRead(INPUT_PIN);
    if (input < 1023) {
      samples[sample_count] = input;
      sample_count++;
    }
    Serial.println(input);

  }
  else if (sample_count == NUM_SAMPS){
    float mean = calculate_mean();
    float std = calculate_std(mean);
    Serial.print("Mean: ");
    Serial.print(mean);
    Serial.print(" STD: ");
    Serial.println(std);
    sample_count++;
  }
}
