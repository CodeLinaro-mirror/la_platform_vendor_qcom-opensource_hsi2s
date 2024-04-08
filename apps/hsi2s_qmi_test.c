/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Test app for hsi2s enable/disable clock via qmi framework
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>



//#define LOGD(fmt, args...) printf(""fmt"", ##args)
//#define LOGD(fmt, ...)
#define LOGD(fmt, args...) do\
{ \
        FILE* fp; \
        fp = fopen("/tmp/qmi.log", "a+"); \
        if (fp) { \
                fprintf(fp, ""fmt"", ##args); \
                fclose(fp); \
        }\
} while(0)


extern int hsi2s_qmi(int enable_hsi2s_clks);

int main(int argc, char *argv[])
{
	int enable_hsi2s_clks = 1;
	if (argc > 1) {
		enable_hsi2s_clks = !!atoi(argv[1]);
	}
	LOGD("\n%s: enable_hsi2s_clks :%d \n", argv[0], enable_hsi2s_clks);

	return hsi2s_qmi(enable_hsi2s_clks);
}

