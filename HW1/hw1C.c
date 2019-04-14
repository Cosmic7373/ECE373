// Bliss 4-13-19
// ECE 373 HW1 Part C
// Modified from https://www.tutorialgateway.org/c-program-to-convert-celsius-to-fahrenheit/

#include <stdio.h>

int main() {
    float celsius, fahrenheit;

    printf("Please Enter temperature in Celsius: \n");
    scanf("%f", &celsius);

    // Convert the temperature from celsius to fahrenheit
    fahrenheit = (1.8 * celsius) + 32;
    printf("\n %.2f Celsius = %.2f Fahrenheit\n", celsius, fahrenheit);

    return 0;
}
