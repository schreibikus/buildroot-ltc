#include <linux/types.h>
#include <vstorage.h>
#include <asm-arm/io.h>
#include <bootlist.h>
#include <lowlevel_api.h>
#include <linux/byteorder/swab.h>
#include <efuse.h>
#include <asm-arm/arch-q3f/imapx_base_reg.h>
#include <asm-arm/arch-q3f/imapx_spiflash.h>
#include <asm-arm/arch-q3f/imapx_pads.h>
#include <preloader.h>
#include <lowlevel_api.h>
#include <gdma.h>

//#define SPI_CLK 27000000//3000000
#define LED_NUM 147

#define SPI_TFIFO_EMPTY  (1 << 0)
#define SPI_TFIFO_NFULL  (1 << 1)
#define SPI_RFIFO_NEMPTY (1 << 2)
#define SPI_BUSY         (1 << 4)

#define FLASH_CMD_WREN  0x6
#define FLASH_CMD_WRDI  0x4
#define FLASH_CMD_RDSR  0x5
#define FLASH_CMD_WRSR  0x1
#define FLASH_CMD_READ  0x3
#define FLASH_CMD_FARD  0xb
#define FLASH_CMD_WRIT  0x2
#define FLASH_CMD_WESR  0x50
#define FLASH_CMD_ERAS  0xc7

#define FLASH_READ_BLOCK 0x1000

#define FLASH_MOD_POLLREDY 0
#define FLASH_MOD_POLLWREN 1
#define FLASH_DUMMY   FLASH_CMD_RDSR

#define SSP_DMA_DISABLED		(0)
#define SSP_DMA_ENABLED			(1)

/*
 * SSP DMA Control Register - SSP_DMACR
 */
/* Receive DMA Enable bit */
#define SSP_DMACR_MASK_RXDMAE		(0x1UL << 0)
/* Transmit DMA Enable bit */
#define SSP_DMACR_MASK_TXDMAE		(0x1UL << 1)


#define SSP_CR0_MASK_DSS	(0x0FUL << 0)
#define SSP_CR0_MASK_FRF	(0x3UL << 4)
#define SSP_CR0_MASK_SPO	(0x1UL << 6)
#define SSP_CR0_MASK_SPH	(0x1UL << 7)
#define SSP_CR0_MASK_SCR	(0xFFUL << 8)

/*
 * SSP Control Register 0  - SSP_CR1
 */
#define SSP_CR1_MASK_LBM	(0x1UL << 0)
#define SSP_CR1_MASK_SSE	(0x1UL << 1)
#define SSP_CR1_MASK_MS		(0x1UL << 2)
#define SSP_CR1_MASK_SOD	(0x1UL << 3)

#define SSP_IMSC_MASK_RORIM (0x1UL << 0) /* Receive Overrun Interrupt mask */
#define SSP_IMSC_MASK_RTIM  (0x1UL << 1) /* Receive timeout Interrupt mask */
#define SSP_IMSC_MASK_RXIM  (0x1UL << 2) /* Receive FIFO Interrupt mask */
#define SSP_IMSC_MASK_TXIM  (0x1UL << 3) /* Transmit FIFO Interrupt mask */
#define DEFAULT_SSP_REG_IMSC  0x0UL
#define DISABLE_ALL_INTERRUPTS DEFAULT_SSP_REG_IMSC
#define ENABLE_ALL_INTERRUPTS (~DEFAULT_SSP_REG_IMSC)

#define CLEAR_ALL_INTERRUPTS  0x3

/*
 * SSP State - Whether Enabled or Disabled
 */
#define SSP_DISABLED			(0)
#define SSP_ENABLED			(1)


//#undef writel(v,a)
//#undef readl(a)
//#define readl(a)			(*(volatile unsigned int *)(a))
//#define writel(v,a)		(*(volatile unsigned int *)(a) = (v))

#define readl_temp(a)			(*(volatile unsigned int *)(a))
#define writel_temp(v,a)		(*(volatile unsigned int *)(a) = (v))

#define SSP_WRITE_BITS(reg, val, mask, sb) \
 ((reg) = (((reg) & ~(mask)) | (((val)<<(sb)) & (mask))))

#undef philip_printf
#define philip_printf(strings, arg...) do {\
    if(1) { \
        printf("\033[0;44m""%s""\033[0m""\033[0;34m"",%s()""\033[0m""\033[0;32m"",%d:""\033[0m" strings "\n", __FILE__,__FUNCTION__,__LINE__, ##arg); \
    } \
}while(0)


#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define memcpy irf->memcpy
#define module_enable irf->module_enable
#define pads_chmod irf->pads_chmod
#define pads_pull irf->pads_pull
#define printf irf->printf

struct spi_transfer {
    uint8_t *in;
    uint8_t *out;
    int len;
};

extern struct irom_export *irf;
static int spibytes = 256;

static void gpio_out(int num,int value)
{
    *(unsigned int *)(0x20f00000+num*0x40+0x4) =!!value;  
}

static void gpio_init(int num)
{
    *((unsigned int *)(0x21e0906c+(num/8)*4)) |= 1<<(num%8);//GPIO function
    *(unsigned int *)(0x20f00000+num*0x40+0x8) = 1;//direct:output
    *(unsigned int *)(0x20f00000+num*0x40+0x4) = 1;//high 
}

static void spi_set_bytes(int bytes) {
    spibytes = bytes;
    printf("spi: bytes set to: %d\n", bytes);
}

