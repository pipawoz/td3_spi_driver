/**
 * @file spi_driver.c
 * @author Pedro Wozniak Lorice (pwozniaklorice@est.frba.utn.edu.ar)
 * @brief spi driver
 * @version 1.0
 * @date 2019-11-28
 *
 * @copyright Copyright (c) 2019
 *
 */


/*************************************************************************
 *  Header Files
 **************************************************************************/


#include "spi_driver.h"

/* Device info structure */
static struct spi_dev_data_t dev;
static atomic_t index_rx;
static atomic_t index_tx;

static struct file_operations spi_driver_dev_fops = { .owner = THIS_MODULE,
                                                      .open = spi_driver_open,
                                                      .release = spi_driver_close,
                                                      .write = spi_driver_write,
                                                      .read = spi_driver_read };


static const struct of_device_id spi_td3_driver_of_match[] = {
    { .compatible = "td3,omap4-mcspi" },
    {},
};


MODULE_DEVICE_TABLE(of, spi_td3_driver_of_match);


static struct platform_driver spi_driver_platform = {
    .driver =
      {
        .name = "spi_driver_td3_platform",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(spi_td3_driver_of_match),
      },
    .probe = spi_driver_probe,
    .remove = spi_driver_remove,
};


/*************************************************************************
 *  Functions
 **************************************************************************/


/**
 * @brief Driver IRQ Handler
 *
 * @param irq IRQ number
 * @param dev_id Device ID
 * @param regs Struct pointer
 * @return irqreturn_t Return value
 */
static irqreturn_t spi_driver_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    iowrite32(IRQ_STAT_TXS | IRQ_STAT_RXS, dev.pspi_addr + MCSPI_IRQSTATUS);

    if(dev.irqstatus & IRQ_STAT_RXS)
    {
        iowrite32(IRQ_STAT_RXS, dev.pspi_addr + MCSPI_IRQSTATUS);

        dev.p_rx_buff[dev.rx_pos] = ioread32(dev.pspi_addr + MCSPI_RX0);

        if(dev.rx_pos >= dev.rx_len)
        {
            iowrite32(IRQ_EN_RXD, dev.pspi_addr + MCSPI_IRQENABLE);
            iowrite32(SPI_CH0_DS, dev.pspi_addr + MCSPI_CH0CTRL);
            atomic_inc(&index_rx);
            wake_up_interruptible(&spi_rx_queue);
            return IRQ_HANDLED;
        }
        else
        {
            dev.rx_pos++;
            return IRQ_HANDLED;
        }
    }

    if(dev.irqstatus & IRQ_STAT_TXS)
    {
        iowrite32(IRQ_STAT_TXS, dev.pspi_addr + MCSPI_IRQSTATUS);

        if(dev.tx_pos >= dev.tx_len)
        {
            iowrite32(IRQ_EN_TXD, dev.pspi_addr + MCSPI_IRQENABLE);
            iowrite32(SPI_CH0_DS, dev.pspi_addr + MCSPI_CH0CTRL);
            atomic_inc(&index_tx);
            wake_up_interruptible(&spi_tx_queue);
            return IRQ_HANDLED;
        }
        else
        {
            iowrite32(((( int )dev.p_tx_buff[dev.tx_pos]) << 8), dev.pspi_addr + MCSPI_TX0);
            dev.tx_pos++;
            dev.tx_pos++;
            iowrite32(IRQ_EN_TXE, dev.pspi_addr + MCSPI_IRQENABLE);
            return IRQ_HANDLED;
        }
    }

    return IRQ_HANDLED;
}

/**
 * @brief Driver write function
 *
 * @param filp File struct pointer
 * @param buff User buff
 * @param count Size read
 * @param offp
 * @return ssize_t Return value
 */
