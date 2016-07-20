#ifndef _EXOSITE_TYPEDEFS_H_
#define _EXOSITE_TYPEDEFS_H_

#include <stdint.h>
#include <stdbool.h>

#define CIK_LENGTH 40

typedef int exo_desc_t;


typedef struct
{
    char alias[64];
    char value[32];
} exosite_data_port_t;

typedef struct
{
	char toString[64];
} date_time_t;

typedef struct
{
	char id[32];
	//int idLen;
} content_id_t;

typedef struct
{
	//uint8_t contentType;
	char contentType[32];
	int contentSize;
	date_time_t updatedTimeStamp;
} content_info_t;

#endif