static int spi_init_clk(void)
{
    int pclk =134000000;// module_get_clock(APB_CLK_BASE),
    int	i, j;

#if 0
    for(i = 2; i < 255; i += 2)
        for(j = 0; j < 256; j++)
            if(pclk / (i * (j + 1)) <= SPI_CLK)
                goto calc_finish;

calc_finish:
    if(i >= 255 || j >= 256) {
        printf("spi: calc clock failed(%d, %d), use default.\n", i, j);
        i = 2; j = 39;
    }
    printf("PCLK: %d, PS: %d, SCR: %d, Fout: %d\n", 
            pclk, i, j, pclk / (i * (j + 1)));
#else
    i = 2;
    j = 1;
    printf("PCLK: %d, PS: %d, SCR: %d\n", 
            pclk, i, j);
#endif
    writel(0, SSP_CR1_0);
    writel(0, SSP_IMSC_0);
    writel(i, SSP_CPSR_0);
    writel((j << 8) | (3 << 6) | (0 << 4) | (7 << 0), SSP_CR0_0);
    writel((1 << 3) | (0 << 2) | (0 << 1) | (0 << 0), SSP_CR1_0);
    writel(16<<8 | 16, SSP_FIFOTH_0);

    return 0;
}

static int spi_init_pads(void) 
{
    int index;

    //if(boot_device() != DEV_FLASH)
    //printf("warning: not spi boot\n");

    //	index = (xom == 4) ? 74: 
    //		((xom == 6)? 27: 121);
    index = 70;
    //printf("xom=%d\n", xom);

    pads_chmod(PADSRANGE(index, (index + 4)), PADS_MODE_CTRL, 0);
    pads_chmod(80, PADS_MODE_CTRL, 0);
    //if(ecfg_check_flag(ECFG_ENABLE_PULL_SPI))

    pads_pull(PADSRANGE(index, (index + 4)), 1, 1);
    pads_pull(80, 1, 0);		/* clk */

    return 0;
}

static void spi_fifo_clear(void)
{
    while(readl(SSP_SR_0) & SPI_BUSY);
    while(readl(SSP_SR_0) & SPI_RFIFO_NEMPTY) {
        readl(SSP_DR_0);
    }
}

static void clear_result_buf(char *data_addr, unsigned int data_len)
{
    unsigned int tmp;
    unsigned char* pSrc;

    tmp = data_len;
    pSrc = (unsigned char *)data_addr;
    while(tmp)
    {
        *pSrc++= 0;
        tmp--;
    }
}

#define min(X, Y)				\
    ({ typeof (X) __x = (X), __y = (Y);	\
     (__x < __y) ? __x : __y; })

static void spi_dma_start(struct spi_transfer *td, int count)
{
	int skip = 0, dummy = 0;
	uint32_t len;
    uint8_t *p, ex[8], exlen = 0;
	struct gdma_addr src;
	struct gdma_addr dst;

	/*open spi cs*/
	gpio_out(74, 0);
	
	len = skip = td->len;
	p = td->out;

	if(count > 1) {
		memcpy(ex, td->out, len);
		td++;
		exlen = min(8, len + td->len);

		if(td->out) {
			memcpy(ex + len, td->out, exlen - len);
			p	 = td->out + exlen - len;
			len	 = td->len + len - exlen;
			skip += td->len;
		} else {
			p = td->in;
			len	 = td->len;
			dummy = td->len + len - exlen;
		}
	}
	
	src.byte = SSP_DR_0;
	src.modbyte = 0;
	dst.byte = p;
	dst.modbyte = 0;

	spi_fifo_clear();
	/*clear storage buf*/
	clear_result_buf(p, len);
#if 0
	/*send read cmd and address to spi*/
	dma_spi_send_cmd((struct gdma_addr*)&src, (struct gdma_addr*)&dst, ex, exlen, &len);
	/*start transfer*/
    dma_peri_transfer((struct gdma_addr*)&src, (struct gdma_addr*)&dst, len);
	/*empty spi fifo*/
	dma_peri_flush_fifo((struct gdma_addr*)&src, (struct gdma_addr*)&dst);
#endif
    dma_peri_transfer_with_cmd((struct gdma_addr*)&src, (struct gdma_addr*)&dst, len, ex, exlen);

#if 0
	int i;
	for(i = 0; i < (len > 20? 20: len); i++) {
		printf("tx->in[%d] = 0x%x\n", i, p[i]);
	}
#endif
	/*close spi cs*/
	gpio_out(74, 1);
}

int flash_read(uint8_t *buf, int offs, int len, int extra)
{
	uint8_t tx[8] = {0, 0, 0, FLASH_CMD_FARD };
	int *d = (int *)&tx[4];
	struct spi_transfer td[] = {
		{NULL, &tx[3], 5 },
		{buf,  NULL, 0 }
	}, *p;

	p = &td[1];
	*d = offs;
	*d = __swab32(*d);
	*d >>= 8;

	p->len = len;	
	
	spi_dma_start(td, ARRAY_SIZE(td));
	
	return 0;
}

void spi_dma_open()
{
	int val;

	val = readl(SSP_DMACR_0);
	val &= ~0x3;
	writel(val | 0x3, SSP_DMACR_0);
}

int flash_reset(void) 
{
	int ret;

    module_enable(SSP_SYSM_ADDR);
	spi_init_clk();
	spi_init_pads();
	
	spi_dma_open();

    spi_fifo_clear();
    gpio_init(74);//for Q3 spi cs,gpio95
	
	ret = init_gdma();
	if(ret < 0){
		printf("dma init error!\n");
		while(1);
	}
    return 0;
}


