#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <exosite.h>
#include <platform/exosite_pal.h>

void write_callback(const exosite_data_port_t *,
					int opStatus);

void read_callback(	const exosite_data_port_t *,
					int opStatus);

void subscribe_callback(const exosite_data_port_t *,
						int opStatus);

int main()
{
	int sockRead, sockWrite, sockSub;
	int len;
	int opStatus = -1;
	int i;
	date_time_t curDateTime;

	
	exo_init(	"hackntu2016",
				"iot1001",
				"1A:01:00:00:00:03");

	exosite_pal_get_current_date_time(&curDateTime);

	while(1)
	{
		printf("start to write data to cloud\r\n");
		sockWrite = exo_write(	"AccXRaw_ICM20648",
								"3.5",
								write_callback);

		sockRead = exo_read("LED1Status_Board",
							read_callback);

		sockSub = exo_subscribe("LED1Status_Board",
								&curDateTime,
								subscribe_callback);


		printf("loop start\r\n");
		exo_loop_start();

		exosite_pal_get_current_date_time(&curDateTime);

	
		printf("loop end\r\n");
	}

	return 0;
}

void
	write_callback(	const exosite_data_port_t *dataPort,
					int opStatus)
{
	printf("write status = %d\r\n", opStatus);
}

void
	read_callback(	const exosite_data_port_t *dataPort,
					int opStatus)
{
	printf("read status = %d\r\n", opStatus);

	printf("%s=%s\r\n", dataPort->alias, dataPort->value);
}

void
	subscribe_callback(	const exosite_data_port_t *dataPort,
						int opStatus)
{
	printf("subscribe status = %d\r\n", opStatus);
	if(opStatus == 200)
		printf("%s=%s\r\n", dataPort->alias, dataPort->value);
}