static ssize_t spi_driver_read(struct file *filp, char *buff, size_t count, loff_t *offp)
{
    uint32_t reg_data = 0;
    uint32_t rv = 0;

    if(!(access_ok(VERIFY_WRITE, buff, count)))
    {
        print_err("Invalid read buffer\n");
        return -ENOMEM;
    }

    reg_data = ioread32(dev.pspi_addr + MCSPI_IRQSTATUS);
    iowrite32(IRQ_STAT_RXS | reg_data, dev.pspi_addr + MCSPI_IRQSTATUS);

    dev.rx_len = count;
    dev.rx_pos = 0;
    atomic_set(&index_rx, 0);

    dev.tx_len = count;
    dev.tx_pos = 0;
    atomic_set(&index_tx, 0);

    rv = copy_from_user(dev.p_tx_buff, buff, count);

    if(rv != 0)
    {
        print_err("copy_from_user error\n");
        kfree(dev.p_tx_buff);
        return -EFAULT;
    }

    iowrite32(((( int )dev.p_tx_buff[dev.tx_pos]) << 8), dev.pspi_addr + MCSPI_TX0);

    iowrite32(IRQ_EN_TXE, dev.pspi_addr + MCSPI_IRQENABLE);

    gpio_set_value(GPIO_CS, CS0_GPIO_EN);

    iowrite32(SPI_CH0_EN, dev.pspi_addr + MCSPI_CH0CTRL);

    if((rv = wait_event_interruptible(spi_tx_queue, (atomic_read(&index_tx)) > 0)) < 0)
    {
        print_err("wait_event_interruptible write read error\n");
        return rv;
    }

    iowrite32(IRQ_EN_RXE, dev.pspi_addr + MCSPI_IRQENABLE);

    if((rv = wait_event_interruptible(spi_rx_queue, (atomic_read(&index_rx)) > 0)) < 0)
    {
        print_err("wait_event_interruptible read error\n");
        return rv;
    }

    gpio_set_value(GPIO_CS, CS0_GPIO_DS);

    iowrite32(SPI_CH0_DS, dev.pspi_addr + MCSPI_CH0CTRL);

    rv = copy_to_user(buff, dev.p_rx_buff, count);

    if(rv == 0)
    {
        print_info("%d bytes sent to user\n", sizeof(count));
        return count;
    }
    else
    {
        print_err("error sending %d bytes to user\n", sizeof(count));
        return -EFAULT;
    }

    return rv;
}


/**
 * @brief Driver write function
 *
 * @param filp File struct pointer
 * @param buff User buff
 * @param count Size to write
 * @param offp
 * @return ssize_t Return value
 */
static ssize_t spi_driver_write(struct file *filp, const char *buff, size_t count, loff_t *offp)
{
    ssize_t rv = 0;

    if(count > MAX_RW_COUNT)
    {
        print_err("max write size reached\n");
        return -ENOMEM;
    }

    if(!(access_ok(VERIFY_READ, buff, count)))
    {
        print_err("Invalid write buffer");
        return -ENOMEM;
    }

    rv = copy_from_user(dev.p_tx_buff, buff, count);

    if(rv == 0)
    {
        print_info("received %d bytes from user", sizeof(count));
    }
    else
    {
        print_err("error receiving data from user\n");
        kfree(dev.p_tx_buff);
        return -EFAULT;
    }

    dev.tx_len = count;
    dev.tx_pos = 0;
    atomic_set(&index_tx, 0);

    iowrite32(((( int )dev.p_tx_buff[dev.tx_pos]) << 8) + (( int )dev.p_tx_buff[dev.tx_pos + 1]),
              dev.pspi_addr + MCSPI_TX0);

    iowrite32(IRQ_EN_TXE, dev.pspi_addr + MCSPI_IRQENABLE);

    gpio_set_value(GPIO_CS, CS0_GPIO_EN);

    iowrite32(SPI_CH0_EN, dev.pspi_addr + MCSPI_CH0CTRL);

    if((rv = wait_event_interruptible(spi_tx_queue, (atomic_read(&index_tx)) > 0)) < 0)
    {
        print_err("wait_event_interruptible write error\n");
        kfree(dev.p_tx_buff);
        return rv;
    }

    gpio_set_value(GPIO_CS, CS0_GPIO_DS);

    iowrite32(SPI_CH0_DS, dev.pspi_addr + MCSPI_CH0CTRL);

    return count;
}


/**
 * @brief Driver open function
 *
 * @param inode Inode struct pointer
 * @param filp File struct pointer
 * @return int Return value
 */
static int spi_driver_open(struct inode *inode, struct file *filp)
{
    uint32_t count = 30;
    uint32_t rv;

    if((dev.p_rx_buff = ( char * )kmalloc(sizeof(uint32_t) * count, GFP_KERNEL)) == NULL)
    {
        print_err("kmalloc p_rx_buff\n");
        return -ENOMEM;
    }

    if((dev.p_tx_buff = ( char * )kmalloc(sizeof(uint32_t) * count, GFP_KERNEL)) == NULL)
    {
        print_err("kmalloc p_tx_buff\n");
        return -ENOMEM;
    }

    rv = mutex_lock_interruptible(&dev.mtx_lock);

    if(rv < 0)
    {
        print_err("mutex_lock_interruptible error\n");
    }

    return 0;
}


