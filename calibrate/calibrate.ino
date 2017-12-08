#define INPUT_PIN 0
#define NUM_SAMPS 500
#define OUTPUT_LED_PIN 8
#define OUTPUT_LED_PIN_TWO 9
#define OUTPUT_LED_PIN_THREE 7
#define OUTPUT_LED_PIN_FOUR 10
#define LEDS_ON 0


/*
 * Distance: 53.75 inches
 * Green off: Mean: 95.15 STD: 14.61
 * 

 * Green on: 
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

  #if LEDS_ON == 1
  digitalWrite(OUTPUT_LED_PIN, HIGH);
  digitalWrite(OUTPUT_LED_PIN_TWO, HIGH);
  digitalWrite(OUTPUT_LED_PIN_THREE, HIGH);
  digitalWrite(OUTPUT_LED_PIN_FOUR, HIGH);
  #endif
}

float calculate_mean()
{
  float mean = 0;
  for (int i = 0; i < NUM_SAMPS; i++) {
    mean += samples[i];
  }
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
