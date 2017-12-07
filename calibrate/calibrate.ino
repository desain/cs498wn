#define INPUT_PIN 0
#define NUM_SAMPS 500
#define OUTPUT_LED_PIN 8
#define OUTPUT_LED_PIN_TWO 9
#define OUTPUT_LED_PIN_THREE 7
#define OUTPUT_LED_PIN_FOUR 10

/*
 * Green off: Mean: 96.49 STD: 16.79
 * Green on: Mean: 250.58 STD: 6.31
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

  digitalWrite(OUTPUT_LED_PIN, HIGH);
  digitalWrite(OUTPUT_LED_PIN_TWO, HIGH);
  digitalWrite(OUTPUT_LED_PIN_THREE, HIGH);
  digitalWrite(OUTPUT_LED_PIN_FOUR, HIGH);

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
