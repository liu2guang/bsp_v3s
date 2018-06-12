/*
 * File      : drv_uart.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2017, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2017-5-30      Bernard      the first version
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>

#include "board.h"
#include "interrupt.h"
#include <dfs_fs.h>

#define SD_LINK

#ifdef SD_LINK
#define SD_LINK_PRINTF         rt_kprintf
#else
#define SD_LINK_PRINTF(...)
#endif

struct mmc_mci {
	struct rt_mmcsd_host *host;
	void *dev_ptr;
    int plug;
    int clock;
    int bus_width;
    volatile int err;
};
static struct mmc_mci mci;

int sunxi_mmc_getcd(void *mmchost);
static inline uint32_t mmcsd_is_cd(struct mmc_mci *mmc)
{
	return sunxi_mmc_getcd(mmc->dev_ptr);
}

struct mmc_cmd {
	uint16_t cmdidx;
	uint32_t resp_type;
	uint32_t cmdarg;
	uint32_t response[4];
};
struct mmc_data {
	union {
		char *dest;
		const char *src;
	};
	uint32_t flags;
	uint32_t blocks;
	uint32_t blocksize;
};

#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_136	(1 << 1)		/* 136 bit response */
#define MMC_RSP_CRC	(1 << 2)		/* expect valid crc */
#define MMC_RSP_BUSY	(1 << 3)		/* card may send busy */
#define MMC_RSP_OPCODE	(1 << 4)		/* response contains opcode */

#define MMC_RSP_NONE	(0)
#define MMC_RSP_R1	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R1b	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE| MMC_RSP_BUSY)
#define MMC_RSP_R2	(MMC_RSP_PRESENT|MMC_RSP_136|MMC_RSP_CRC)
#define MMC_RSP_R3	(MMC_RSP_PRESENT)
#define MMC_RSP_R4	(MMC_RSP_PRESENT)
#define MMC_RSP_R5	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R6	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R7	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
static int mmc_resp_idx[0x10] = {
    MMC_RSP_NONE,MMC_RSP_R1,MMC_RSP_R1b,MMC_RSP_R2,
    MMC_RSP_R3,MMC_RSP_R4,MMC_RSP_R6,MMC_RSP_R7,MMC_RSP_R5};

#define MMC_DATA_READ		1
#define MMC_DATA_WRITE		2
static int mmc_flags_idx[0x04] = {0,MMC_DATA_WRITE,MMC_DATA_READ,MMC_DATA_READ|MMC_DATA_WRITE};
static void mmc_conv_reqtommc(struct rt_mmcsd_cmd* req, struct mmc_cmd *cmd, struct mmc_data *data)
{
    if (!req) return;
    rt_memset(cmd, 0, sizeof(struct mmc_cmd));
    rt_memset(data, 0, sizeof(struct mmc_data));
    cmd->cmdidx = req->cmd_code;
    cmd->resp_type = mmc_resp_idx[resp_type(req)];
    cmd->cmdarg = req->arg;
    if (!req->data) return;
    data->dest = (char *)req->data->buf;
    data->flags = mmc_flags_idx[req->data->flags&0x3];
    data->blocks = req->data->blks;
    data->blocksize = req->data->blksize;
}
extern int sunxi_mmc_send_cmd(void *mmchost, struct mmc_cmd *cmd, struct mmc_data *data);
static void mmc_mci_request(struct rt_mmcsd_host *host, struct rt_mmcsd_req *req)
{
	struct mmc_mci *mmc = (struct mmc_mci*)host->private_data;
    RT_ASSERT(mmc != RT_NULL);

    struct mmc_cmd cmd;
    struct mmc_data data;
    mmc_conv_reqtommc(req->cmd, &cmd, &data);
    req->cmd->err = sunxi_mmc_send_cmd(mmc->dev_ptr, &cmd, (req->cmd->data)?&data:RT_NULL);
    if (req->cmd->err == 0){
        rt_memcpy(req->cmd->resp, cmd.response, sizeof(req->cmd->resp));
        mmc->err = 0;
    }else{ mmc->err++; }
    if (req->stop) {
        mmc_conv_reqtommc(req->stop, &cmd, &data);
        req->stop->err = sunxi_mmc_send_cmd(mmc->dev_ptr, &cmd, (req->stop->data)?&data:RT_NULL);
        if (req->stop->err == 0){
            rt_memcpy(req->stop->resp, cmd.response, sizeof(req->stop->resp));
            mmc->err = 0;
        }else{ mmc->err++; }
    }
    mmcsd_req_complete(host);
}

