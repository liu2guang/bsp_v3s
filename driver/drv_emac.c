#include "rthw.h"
#include <rtthread.h>
#include "rtdevice.h"

#include "board.h"
#include "interrupt.h"
#include <netif/ethernetif.h>
#include <lwipopts.h>

//#define NET_TRACE
//#define ETH_RX_DUMP
//#define ETH_TX_DUMP

#ifdef NET_TRACE
#define NET_DEBUG         rt_kprintf
#else
#define NET_DEBUG(...)
#endif /* #ifdef NET_TRACE */

#define PHY_ADDR            1
#define MAX_ADDR_LEN 		6
#define EMAC_INT_STA		0x08
#define EMAC_INT_EN			0x0c

#define _EMAC_DEVICE(eth)	(struct emac_device*)(eth)
#define __REG(x)     (*((volatile ulong *)(x)))

struct emac_device
{
	/* inherit from Ethernet device */
	struct eth_device parent;
    /* uboot dev_t */
    uint32_t sysctl;
    uint32_t base;
    void * dev_ptr;
	/* interface address info. */
	rt_uint8_t  dev_addr[MAX_ADDR_LEN];			/* MAC address	*/
};
static struct emac_device _emac;

void _enet_isr(int vector, void *param)
{
    struct eth_device *dev = (struct eth_device *)param;
    struct emac_device *emac = _EMAC_DEVICE(dev);
    RT_ASSERT(emac != RT_NULL);

    eth_device_ready(dev);
    __REG(emac->base + EMAC_INT_STA) = 0x100;
}

extern int _sun8i_emac_eth_init(void *priv, rt_uint8_t *enetaddr);
static rt_err_t _emac_init(rt_device_t dev)
{
    struct emac_device *emac = _EMAC_DEVICE(dev);
    RT_ASSERT(emac != RT_NULL);

	/* initialize enet */
    _sun8i_emac_eth_init(emac->dev_ptr, emac->dev_addr);
	return RT_EOK;
}

static rt_err_t _emac_open(rt_device_t dev, rt_uint16_t oflag)
{
	return RT_EOK;
}

static rt_err_t _emac_close(rt_device_t dev)
{
	return RT_EOK;
}

static rt_size_t _emac_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size)
{
	rt_set_errno(-RT_ENOSYS);
	return 0;
}

static rt_size_t _emac_write (rt_device_t dev, rt_off_t pos, const void* buffer, rt_size_t size)
{
	rt_set_errno(-RT_ENOSYS);
	return 0;
}

static rt_err_t _emac_control(rt_device_t dev, int cmd, void *args)
{
    struct emac_device *emac = _EMAC_DEVICE(dev);
    RT_ASSERT(emac != RT_NULL);

	switch(cmd)
	{
	case NIOCTL_GADDR:
		/* get MAC address */
		if(args) rt_memcpy(args, emac->dev_addr, MAX_ADDR_LEN);
		else return -RT_ERROR;
		break;

	default :
		break;
	}

	return RT_EOK;
}

/* Ethernet device interface */
/* transmit packet. */
extern int _sun8i_emac_eth_send(void *priv, void *packet, int len, int offs);
rt_err_t _emac_tx(rt_device_t dev, struct pbuf* p)
{
    struct pbuf* q;
    rt_err_t result = RT_EOK;
    struct emac_device *emac = _EMAC_DEVICE(dev);
    RT_ASSERT(emac != RT_NULL);

#ifdef ETH_TX_DUMP
    rt_size_t dump_count = 0;
    rt_uint8_t * dump_ptr;
    rt_size_t dump_i;
    NET_DEBUG("tx_dump, size:%d\r\n", p->tot_len);
#endif
    int offset = 0;
    for (q = p; q != NULL; q = q->next){
        _sun8i_emac_eth_send(emac->dev_ptr, q->payload, q->len, offset|(q->next?0x10000:0x0));
        offset += q->len;
#ifdef ETH_TX_DUMP
        dump_ptr = q->payload;
        for(dump_i=0; dump_i<q->len; dump_i++)
        {
            NET_DEBUG("%02x ", *dump_ptr);
            if( ((dump_count+1)%8) == 0 )
            {
                NET_DEBUG("  ");
            }
            if( ((dump_count+1)%16) == 0 )
            {
                NET_DEBUG("\r\n");
            }
            dump_count++;
            dump_ptr++;
        }
#endif
    }
#ifdef ETH_TX_DUMP
    NET_DEBUG("\r\n");
#endif

    return result;
}

/* reception packet. */
extern int _sun8i_eth_recv(void *priv, rt_int8_t **packetp);
extern int _sun8i_free_pkt(void *priv);
struct pbuf *_emac_rx(rt_device_t dev)
{
    struct pbuf *q,*p = RT_NULL;
    struct emac_device *emac = _EMAC_DEVICE(dev);
    RT_ASSERT(emac != RT_NULL);

