#ifndef _FAT32_FILENAMES_H
#define _FAT32_FILENAMES_H

// convert file names from their representation on disk
// eg., "TEST    TXT" becomes "TEST.TXT"
void filename_fs_to_8_3 (const char *input_name,
                         char *output_name);

// convert file names into their representation on disk
// eg., "test.txt" becomes "TEST    TXT"
void filename_8_3_to_fs (const char *input_name,
                         char output_name[13]);

#endif
