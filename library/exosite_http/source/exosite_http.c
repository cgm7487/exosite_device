#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include <pt.h>

#include <typedefs.h>
#include <exosite.h>
#include <config.h>
#include <platform/exosite_pal.h>

#include "utility.h"

#define PDU_LENGTH HTTP_MSG_SIZE
#define WAIT_TIME_MS 2000

enum
{
    EXO_SOCK_STATE_IDLE = 0,
	EXO_SOCK_STATE_REQ_READY,
    EXO_SOCK_STATE_WAIT_RSP,
    //EXO_SOCK_STATE_DISCONNECT,
    //EXO_SOCK_STATE_COMPLETED,
};

enum
{
	EXO_DEVICE_STATE_UNINITIALIZED,
	EXO_DEVICE_STATE_INITIALIZED
};

enum
{
    EXO_OP_MODE_NOP,
    EXO_OP_MODE_ACTIVATE,
    EXO_OP_MODE_READ,
    EXO_OP_MODE_WRITE,
    EXO_OP_MODE_SUBSCRIBE,
	EXO_OP_MODE_CONTENT_LIST,
	EXO_OP_MODE_CONTENT_INFO,
	EXO_OP_MODE_CONTENT_DOWNLOAD
};

typedef struct
{
    int sock;
    char pdu[PDU_LENGTH];
    exosite_data_port_t dataPorts[NUM_OF_DATA_PORTS];
	size_t dataPortIndex;
	exo_result_callback_t resultCallback;

    exosite_timer_t waitTimer;
    uint8_t sockStatus;
    uint8_t opMode;

	int opStatus;
} exosite_http_context_t;

typedef struct
{
    exosite_http_context_t queueInstance[NUM_OF_SOCKETS];
	char vendor[64];
	char model[64];
	char sn[32];
    char cik[CIK_LENGTH + 1];

	uint8_t deviceStatus;
} exosite_http_context_queue_t;

exosite_http_context_queue_t exoHttpContextQueue;

static char recDateTime[128];

static exo_error_status_t
	exo_activate(	const char *vendor,
					const char *model,
					const char *sn);

static void
	release_op(exosite_http_context_t *context);

static bool
	is_http_op_context_existed(uint8_t opMode);

static exosite_http_context_t *
	get_http_op_context(uint8_t opMode);

static bool
	build_http_msg(exosite_http_context_t *);

static bool
	assign_call_back(	exo_result_callback_t lp,
						const exo_result_callback_t rp);

static void
	check_http_error_status_to_determine_if_reset_cik(int status);

static void
	close_socket(int *sp);

static 
PT_THREAD(data_activate_thread(struct pt *pt))
{
	PT_BEGIN(pt);

	char pdu[PDU_LENGTH];
    char dataBuf[PDU_LENGTH];
    int bufLen;
    int status;

	int sock;
	exosite_timer_t waitTimer;

	do
	{

		if(exosite_pal_load_cik(exoHttpContextQueue.cik,
								CIK_LENGTH))
			break;

		build_msg_activate(	pdu,
							sizeof(pdu),
							exoHttpContextQueue.vendor,
							exoHttpContextQueue.model,
							exoHttpContextQueue.sn);

		if(!exosite_pal_sock_connect(&sock))
		{
			exosite_pal_sock_close(&sock);
			break;
		}

		if(!exosite_pal_sock_write( &sock,
		                            pdu,
		                            strlen(pdu)))
		{
			exosite_pal_sock_close(&sock);
			break;
		}

		exosite_pal_timer_init(&waitTimer);
		exosite_pal_timer_countdown_ms(&waitTimer, WAIT_TIME_MS);

		while(1)
		{
			if(exosite_pal_timer_expired(&waitTimer))
			{
				exosite_pal_sock_close(&sock);
				break;
			}

			bufLen = sizeof(pdu);
			if(exosite_pal_sock_read(	&sock,
										dataBuf,
				                    	&bufLen)  > 0)
				break;
		}

		if(!parse_rsp_status(	dataBuf,
								bufLen,
								&status))
		{
			exosite_pal_sock_close(&sock);
			break;
		}

		if(status != 200)
		{
			exosite_pal_sock_close(&sock);
			break;
		}

		if(!parse_cik_info(	dataBuf,
							bufLen,
							exoHttpContextQueue.cik))
		{
			exosite_pal_sock_close(&sock);
			break;
		}

		exosite_pal_save_cik(	exoHttpContextQueue.cik,
								CIK_LENGTH);

		exosite_pal_sock_close(&sock);
	} while(false);

	PT_END(pt);
}

