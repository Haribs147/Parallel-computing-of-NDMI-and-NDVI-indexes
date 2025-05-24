#ifndef DATA_TYPES_H
#define DATA_TYPES_H

typedef struct
{
    char** path;
    float** raw_data;
    float** processed_data;
    int* width;
    int* height;
    const char* band_name;
} BandData;

enum BandType
{
    B04,
    B08,
    B11,
    SCL
};

#endif // DATA_TYPES_H