    do {
        rt_int8_t *framepack = RT_NULL;
        int framelength = _sun8i_eth_recv(emac->dev_ptr, &framepack);
        if (framelength <= 0){
            break;
        }
        p = pbuf_alloc(PBUF_LINK, framelength, PBUF_RAM);
        if (p == RT_NULL) {
            break;
        }
#ifdef ETH_RX_DUMP
        rt_size_t dump_count = 0;
        rt_uint8_t * dump_ptr;
        rt_size_t dump_i;
        NET_DEBUG("rx_dump, size:%d\r\n", framelength);
#endif
        int offset = 0;
        for (q = p; q != RT_NULL; q= q->next){
            rt_memcpy(q->payload,framepack+offset,q->len);
            offset += q->len;
#ifdef ETH_RX_DUMP
            dump_ptr = q->payload;
            for(dump_i=0; dump_i<q->len; dump_i++)
            {
                NET_DEBUG("%02x ", *dump_ptr);
                if( ((dump_count+1)%8) == 0 )
                {
                    NET_DEBUG("  ");
                }
                if( ((dump_count+1)%16) == 0 )
                {
                    NET_DEBUG("\r\n");
                }
                dump_count++;
                dump_ptr++;
            }
#endif
        }
        _sun8i_free_pkt(emac->dev_ptr);
#ifdef ETH_RX_DUMP
        NET_DEBUG("\r\n");
#endif
    }while (0);

    return p;
}

extern int miiphy_link(const char *devname, unsigned char addr);
static void phy_thread_entry(void *parameter)
{
    struct eth_device *dev = (struct eth_device *)parameter;
    struct emac_device *emac = _EMAC_DEVICE(dev);
    RT_ASSERT(emac != RT_NULL);

    // wait for init to complete
    int initlink = 0;
    rt_thread_delay(RT_TICK_PER_SECOND);
    while (1)
    {
        /* check link status */
        int link = miiphy_link("emac", PHY_ADDR);
        if (link != initlink){
            rt_kprintf("emac link status:%d\n", link);
            eth_device_linkchange(dev, link);
            initlink = link;
        }
        /* dma buf error, restat emac */
        int status = __REG(emac->base + EMAC_INT_STA);
        if (status & 0x40){
            __REG(emac->base + EMAC_INT_STA) = 0xffff;
            rt_kprintf("emac dma err status:%x\n", status);
            eth_device_linkchange(dev, 0);
            _emac_init(&dev->parent);
            rt_thread_delay(RT_TICK_PER_SECOND);
        }
        rt_thread_delay(RT_TICK_PER_SECOND);
    }
}

extern int sun8i_emac_eth_probe(const char* name, uint32_t sysctl, uint32_t reg, uint8_t addr, void **priv);
int rt_hw_eth_init(void)
{
    _emac.sysctl = 0x01c00030;
    _emac.base = 0x01c30000;
    int ret = sun8i_emac_eth_probe("emac", _emac.sysctl, _emac.base, PHY_ADDR, &_emac.dev_ptr);
    if (ret != 0){
        rt_kprintf("failed to rt_hw_eth_init code:%d\n", ret);
        return ret;
    }

    /* test MAC address */
	_emac.dev_addr[0] = 0x00;
	_emac.dev_addr[1] = 0x11;
	_emac.dev_addr[2] = 0x22;
	_emac.dev_addr[3] = 0x33;
	_emac.dev_addr[4] = 0x44;
	_emac.dev_addr[5] = 0x55;

	_emac.parent.parent.init       = _emac_init;
	_emac.parent.parent.open       = _emac_open;
	_emac.parent.parent.close      = _emac_close;
	_emac.parent.parent.read       = _emac_read;
	_emac.parent.parent.write      = _emac_write;
	_emac.parent.parent.control    = _emac_control;
	_emac.parent.parent.user_data  = RT_NULL;

	_emac.parent.eth_rx     = _emac_rx;
	_emac.parent.eth_tx     = _emac_tx;

    /* register ETH device */
    eth_device_init(&(_emac.parent), "e0");
    rt_hw_interrupt_install(114, _enet_isr, &(_emac.parent), "emac");
    rt_hw_interrupt_umask(114);

    /* check phy link status */
    rt_thread_t tid = rt_thread_create("phy", phy_thread_entry,
                           &(_emac.parent),
                           512, RT_THREAD_PRIORITY_MAX - 4, 20);
    rt_thread_startup(tid);

	return 0;
}
INIT_DEVICE_EXPORT(rt_hw_eth_init);