static 
PT_THREAD(data_read_thread(struct pt *pt))
{
PT_BEGIN(pt);

	char dataBuf[4096];
	static exosite_http_context_t *opContext;
	int status;
	int i;

	do
	{
		printf("data_read_thread():\r\n");
		if(!is_http_op_context_existed(EXO_OP_MODE_READ))
			break;

		opContext = get_http_op_context(EXO_OP_MODE_READ);

		if(exoHttpContextQueue.deviceStatus != EXO_DEVICE_STATE_INITIALIZED)
		{
			struct pt actPt;

			PT_SPAWN(pt, &actPt, data_activate_thread(&actPt));

			exoHttpContextQueue.deviceStatus = EXO_DEVICE_STATE_INITIALIZED;
		}

		if(!build_http_msg(opContext))
		{
			release_op(opContext);
			break;
		}

		if(!exosite_pal_sock_write( &opContext->sock,
									opContext->pdu,
									strlen(opContext->pdu)))
		{
			opContext->opStatus = -1;

			opContext->resultCallback(	NULL,
										opContext->opStatus);

			close_socket(&opContext->sock);
			release_op(opContext);
			break;
		}

		PT_YIELD(pt);

		memset(dataBuf, 0, sizeof(dataBuf));
		size_t bufLen = sizeof(dataBuf);

		exosite_pal_timer_init(&opContext->waitTimer);
		exosite_pal_timer_countdown_ms(&opContext->waitTimer, WAIT_TIME_MS);

		bool isReadData = true;
		while(exosite_pal_sock_read(	&opContext->sock,
										dataBuf,
										&bufLen) == 0)
		{
			PT_YIELD(pt);
			if(exosite_pal_timer_expired(&opContext->waitTimer))
			{
				opContext->opStatus = -1;

				opContext->resultCallback(	NULL,
											opContext->opStatus);

				close_socket(&opContext->sock);
				release_op(opContext);

				isReadData = false;
				break;
			}
		}

		if(!isReadData)
			break;

		printf("data_read_thread(): got rsp\r\n");

		if(parse_rsp_status(dataBuf,
							bufLen,
							&status))
		{
			opContext->opStatus = status;

			if(status == 200)
			{								
				if(!parse_msg_read(	dataBuf,
									bufLen,
									opContext->dataPorts,
									opContext->dataPortIndex))
				{
					break;
				}
			}

			check_http_error_status_to_determine_if_reset_cik(status);

			if(status == 401 || status == 403)
			{
				struct pt actPt;

				PT_SPAWN(pt, &actPt, data_activate_thread(&actPt));
			}

			for(i = 0; i < opContext->dataPortIndex; ++i)
			{
				opContext->resultCallback(	&opContext->dataPorts[i],
											opContext->opStatus);	
			}

			release_op(opContext);
		}
	} while(false);

PT_END(pt);
}

static 
PT_THREAD(data_long_polling_thread(struct pt *pt))
{
PT_BEGIN(pt);

	char dataBuf[4096];
	static exosite_http_context_t *opContext;
	int status;
	int i;

	do
	{
		printf("data_long_polling_thread():\r\n");
		if(!is_http_op_context_existed(EXO_OP_MODE_SUBSCRIBE))
		{
			printf("no subscribe, thread end\r\n");
			break;
		}

		opContext = get_http_op_context(EXO_OP_MODE_SUBSCRIBE);

		if(exoHttpContextQueue.deviceStatus != EXO_DEVICE_STATE_INITIALIZED)
		{
			struct pt actPt;

			PT_SPAWN(pt, &actPt, data_activate_thread(&actPt));

			exoHttpContextQueue.deviceStatus = EXO_DEVICE_STATE_INITIALIZED;
		}

		if(!build_http_msg(opContext))
		{
			release_op(opContext);
			break;
		}

		if(!exosite_pal_sock_write( &opContext->sock,
									opContext->pdu,
									strlen(opContext->pdu)))
		{
			opContext->opStatus = -1;

			opContext->resultCallback(	NULL,
										opContext->opStatus);

			close_socket(&opContext->sock);
			release_op(opContext);
			break;
		}

		PT_YIELD(pt);

		memset(dataBuf, 0, sizeof(dataBuf));
		size_t bufLen = sizeof(dataBuf);

		exosite_pal_timer_init(&opContext->waitTimer);
		exosite_pal_timer_countdown_ms(&opContext->waitTimer, WAIT_TIME_MS);

		bool isReadData = true;
		while(exosite_pal_sock_read(	&opContext->sock,
										dataBuf,
										&bufLen) == 0)
		{
			PT_YIELD(pt);
			if(exosite_pal_timer_expired(&opContext->waitTimer))
			{
				opContext->opStatus = -1;

				opContext->resultCallback(	NULL,
											opContext->opStatus);

				close_socket(&opContext->sock);
				release_op(opContext);

				isReadData = false;
				break;
			}
		}

		if(!isReadData)
			break;

		if(parse_rsp_status(dataBuf,
							bufLen,
							&status))
		{
			opContext->opStatus = status;

			if(status == 304)
			{

				opContext->resultCallback(	&opContext->dataPorts[0],
											opContext->opStatus);

			}

			if(status == 200)
			{								
				if(!parse_msg_read(	dataBuf,
									bufLen,
									opContext->dataPorts,
									opContext->dataPortIndex))
				{
					break;
				}
			}

			check_http_error_status_to_determine_if_reset_cik(status);

			if(status == 401 || status == 403)
			{
				struct pt actPt;

				PT_SPAWN(pt, &actPt, data_activate_thread(&actPt));
			}

			for(i = 0; i < opContext->dataPortIndex; ++i)
			{
				opContext->resultCallback(	&opContext->dataPorts[i],
											opContext->opStatus);	
			}

			release_op(opContext);
		}
	} while(false);

PT_END(pt);
}

