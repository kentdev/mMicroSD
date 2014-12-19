#include "fat32_filenames.h"

#include <stdint.h>

#ifndef bool
#include <stdbool.h>
#endif

// convert file names from their representation on disk
// eg., "TEST    TXT" becomes "TEST.TXT"
void filename_fs_to_8_3 (const char *input_name,
                         char output_name[13])
{
    uint8_t in_index = 0;
    uint8_t out_index = 0;
    
    bool wrote_dot = false;
    
    while (in_index < 11 && out_index < 12)
    {
        if (in_index < 8)
        {
            if (input_name[in_index] == ' ')
            {
                in_index = 8;  // skip to the last 3 chars
                continue;
            }
            else
            {
                output_name[out_index++] = input_name[in_index];
            }
        }
        else
        {  // extension characters
            if (input_name[in_index] != ' ')
            {
                if (!wrote_dot)
                {
                    output_name[out_index++] = '.';
                    wrote_dot = true;
                }
                output_name[out_index++] = input_name[in_index];
            }
        }
        
        in_index++;
    }
    
    output_name[out_index] = '\0';
}


// convert file names into their representation on disk
// eg., "test.txt" becomes "TEST    TXT"
void filename_8_3_to_fs (const char *input_name,
                         char *output_name)
{
    uint8_t in_index = 0;
    uint8_t out_index = 0;
    
    char in_char;
    
    while (out_index < 11)
    {
        if (in_index >= 12 || input_name[in_index] == '\0')
        {
            break;
        }
        else if (input_name[in_index] == '.')
        {  // encountered a period
            while (out_index < 8)
            {  // fill the output with spaces until the 3-char extension
                output_name[out_index] = ' ';
                out_index++;
            }
            
            in_index++;
        }
        else
        {
            in_char = input_name[in_index];
            
            // convert to uppercase
            if (in_char >= (char)0x61 && in_char <= (char)0x7a)
				in_char -= 0x20;
            
            output_name[out_index] = in_char;
            
            in_index++;
            out_index++;
        }
    }
    
    while (out_index < 11)
    {
        output_name[out_index] = ' ';
        out_index++;
    }
}

