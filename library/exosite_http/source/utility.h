#ifndef _EXOSITE_UTILITY_H_
#define _EXOSITE_UTILITY_H_

#include <typedefs.h>


bool
    build_msg_activate( char *pdu,
						int length,
						const char *vendor,
						const char *model,
						const char *serialNumber);

int
    convert_data_ports_to_read_string(	char *dataString,
										const exosite_data_port_t *dataPorts,
										uint8_t numofDataPorts);

bool
    build_msg_read(	char *pdu,
					int length,
					const exosite_data_port_t *dataPorts,
					uint8_t numOfDataPorts,
					const char *cik);

int
    convert_data_ports_to_write_string(	char *dataString,
										const exosite_data_port_t *dataPorts,
										uint8_t numofDataPorts);
bool
    build_msg_write(char *pdu,
					int length,
					const exosite_data_port_t *dataPorts,
					uint8_t numOfDataPorts,
					const char *cik);

bool
    build_msg_long_polling(	char *pdu,
                            int length,
                            char *recDateTime,
                            const char *alias,
                            uint32_t reqTimeout,
							const char *cik);

bool
	build_msg_list_content(	char *pdu,
							int length,
							const char *vendor,
							const char *model,
							const char *cik);

bool
    build_msg_get_content_info(	char *pdu,
								int length,	
								const char *vendor,
								const char *model,
								const char *contentId,
								const char *cik);

bool
	build_msg_get_content(	char *pdu,
							int length,
							const char *vendor,
							const char *model,
							const char *contentId,
							int startPos,
							int tendPos,
							const char *cik);

bool
    parse_msg_read(	const char *parseData,
                    int dataLen,
                    exosite_data_port_t *dataPorts,
					int numofDataPorts);

bool
    parse_rsp_status(	const char *parseData,
						int dataLen,
						int *status);

bool
    parse_cik_info(	const char *parseData,
                    int dataLen,
                    char *cik);

bool
	parse_content_list(	const char *parseData,
						int dataLen,
						content_id_t *idList,
						int *listSize);

bool
	parse_content_info(	const char *parseData,
						int dataLen,
						content_info_t *contentInfo);

bool
	parse_content(	const char *parseData,
					int dataLen,
					uint8_t *buf,
					int *bufSize); 
	

#endif