static 
PT_THREAD(data_write_thread(struct pt *pt))
{
PT_BEGIN(pt);

	char dataBuf[4096];

	static exosite_http_context_t *opContext;
	int status;

	do
	{
		printf("data_write_thread():\r\n");
		if(!is_http_op_context_existed(EXO_OP_MODE_WRITE))
			break;

		opContext = get_http_op_context(EXO_OP_MODE_WRITE);

		if(exoHttpContextQueue.deviceStatus != EXO_DEVICE_STATE_INITIALIZED)
		{
			struct pt actPt;

			PT_SPAWN(pt, &actPt, data_activate_thread(&actPt));

			exoHttpContextQueue.deviceStatus = EXO_DEVICE_STATE_INITIALIZED;
		}

		if(!build_http_msg(opContext))
		{
			release_op(opContext);
			break;
		}

		if(!exosite_pal_sock_write( &opContext->sock,
									opContext->pdu,
									strlen(opContext->pdu)))
		{
			opContext->opStatus = -1;

			opContext->resultCallback(	NULL,
										opContext->opStatus);

			close_socket(&opContext->sock);
			release_op(opContext);
			break;
		}

		PT_YIELD(pt);

		memset(dataBuf, 0, sizeof(dataBuf));
		size_t bufLen = sizeof(dataBuf);

		exosite_pal_timer_init(&opContext->waitTimer);
		exosite_pal_timer_countdown_ms(&opContext->waitTimer, WAIT_TIME_MS);

		bool isReadData = true;
		while(exosite_pal_sock_read(	&opContext->sock,
										dataBuf,
										&bufLen) == 0)
		{
			PT_YIELD(pt);
			if(exosite_pal_timer_expired(&opContext->waitTimer))
			{
				opContext->opStatus = -1;

				opContext->resultCallback(	NULL,
											opContext->opStatus);

				close_socket(&opContext->sock);
				release_op(opContext);

				isReadData = false;
				break;
			}
		}

		if(!isReadData)
			break;

		printf("data_write_thread(): got rsp\r\n");


		if(parse_rsp_status(dataBuf,
							bufLen,
							&status))
		{
			opContext->opStatus = status;
			opContext->resultCallback(	NULL,
										opContext->opStatus);

			release_op(opContext);

			check_http_error_status_to_determine_if_reset_cik(status);
		}

		printf("rsp status = %d\r\n", status);

		if(status == 401 || status == 403)
		{
			struct pt actPt;

			PT_SPAWN(pt, &actPt, data_activate_thread(&actPt));
		}
	} while(false);

PT_END(pt);
}

static 
PT_THREAD(driver_thread(struct pt *pt))
{
	static struct pt writePt, readPt, lpPt;

	PT_BEGIN(pt);

	PT_INIT(&writePt);
	PT_INIT(&readPt);
	PT_INIT(&lpPt);

	PT_WAIT_THREAD(pt,	data_write_thread(&writePt) &
			 			data_read_thread(&readPt) &
						data_long_polling_thread(&lpPt));

	PT_END(pt);
}

