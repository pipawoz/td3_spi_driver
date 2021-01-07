#include <linux/init.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/pagemap.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/gpio.h>

/*************************************************************************
 *  Module Description
 **************************************************************************/

MODULE_DESCRIPTION("SPI Driver");
MODULE_AUTHOR("Pedro Wozniak Lorice");
MODULE_LICENSE("GPL");

/*************************************************************************
 *  Platform Driver Defines
 **************************************************************************/

#define SPI_DRIVER_NAME "SPI TD3 DRIVER"
#define SPI_DRIVER_NAME_SHORT "spi_td3"
#define SPI_DRIVER_CLASS "spi_td3_class"
#define SPI_DRIVER_MINORBASE 0
#define SPI_DRIVER_MINORCOUNT 1
#define SPI_DRIVER_DEV_PARENT NULL
#define SPI_DRIVER_DEVDATA NULL

/*************************************************************************
 *  BMP280 Defines
 **************************************************************************/

static DECLARE_WAIT_QUEUE_HEAD (spi_rx_queue);  /*Interrupt queue */
static DECLARE_WAIT_QUEUE_HEAD (spi_tx_queue);	/*Interrupt queue */


#define BMP280_SENSOR_ADDR 0x77     /* Sensor Address */
#define BMP280_CHIP_ID 0x58         /* Device Chip ID */

#define CM_PER 0x44E00000           /* Clock Module Peripheral Registers */
#define CONTROL_MODULE 0X44E10000   /* Control Module Registers*/
#define SPI_ADDR 0x48030000         /* SPI Base Address */


/* Configuration registers */
#define MCSPI_SYSCONFIG (0x110)
#define MCSPI_SYSSTATUS ( 0x114)
#define MCSPI_IRQSTATUS ( 0x118)
#define MCSPI_IRQENABLE (0x11C)
#define MCSPI_SYST (0x124)
#define MCSPI_MODULCTRL (0x128)
#define MCSPI_CH0CONF (0x12C)
#define MCSPI_CH0STAT (0x130)
#define MCSPI_CH0CTRL (0x134)
#define MCSPI_TX0 (0x138)      /* Channel 0 data to transmit */
#define MCSPI_RX0 (0x13C)      /* Channel 0 data received */

#define CM_PER_SPI0_CLKCTRL (0x4C)

#define PIN_SPI0_SCLK_OFFSET (0x950)
#define PIN_SPI0_D0_OFFSET (0x954)
#define PIN_SPI0_D1_OFFSET (0x958)
#define PIN_SPI0_CS0_OFFSET (0x95C)


/* MCSPI_SYSSTATUS Pag4924 */
#define SYS_STAT_RD (1 << 0) /* Reset Done */

/* MCSPI_IRQSTATUS Pag4925 */
#define IRQ_STAT_TXS (1 << 0)  /* TX0 Empty */
#define IRQ_STAT_RXS (1 << 2)  /* RX0 Full */

/* MCSPI_IRQENABLE Pag4928 */
#define IRQ_EN_TXE (1 << 0) /* Enable TX0 Empty */
#define IRQ_EN_RXE (1 << 2) /* Enable RX0 Full */
#define IRQ_EN_TXD (0 << 0) /* Disable TX0 Empty */
#define IRQ_EN_RXD (0 << 2) /* Disable RX0 Full */

/* MCSPI_CH0CTRL */
#define SPI_CH0_EN (1 << 0)     /* Enable MCSPI_CH0 */
#define SPI_CH0_DS (0 << 0)     /* Disable MCSPI_CH0 */


/* MCSPI_CH0CONF */
#define GPIO_CS (115)
#define CS0_EN (1 << 20)
#define CS0_DS (0 << 20)

/* CS_GPIO */
#define CS0_GPIO_EN (0x00)
#define CS0_GPIO_DS (0x01)
#define GPIO_OUTPUT (1)
#define GPIO_INPUT  (0)

/*************************************************************************
 *  Function Primitives
 **************************************************************************/

static int spi_driver_open(struct inode *inode, struct file *filp);
static int spi_driver_close(struct inode *inode, struct file *filp);
static ssize_t spi_driver_read(struct file *filp, char *buff, size_t count, loff_t *offp);
static ssize_t spi_driver_write(struct file *filp, const char *buff, size_t count, loff_t *offp);
static int spi_driver_probe(struct platform_device *pdev);
static int spi_driver_remove(struct platform_device *pdev);
static irqreturn_t spi_driver_irq_handler(int irq, void *dev_id, struct pt_regs *regs);


/*************************************************************************
 *  Global Variables
 **************************************************************************/

struct spi_dev_data_t
{
    uint8_t chip_id;                         /* Chip ID */
    uint8_t dev_addr;                        /* Device Address */
    size_t spi_irq_num;                      /* Interrupt number */
    char *p_tx_buff;                         /* Tx buffer */
    char *p_rx_buff;                         /* Rx buffer */
    size_t tx_len;                           /* Tx length */
    size_t rx_len;                           /* Rx length */
    size_t tx_pos;                           /* Tx position */
    size_t rx_pos;                           /* Rx position */
    uint32_t irqstatus;                      /* IRQ Status */
    struct mutex mtx_lock;                   /* Mutex */
    void *pcm_per;
    void *pcontrol_module;
    void *pspi_addr;
};

/* Platform driver */
static dev_t spi_driver_dev;
static struct class *spi_driver_dev_class;
static struct cdev *p_cdev;


/*************************************************************************
 *  Debug Wrapper
 **************************************************************************/

#define DEBUG

#ifdef DEBUG
#    define print_dbg(fmt, args...) printk(KERN_DEBUG "spi_driver: " fmt, ##args)
#    define print_err(fmt, args...) printk(KERN_ERR "spi_driver: " fmt, ##args)
#    define print_info(fmt, args...) printk(KERN_INFO "spi_driver: " fmt, ##args)
#else
#    define print_dbg(fmt, args....) \
        do                           \
        {                            \
        } while(0)
#    define print_err(fmt, args....) \
        do                           \
        {                            \
        } while(0)
#    define print_info(fmt, args....) \
        do                            \
        {                             \
        } while(0)
#endif
