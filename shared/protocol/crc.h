#ifndef CRC_H
#define CRC_H

#include <stdint.h>

/*
This is really confusing so let me explain to best of my abilities:
The goal is to get the REMAINDER of data when divided by some CONSTANT we decide.
Data is an array of bytes. Each byte has bits. Hence the for loops.
The idea is that if one computer and another computer deciede on this CONSTANT to divide the data with...
then they know if something has changed or if the message was corrupted.

Think of it like a tag being like a recipet. 
After you get the data along with the recipet you can check based on the item costs to see if they add up
If they don't match the reciept end price something was messed up

The actual math involves bitwise operations using XOR or in C it is ^. This lets you subtract a value from another.
This comes directly from the truth table of XOR: 0,0=0 0,1=1 1,0=1 1,1=0. This is how subtraction works too.
Then the divisibility check uses the concept behind divisbility.
Think of base 10 normal numbers. You check if 600 / 3 by first dividing 6 (the left-most number) by 3 to see it fits 2 times.
In the same way we can check if the leftmost bit is a 1, then divisor fits in 1 time
Otherwise it is a 0 bit and the divisor fits 0 times.

Repeating this math across the entire multiple bytes of data, treating it as a big huge number
We can get a remainder when divided by a CONSTANT (i.e. 0x1021)
This is used as the CRC or corruption ending so the reciever can repeat the math of the data and compare to our remainder.
*/

static inline uint16_t ccsds_calculate_crc(const uint8_t *data, uint16_t length)
{
    uint16_t value = 0xFFFF; // use 0xFFFF instead of 0x0000 so we don't get false 0x0000 flags when its not working

    for (uint16_t byte = 0; byte < length; byte++) 
    {
        // grab the byte and set it to the top so that the byte sits on the left
        value ^= ( (uint16_t)data[byte] << 8);

        // for each of these 8 bits in the data[i] byte
        for (uint16_t bit = 0; bit < 8; bit++)
        {
            // check if the top bit (0x8000) is 1
            if (value & 0x8000)
            {
                // shift and subtract (XOR) the polynomial
                value = (value << 1) ^ 0x1021;
            } else {
                // shift left
                value <<= 1;
            }
        }
        
    }

    return value;
}

#endif