/**
 * FGo
 * LLVM instrumentation test program
 *
 * Created by Joshua Yao on Nov. 27, 2023
 */

#include <stdio.h>
#include <stdlib.h>

int addNumber(int a, int b)
{
    return a + b;
}

int multiplyNumber(int a, int b)
{
    return a * b;
}

typedef int (*operateNumber)(int, int);

int main()
{
    int input = 0, result = 0;
    operateNumber functions[] = {&addNumber, &multiplyNumber};

    scanf("%d", &input);
    if (input < 0 || input > 10000) {
        printf("Wrong input!");
        return 1;
    }

    switch (input) {
    case 0:
    case 1:
        result = functions[input](input, input + 1);
        break;
    default:
        result = functions[input % 2](input * 3, input * 4);
        break;
    }

    return 0;
}