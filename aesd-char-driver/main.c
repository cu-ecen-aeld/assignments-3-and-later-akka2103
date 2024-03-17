/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */
 
 /*
 Referenec: kbiggs, logic to calculate the write length while realloc in write function
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Akash Karoshi"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;
    PDEBUG("open");
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release - nothing to release as there is no dynamic allocation in aesd_open
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    size_t bytes_to_copy = 0;
    ssize_t retval = 0;
    struct aesd_dev *dev = NULL;
    size_t offset = 0;
    struct aesd_buffer_entry *entry = NULL;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    // Check if file pointer is valid
    if ((filp == NULL) || (buf==NULL)) 
    {
        retval = -EINVAL;
        goto out;
    }

    // Retrieve device pointer from file's private data
    dev = filp->private_data;
    if (!dev) 
    {
        PDEBUG("private data assignment failed");
        retval = -EPERM;
        goto out;
    }

    // Lock the mutex before reading from the circular buffer
    if (mutex_lock_interruptible(&dev->mutex_lock)) 
    {
        PDEBUG("Mutex lock failed for read");
        retval = -ERESTARTSYS;
        goto out;
    }

    // Find the entry at the specified offset
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buf, *f_pos, &offset);
    if (!entry) 
    {
        // No data available at the specified offset
 	PDEBUG("No data available at the specified offset");
        retval = 0;
        goto out_unlock;
    }

    // Determine the number of bytes to copy
    bytes_to_copy = min(count, entry->size - offset);

    // Copy the data from the circular buffer to the user buffer
    if (copy_to_user(buf, entry->buffptr + offset, bytes_to_copy)) 
    {
        PDEBUG("copy buffer contents to user, failed");
        retval = -EFAULT;
        goto out_unlock;
    }

    // Update the file position
    *f_pos += bytes_to_copy;

    retval = bytes_to_copy;

out_unlock:
    mutex_unlock(&dev->mutex_lock);

out:
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    ssize_t write_len = 0;
    struct aesd_dev *dev = NULL;
    char *temp_buffer = NULL;

    // Check if file pointer and buffer are valid
    if ((filp==NULL) || (buf==NULL))
    {
        retval = -EINVAL; // Invalid argument
        goto out;
    }
    
    // Retrieve device pointer from file's private data
    dev = filp->private_data;
    if (!dev)
    {
        // Invalid device data
        retval = -EPERM;
        goto out_free_temp_buffer;
    }

    // Allocate memory to store the temporary buffer
    temp_buffer = kmalloc(count, GFP_KERNEL);
    if (!temp_buffer)
    {
        // Unable to allocate memory
        retval = -ENOMEM;
        goto out;
    }

    // Copy buffer from user space into kernel buffer
    if (copy_from_user(temp_buffer, buf, count))
    {
        // Unable to copy buffer from user space
        retval = -EFAULT;
        goto out_free_temp_buffer;
    }

    

    // Lock the mutex before writing to the circular buffer
    if (mutex_lock_interruptible(&dev->mutex_lock))
    {
        // Unable to lock mutex
        retval = -ERESTARTSYS;
        goto out_free_temp_buffer;
    }

    // Check for a newline character in the input buffer
    char *newline_position = memchr(temp_buffer, '\n', count);
    if (newline_position)
    {
        write_len = 1 + (newline_position - temp_buffer);
    }
    else
    {
        write_len = count;
    }

    // Reallocate memory for the working entry to accommodate new content
    dev->entry.buffptr = krealloc(dev->entry.buffptr,
                                           dev->entry.size + write_len,
                                           GFP_KERNEL);
    if (!dev->entry.buffptr)
    {
        // Unable to reallocate memory for new write command addition
        retval = -ENOMEM;
        goto out_unlock;
    }

    // Copy the most recent write buffer into the working entry
    memcpy(dev->entry.buffptr + dev->entry.size, temp_buffer, write_len);

    dev->entry.size += write_len;

    // If a newline character is found, add the entry to the circular buffer
    if (newline_position)
    {
        const char *ret_buf = NULL;

        // Add the entry to the circular buffer
        ret_buf = aesd_circular_buffer_add_entry(&dev->buf, &dev->entry);
        if (ret_buf)
        {
            // If more than 10 writes, free the oldest buffer
            kfree(ret_buf);
        }

        // Reset working entry
        dev->entry.size = 0;
        dev->entry.buffptr = NULL;
    }

    // Unlock the mutex
out_unlock:
    mutex_unlock(&dev->mutex_lock);

out_free_temp_buffer:
    kfree(temp_buffer);

out:
    // Return the number of bytes written
    retval = count;

    return retval;
}



struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    //aesd_device.buf.in_offs = 0;
    //aesd_device.buf.out_offs = 0;
    //aesd_device.buf.full = false;
    //aesd_device.entry.buffptr = NULL;
    //aesd_device.entry.size = 0;
    mutex_init(&aesd_device.mutex_lock);
    
    aesd_circular_buffer_init(&aesd_device.buf);

    result = aesd_setup_cdev(&aesd_device);

    if (result) 
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    uint8_t index;
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    struct aesd_buffer_entry *entry;

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific portions here as necessary
     */
    // Free the memory allocated for the working entry
    //kfree(aesd_device.entry.buffptr);

    // Free the memory allocated for circular buffer entries
    
    
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buf, index) 
    {
        kfree(entry->buffptr);
    }

    // Destroy the mutex
    mutex_destroy(&aesd_device.mutex_lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