void
	exo_init(	const char *vendor,
				const char *model,
				const char *sn)
{
	int i;

	memset(exoHttpContextQueue.cik, 0, sizeof(exoHttpContextQueue.cik));
	for(i = 0; i < NUM_OF_SOCKETS; ++i)
	{
		release_op(&exoHttpContextQueue.queueInstance[i]);
		exoHttpContextQueue.queueInstance[i].sock = -1;
	}

	strcpy(exoHttpContextQueue.vendor, vendor);
	strcpy(exoHttpContextQueue.model, model);
	strcpy(exoHttpContextQueue.sn, sn);

	exosite_pal_init();
}

exo_desc_t
	exo_write(	const char *alias,
				const char *value,
				exo_result_callback_t writeCallback)
{
	exosite_http_context_t *opContext = get_http_op_context(EXO_OP_MODE_WRITE);
	if(opContext == NULL)
		return -1;

	if(opContext->dataPortIndex >= NUM_OF_DATA_PORTS)
		return -1;

	if(!assign_call_back(opContext->resultCallback, writeCallback))
		return -1;

	opContext->resultCallback = writeCallback;

	assert(opContext->resultCallback != NULL);

	//insert write data to op
	strcpy(opContext->dataPorts[opContext->dataPortIndex].alias, alias);
	strcpy(opContext->dataPorts[opContext->dataPortIndex].value, value);
	++opContext->dataPortIndex;

	opContext->sockStatus = EXO_SOCK_STATE_REQ_READY;
	
	return opContext->sock;
}

exo_desc_t
	exo_read(	const char *alias,
				exo_result_callback_t readCallback)
{
	//insert read data to op
	exosite_http_context_t *opContext = get_http_op_context(EXO_OP_MODE_READ);

	if(opContext == NULL)
	{
		printf("no context\r\n");
		return -1;
	}

	if(opContext->dataPortIndex >= NUM_OF_DATA_PORTS)
	{
		printf("data port filled\r\n");
		return -1;
	}

	if(!assign_call_back(opContext->resultCallback, readCallback))
		return -1;

	opContext->resultCallback = readCallback;

	strcpy(opContext->dataPorts[opContext->dataPortIndex].alias, alias);
	++opContext->dataPortIndex;

	opContext->sockStatus = EXO_SOCK_STATE_REQ_READY;
	
	return opContext->sock;
}

exo_desc_t
	exo_subscribe(	const char *alias,
					const date_time_t *dateTime,
					exo_result_callback_t subscribeCallback)
{
	//insert subscribe data to op
	exosite_http_context_t *opContext = get_http_op_context(EXO_OP_MODE_SUBSCRIBE);

	if(opContext == NULL)
		return -1;

	if(!assign_call_back(opContext->resultCallback, subscribeCallback))
		return -1;

	//if(opContext->dataPortIndex >= 1) //only support one dataport subscribe now.
	//	return -1;

	opContext->resultCallback = subscribeCallback;

	//strcpy(opContext->dataPorts[opContext->dataPortIndex].alias, alias);
	//++opContext->dataPortIndex;

	//new alias will replace old alias in the current implementation.
	
	strcpy(opContext->dataPorts[0].alias, alias);
	opContext->dataPortIndex = 1;

	opContext->sockStatus = EXO_SOCK_STATE_REQ_READY;

	strcpy(recDateTime, dateTime->toString);

	return opContext->sock;
}

void
	exo_loop_start()
{
	struct pt driver_pt;

	PT_INIT(&driver_pt);

	while(PT_SCHEDULE(driver_thread(&driver_pt)))
	{
	}
}

static exo_error_status_t
	exo_activate(	const char *vendor,
					const char *model,
					const char *sn)
{
	char pdu[PDU_LENGTH];
    char dataBuf[PDU_LENGTH];
    int bufLen;
    int status;

	int sock;
	exosite_timer_t waitTimer;

	if(exosite_pal_load_cik(exoHttpContextQueue.cik,
							CIK_LENGTH))
	{
		return EXO_ERROR_STATUS_SUC;
	}

    build_msg_activate(	pdu,
						sizeof(pdu),
						vendor,
						model,
						sn);
	if(!exosite_pal_sock_connect(&sock))
	{
		exosite_pal_sock_close(&sock);
		return EXO_ERROR_STATUS_FAILED;
	}

    if(!exosite_pal_sock_write( &sock,
                                pdu,
                                strlen(pdu)))
    {
    	exosite_pal_sock_close(&sock);
        return EXO_ERROR_STATUS_FAILED;
    }

	exosite_pal_timer_init(&waitTimer);
	exosite_pal_timer_countdown_ms(&waitTimer, WAIT_TIME_MS);

	while(1)
	{
		if(exosite_pal_timer_expired(&waitTimer))
		{
			exosite_pal_sock_close(&sock);
			return EXO_ERROR_TIME_OUT_FAILED;
		}

		bufLen = sizeof(pdu);
		if(exosite_pal_sock_read(	&sock,
									dataBuf,
		                        	&bufLen)  > 0)
			break;
	}

	if(!parse_rsp_status(	dataBuf,
							bufLen,
							&status))
	{
		exosite_pal_sock_close(&sock);
		return EXO_ERROR_STATUS_FAILED;
	}

	if(status != 200)
	{
		printf("activate status = %d\r\n", status);
		exosite_pal_sock_close(&sock);
		return EXO_ERROR_STATUS_FAILED;
	}

	if(!parse_cik_info(	dataBuf,
						bufLen,
						exoHttpContextQueue.cik))
	{
		exosite_pal_sock_close(&sock);
		return EXO_ERROR_STATUS_FAILED;
	}

	exosite_pal_save_cik(	exoHttpContextQueue.cik,
							CIK_LENGTH);

	exosite_pal_sock_close(&sock);

	return EXO_ERROR_STATUS_SUC;
}