/**
 * @brief Driver close function
 *
 * @param inode Inode struct pointer
 * @param filp File struct pointer
 * @return int Return value
 */
static int spi_driver_close(struct inode *inode, struct file *filp)
{
    print_info("Close\n");

    mutex_unlock(&dev.mtx_lock); /* Free mutex */

    kfree(dev.p_rx_buff);
    kfree(dev.p_tx_buff);

    return 0;
}


/**
 * @brief Driver Init function
 *
 * @return int Return Value
 */
static int __init spi_driver_init(void)
{
    int status;

    if((status = alloc_chrdev_region(
          &spi_driver_dev, SPI_DRIVER_MINORBASE, SPI_DRIVER_MINORCOUNT, "spi_driver_td3")) < 0)
    {
        print_err("alloc_chrdev_region error\n");
        return (status);
    }

    p_cdev = cdev_alloc();
    p_cdev->ops = &spi_driver_dev_fops;
    p_cdev->owner = THIS_MODULE;
    p_cdev->dev = spi_driver_dev;

    if(cdev_add(p_cdev, spi_driver_dev, SPI_DRIVER_MINORCOUNT) == -1)
    {
        print_err("cdev error\n");
        unregister_chrdev_region(spi_driver_dev, SPI_DRIVER_MINORCOUNT);
        return (status);
    }

    if(IS_ERR((spi_driver_dev_class = class_create(THIS_MODULE, SPI_DRIVER_CLASS))))
    {
        print_err("class_create error\n");
        unregister_chrdev_region(spi_driver_dev, SPI_DRIVER_MINORCOUNT);
        return EFAULT;
    }

    if(IS_ERR((device_create(spi_driver_dev_class, NULL, spi_driver_dev, NULL, "spi_td3"))))
    {
        print_err("device_create error\n");
        class_destroy(spi_driver_dev_class);
        unregister_chrdev_region(spi_driver_dev, SPI_DRIVER_MINORCOUNT);
        return EFAULT;
    }

    print_info(
      "Device register correct. MAJOR:%d MINOR:%d\n", MAJOR(spi_driver_dev), MINOR(spi_driver_dev));

    status = platform_driver_register(&spi_driver_platform);

    if(status != 0)
    {
        cdev_del(p_cdev);
        device_destroy(spi_driver_dev_class, spi_driver_dev);
        class_destroy(spi_driver_dev_class);
        unregister_chrdev_region(spi_driver_dev, SPI_DRIVER_MINORCOUNT);
        print_err("platform_driver_register error\n");
        return (status);
    }

    return (status);
}


/**
 * @brief Driver exit function
 *
 */
static void __exit spi_driver_exit(void)
{
    mutex_destroy(&dev.mtx_lock);
    platform_driver_unregister(&spi_driver_platform);
    device_destroy(spi_driver_dev_class, spi_driver_dev);
    class_destroy(spi_driver_dev_class);
    cdev_del(p_cdev);
    unregister_chrdev_region(spi_driver_dev, SPI_DRIVER_MINORCOUNT);
}


/**
 * @brief Driver probe function
 *
 * @param pdev Platform device pointer
 * @return int Return Value
 */
