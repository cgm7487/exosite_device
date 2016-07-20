#include <pt.h>
#include <stdio.h>

static 
PT_THREAD(data_activate_task(struct pt *pt))
{
	PT_BEGIN(pt);

	printf("it's activate task1\r\n");

	printf("it's activate task2\r\n");

	PT_END(pt);
}

static 
PT_THREAD(data_write_task(struct pt *pt))
{
	PT_BEGIN(pt);

	printf("it's write task1\r\n");
	PT_YIELD(pt);

	printf("it's write task2\r\n");
	PT_YIELD(pt);

	PT_END(pt);
}


static 
PT_THREAD(data_read_task(struct pt *pt))
{
	struct pt actPt;
	PT_BEGIN(pt);

	printf("it's read task1\r\n");

	PT_SPAWN(pt, &actPt, data_activate_task(&actPt));

	printf("it's read task2\r\n");
	PT_YIELD(pt);

	PT_END(pt);
}

static 
PT_THREAD(driver_thread(struct pt *pt))
{
	struct pt writePt, readPt;

	PT_BEGIN(pt);

	PT_INIT(&writePt);
	PT_INIT(&readPt);

	PT_WAIT_THREAD(pt, 	data_write_task(&writePt) |
			 			data_read_task(&readPt));

	PT_END(pt);
}

int
	main()
{
	struct pt driver_pt;

	PT_INIT(&driver_pt);

	while(PT_SCHEDULE(driver_thread(&driver_pt)))
	{
	}

	printf("task ended...\r\n");
	return 0;
}