static void
	release_op(exosite_http_context_t *context)
{
	//exosite_pal_sock_close(&context->sock);
	//context->sock = -1;
	memset(context->pdu, 0, sizeof(context->pdu));
	context->dataPortIndex = 0;
	context->resultCallback = NULL;
	context->sockStatus = EXO_SOCK_STATE_IDLE;
	context->opMode = EXO_OP_MODE_NOP;
	context->opStatus = -1;
}

static bool
	is_http_op_context_existed(uint8_t opMode)
{
	int i;

	for(i = 0; i < NUM_OF_SOCKETS; ++i)
	{
		if(opMode == exoHttpContextQueue.queueInstance[i].opMode)
			return true;
	}
	return false;
}

static exosite_http_context_t *
	get_http_op_context(uint8_t opMode)
{
	int i;

	// search existed opMode first
	for(i = 0; i < NUM_OF_SOCKETS; ++i)
	{
		if(opMode == exoHttpContextQueue.queueInstance[i].opMode)
			return &exoHttpContextQueue.queueInstance[i];
	}

	// search unoccupied context
	for(i = 0; i < NUM_OF_SOCKETS; ++i)
	{
		if(exoHttpContextQueue.queueInstance[i].opMode == EXO_OP_MODE_NOP)
		{
			if(exoHttpContextQueue.queueInstance[i].sock < 0)
			{
				if(exosite_pal_sock_connect(&exoHttpContextQueue.queueInstance[i].sock))
				{
					exoHttpContextQueue.queueInstance[i].opMode = opMode;
					return &exoHttpContextQueue.queueInstance[i];
				}
			}
			else
			{
				exoHttpContextQueue.queueInstance[i].opMode = opMode;
				return &exoHttpContextQueue.queueInstance[i];
			}
		}
	}

	return NULL;
}

static bool
	build_http_msg(exosite_http_context_t *cxt)
{
	switch(cxt->opMode)
	{
		case EXO_OP_MODE_READ:
			return build_msg_read(	cxt->pdu,
									PDU_LENGTH,
									cxt->dataPorts,
									cxt->dataPortIndex,
									exoHttpContextQueue.cik);
		//break;

		case EXO_OP_MODE_WRITE:
			return build_msg_write(	cxt->pdu,
									PDU_LENGTH,
									cxt->dataPorts,
									cxt->dataPortIndex,
									exoHttpContextQueue.cik);
		//break;

		case EXO_OP_MODE_SUBSCRIBE:
			return build_msg_long_polling(	cxt->pdu,
											PDU_LENGTH,
											recDateTime,
											cxt->dataPorts[0].alias,
											500, //cxt->reqTimeout,
											exoHttpContextQueue.cik);
		//break;

		default:
			assert(false);
		break;
	}

	return false;
}

static bool
	assign_call_back(	exo_result_callback_t lp,
						const exo_result_callback_t rp)
{
	assert(rp != NULL);

	if(lp != NULL)
	{
		if(lp != rp)
		{
			return false;
		}

		return true;
	}

	lp = rp; // TODO: check why assign fail?

	return true;
}

static void
	check_http_error_status_to_determine_if_reset_cik(int status)
{
	if(status == 401 || status == 403)
	{
		exosite_pal_remove_cik();
		exoHttpContextQueue.deviceStatus = EXO_DEVICE_STATE_UNINITIALIZED;
	}
}

static void
	close_socket(int *sp)
{
	exosite_pal_sock_close((void *)sp);
	*sp = -1;
}