static int spi_driver_probe(struct platform_device *pdev)
{
    uint32_t rv = 0;
    uint32_t reg_data = 0;
    uint32_t count = 0, aux = 0;
    const char *dt_compatible = NULL;
    unsigned int dt_reg[2];

    /* Get an irq for the device */
    dev.spi_irq_num = platform_get_irq(pdev, 0);

    if(dev.spi_irq_num == 0)
    {
        print_err("platform_get_irq error");
        platform_driver_unregister(&spi_driver_platform);
        device_destroy(spi_driver_dev_class, spi_driver_dev);
        class_destroy(spi_driver_dev_class);
        cdev_del(p_cdev);
        unregister_chrdev_region(spi_driver_dev, SPI_DRIVER_MINORCOUNT);
        return -1;
    }

    rv = request_irq(dev.spi_irq_num,
                     ( irq_handler_t )spi_driver_irq_handler,
                     IRQF_TRIGGER_RISING,
                     pdev->name,
                     NULL);

    if(rv != 0)
    {
        print_err("request_irq error");
        platform_driver_unregister(&spi_driver_platform);
        device_destroy(spi_driver_dev_class, spi_driver_dev);
        class_destroy(spi_driver_dev_class);
        cdev_del(p_cdev);
        unregister_chrdev_region(spi_driver_dev, SPI_DRIVER_MINORCOUNT);
        return -1;
    }

    mutex_init(&dev.mtx_lock);

    /* Read property of device tree */
    of_property_read_string(pdev->dev.of_node, "compatible", &dt_compatible);
    of_property_read_u32_array(pdev->dev.of_node, "reg", dt_reg, 2);

    print_info("Device Tree \"compatible\": %s\n", dt_compatible);
    print_info("Device Tree \"reg\": <0x%02x 0x%02x>\n", dt_reg[0], dt_reg[1]);

    /* Obtain base addresses */
    dev.pcm_per = ( unsigned char * )ioremap(CM_PER, 0x400);
    dev.pcontrol_module = ( unsigned char * )ioremap(CONTROL_MODULE, 0x2000);
    dev.pspi_addr = ( unsigned char * )ioremap(dt_reg[0], dt_reg[1]);

    if(!dev.pcm_per || !dev.pcontrol_module || !dev.pspi_addr)
    {
        print_err("error to obtain base addresses");
        return -ENOMEM;
    }

    /* Configure spi clock */
    reg_data = ioread32(dev.pcm_per + CM_PER_SPI0_CLKCTRL);
    reg_data |= 0x02;
    iowrite32(reg_data, dev.pcm_per + CM_PER_SPI0_CLKCTRL);

    /* Set pinmux */
    iowrite32(0x30, dev.pcontrol_module + PIN_SPI0_SCLK_OFFSET);
    iowrite32(0x30, dev.pcontrol_module + PIN_SPI0_D0_OFFSET);
    iowrite32(0x30, dev.pcontrol_module + PIN_SPI0_D1_OFFSET);
    iowrite32(0x30, dev.pcontrol_module + PIN_SPI0_CS0_OFFSET);

    /* Wait SPI reset */
    do
    {
        msleep(1);
        aux = ioread32(dev.pspi_addr + MCSPI_SYSSTATUS);
        if(count >= 4)
        {
            print_info("cant reset SPI0\n");
            return -EBUSY;
        }
        count++;
    } while(aux != SYS_STAT_RD);

    /* Disable channel */
    iowrite32(SPI_CH0_DS, dev.pspi_addr + MCSPI_CH0CTRL);

    /* Start channel configuration */
    reg_data = ioread32(dev.pspi_addr + MCSPI_SYSCONFIG);
    iowrite32(0x308 | reg_data, dev.pspi_addr + MCSPI_SYSCONFIG);

    reg_data = ioread32(dev.pspi_addr + MCSPI_MODULCTRL);
    iowrite32(0x02, dev.pspi_addr + MCSPI_MODULCTRL);

    reg_data = ioread32(dev.pspi_addr + MCSPI_MODULCTRL);
    iowrite32((~0x04) & reg_data, dev.pspi_addr + MCSPI_MODULCTRL);

    reg_data = ioread32(dev.pspi_addr + MCSPI_CH0CONF);
    iowrite32(0x607DF | reg_data, dev.pspi_addr + MCSPI_CH0CONF);

    reg_data = ioread32(dev.pspi_addr + MCSPI_CH0CONF);
    iowrite32(IRQ_STAT_TXS | IRQ_STAT_RXS, dev.pspi_addr + MCSPI_IRQSTATUS);

    /* Configure CS0 with GPIO because normal SPI_CS0 was not working */
    gpio_request(GPIO_CS, "spi_cs0_td3");
    gpio_direction_output(GPIO_CS, GPIO_OUTPUT);

    print_info("End of probe\n");

    return 0;
}

/**
 * @brief Driver remove function
 *
 * @param pdev Platform device pointer
 * @return int Return Value
 */
static int spi_driver_remove(struct platform_device *pdev)
{
    free_irq(dev.spi_irq_num, NULL);
    mutex_destroy(&dev.mtx_lock);
    return 0;
}


module_init(spi_driver_init);
module_exit(spi_driver_exit);