extern int sunxi_mmc_core_init(void *mmchost);
extern void sunxi_mmc_set_ios(void *mmchost, int clock, int bus_width);
static void mmc_mci_set_iocfg(struct rt_mmcsd_host *host, struct rt_mmcsd_io_cfg *io_cfg)
{
	struct mmc_mci *mmc = (struct mmc_mci*)host->private_data;
    RT_ASSERT(mmc != RT_NULL);

	if (io_cfg->power_mode != MMCSD_POWER_OFF){
        if (io_cfg->clock != mmc->clock) {
            sunxi_mmc_core_init(mmc->dev_ptr);
            sunxi_mmc_set_ios(mmc->dev_ptr, io_cfg->clock, 1<<io_cfg->bus_width);
            mmc->clock = io_cfg->clock;
            mmc->bus_width = io_cfg->bus_width;
        }
        if (io_cfg->bus_width != mmc->bus_width){
            sunxi_mmc_set_ios(mmc->dev_ptr, 0, 1<<io_cfg->bus_width);
            mmc->bus_width = io_cfg->bus_width;
        }
    }
}

static const struct rt_mmcsd_host_ops ops = {
	mmc_mci_request,
	mmc_mci_set_iocfg,
};

static void sd_thread_entry(void *parameter)
{
    uint32_t status;
    struct mmc_mci *mmc = (struct mmc_mci*)parameter;
    
    status = mmc->plug;
    rt_thread_delay(10*RT_TICK_PER_SECOND);
    while(1)
    {
        if (status != MMCSD_HOST_PLUGED && !mmc->host->card){
            mmcsd_change(mmc->host);
            mmc->plug = mmcsd_wait_cd_changed(5000);
            if (mmc->plug == MMCSD_HOST_PLUGED) {
                SD_LINK_PRINTF("MMC: Card detected!\n");
               if (dfs_mount("sd0", "/mmc", "elm", 0, 0) == 0)
                    SD_LINK_PRINTF("Mount /mmc ok!\n");
                else
                    SD_LINK_PRINTF("Mount /mmc failed!\n");
                status = mmc->plug;
            }
        }else if(mmc->err > 10 && mmc->host->card){
            SD_LINK_PRINTF("MMC: No card detected!\n");
            mmcsd_change(mmc->host);
            mmc->plug = mmcsd_wait_cd_changed(5000);
            if (mmc->host->card == NULL)
                SD_LINK_PRINTF("Unmount /mmc ok!\n");
            else
                SD_LINK_PRINTF("Unmount /mmc failed!\n");
            status = mmc->plug;
            mmc->err = 0;
        }

        rt_thread_delay(10*RT_TICK_PER_SECOND);
    }
}

extern void *sunxi_mmc_probe(int sdc_no);
int rt_hw_tf_init(void)
{
	mci.host = mmcsd_alloc_host();
	if (!mci.host){
        rt_kprintf("failed to alloc host in rt_hw_rf_init\n");
		return -1;
    }
    gpio_set_mode(SUNXI_GPF(0), PIN_TYPE(SUNXI_GPF_SDC0)|PULL_UP);
    gpio_set_mode(SUNXI_GPF(1), PIN_TYPE(SUNXI_GPF_SDC0)|PULL_UP);
    gpio_set_mode(SUNXI_GPF(2), PIN_TYPE(SUNXI_GPF_SDC0)|PULL_UP);
    gpio_set_mode(SUNXI_GPF(3), PIN_TYPE(SUNXI_GPF_SDC0)|PULL_UP);
    gpio_set_mode(SUNXI_GPF(4), PIN_TYPE(SUNXI_GPF_SDC0)|PULL_UP);
    gpio_set_mode(SUNXI_GPF(5), PIN_TYPE(SUNXI_GPF_SDC0)|PULL_UP);
    mci.dev_ptr = sunxi_mmc_probe(0);
    if (!mci.dev_ptr){
        rt_kprintf("failed to init mmc in rt_hw_rf_init\n");
		return -1;
    }

	mci.host->ops = &ops;
	mci.host->freq_min = 400000;
	mci.host->freq_max = 50000000;
	mci.host->valid_ocr = VDD_32_33 | VDD_33_34;
	mci.host->flags = MMCSD_BUSWIDTH_4 | MMCSD_SUP_HIGHSPEED;
	mci.host->max_blk_size = 512;
	mci.host->max_blk_count = 4096;
	mci.host->private_data = &mci;

    mmcsd_change(mci.host);
    mci.plug = mmcsd_wait_cd_changed(5000);
    if (mci.plug == MMCSD_HOST_PLUGED) {
        SD_LINK_PRINTF("MMC: Card detected!\n");
        if (dfs_mount("sd0", "/mmc", "elm", 0, 0) == 0)
            SD_LINK_PRINTF("Mount /mmc ok!\n");
        else
            SD_LINK_PRINTF("Mount /mmc failed!\n");
    }else{
        SD_LINK_PRINTF("MMC: No card detected!\n");
    }

    /* start sd monitor */
    rt_thread_t tid = rt_thread_create("sd_mon", sd_thread_entry,
                           &mci,
                           2048, RT_THREAD_PRIORITY_MAX - 2, 20);
    rt_thread_startup(tid);
    
    return 0;
}
INIT_ENV_EXPORT(rt_hw_tf_init);
