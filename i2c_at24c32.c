#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/i2c.h>
#include<linux/fs.h>
#include<linux/slab.h>
#include<linux/delay.h>
#include<linux/jiffies.h>
#include<asm/uaccess.h>

struct at24_prv {
	struct i2c_client client_prv;
	struct kobject *at24_kobj;
	char *ptr;
	unsigned int size;			//size of eeprom
	unsigned int pagesize;			//pagesize
	unsigned int address_width;		//address width
	unsigned int page_no;			//pageno
};

struct at24_prv *prv=NULL;

static unsigned read_limit = 32;
static unsigned write_max = 32;
static unsigned write_timeout = 25;

//read logic for reading from the device
static ssize_t at24_eeprom_read(struct i2c_client *client, char *buf, unsigned offset, size_t count){
	struct i2c_msg msg[2];
	u8 msgbuf[2];
	unsigned long timeout, read_time;
	int status;
	
	memset(msg,0,sizeof(msg));
	memset(msgbuf, '\0', sizeof(msgbuf));
	if (count>read_limit)
		count=read_limit;

	//sending dev offset as per device standart MSB first
        msgbuf[0] = offset >> 8;
        msgbuf[1] = offset;

	//device addr for transfer
        msg[0].addr = client->addr;

	//flag 0 means we are writing from device
        msg[0].flags = 0;
        msg[0].buf = msgbuf;		//read address

        msg[0].len = 2;

        msg[1].addr = client->addr;
        msg[1].flags = 1;		//flag 1 means we are reading from device
        msg[1].buf = buf;		//buf where device should put the readed data
        msg[1].len = count;		//no. of bytes readed

	//performing read ops.
        timeout = jiffies + msecs_to_jiffies(write_timeout);
        do {
                read_time = jiffies;
		
		//main api to transfer to i2c client
                status = i2c_transfer(client->adapter, msg, 2);
                if (status == 2)
                        status = count;
                if (status == count) {
                        return count;
                }
                msleep(1);
        } while (time_before(read_time, timeout));	//if cannot read in a specfic time them something went wrong

        return -ETIMEDOUT;



}

//write logic to transfer to device
static ssize_t at24_eeprom_write(struct i2c_client *client,const char *buf, unsigned offset, size_t count){
	struct i2c_msg msg;
	unsigned long timeout, write_time;
	ssize_t status;
	char msgbuf[40];

	memset(msgbuf, '\0', sizeof(msgbuf));
	if (count>=write_max)
		count=write_max;
	
	//init device buf with req info
	msg.addr=prv->client_prv;
	msg.buf=msgbuf;
	msg.buf[0]=(offset>>8);
	msg.buf[1]=offset;

	memcpy(&msg.buf[2],buf,count);
	msg.len=count+2;

	//performing write ops.
	timeout = jiffies + msecs_to_jiffies(write_timeout);
	do{
		write_time=jiffies;
		status=i2c_transfer(client->adaptor,&msg,1);
		if (status==1)
			status=count;
		if (status==count)
		return count;
		msleep(1);
	}while(time_before(write_time,timeout));	//if cannot read in a specfic time them something went wrong

	return -ETIMEDOUT;
}

//sysfs read func
static ssize_t at24_sys_read(struct kobject *obj, struct kobj_attribute *attr, char *buff){
	ssize_t ret;

	loff_t off=prv->page_no*32;

	//internally calling the wrapper func
	ret=at24_eeprom_read(&prv->client,buff,(int)off,prv->pagesize);
	       if (ret < 0) {
                pr_info("error in reading\n");
                return ret;
        }

        return prv->pagesize;

}

//sysfd write func!!!and data is read from device priv func
static ssize_t at24_sys_write(struct kobject *obj, struct kobj_attribute *attr, const char *buff, size_t count){
	ssize_t ret;

	loff_t off=prv->page_no*32;
	if (count>write_max){
	pr_info("Write length must be <=32");
	return -EFBIG;
	}
	
	//internally calling the wrapper func
	ret=at24_eeprom_write(&prv->client_prv,buff,(int)off,count);
	if (ret<0)
	{
	pr_info("Failed to write to the device\n");
	return ret;
	}

	return count;
}

//sysfs get offset func !!and data is readed from device priv func
static ssize_t at24_get_offset(struct kobject *obj, struct kobj_attribute *attr, char *buff){
	pr_info("Page:%d\n",prv->page_no);
	return strlen(buf);
}

//sysfs set offset func	!! and data is set into device priv func
static ssize_t at24_set_offset(struct kobject *obj, struct kobj_attribute *attr, const char *buff, size_t count){
	unsigned int long temp;

	if (!kstrtol(buff,16,&temp)){
		if (temp<128){
		prv->page_no=temp;
		pr_info("Page:%d\n",prv->page_no);
		}
		else
		pr_info("only 128 pages are available!!use a vailed no");
	}

	return count;
}

//kobject attribute for creating file
static struct kobj_attribute at24_rw = __ATTR("at24_wr",0660,at24_sys_read,at24_sys_write);
static struct kobj_attribute at24_offset = __ATTR("at24_offset",0660, at24_get_offset,at24_set_offset);

static struct attribute *attrs[] = {
	&at24_rw.attr,
	&at24_offset.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs=attrs,
};

static int at24_probe(struct i2c_client *client, const struct i2c_device_id *id){
int ret;

	pr_info("probe func of at24_probe got invoked\n");

	//allocating memory for private struct
	prv=(struct at24_prv *)kzalloc(sizeof(struct at24_prv),GFP_KERNEL);
	if (prv==NULL){
	pr_info("requested memory not allocated\n");
	return -1;
	}

	prv->client_prv=*client;

	//reterving device properties
	if (device_property_read_u32(&client->dev,"size",&prv->size)!=0){
	dev_error(&client->dev, "Error missing \"size\" property\n");
	return -ENODEV;
	}

	if (device_property_read_u32(&client->dev,"pagesize",&prv->pagesize)!=0){
	dev_error(&client->dev, "Error missing \"pagesize\" property\n");
	return -ENODEV;
	}

	if (device_property_read_u32(&client->dev,"address-width",&prv->pagesize)!=0)
	{
	dev_error(&client->dev, "Error missing \"pagesize\" property\n");
	return -ENODEV;
	}

	//creating a kobject in the sysfs
	prv->at24_kobj=kobject_create_and_add("at24_eeprom",NULL);
	if(!prv->at24_kobj)
		return -ENOMEM;

	//init. kobj with attr for driver ops
	ret=sysfs_create_group(prv->at24_kobj,&attr_group);
	if (ret)
		kobject_put(prv->at24_kobj);

        pr_info("Sysfs entry created for AT24C32 device \n");
        pr_info("       SIZE            :%d\n", prv->size);
        pr_info("       PAGESIZE        :%d\n", prv->pagesize);
        pr_info("       address-width   :%d\n", prv->address_width);

return 0;
}

//invoked when module is removed
static int at24_remove(struct i2c_client *client){
	//reduce the reference count to kobj
	kobject_del(prv->at24_kobj);
	kfree(prv);	

	pr_info("at24_removed\n");	

	return 0;
}

//device id which is matched to device tree entry structue found in the Driver Bus layer(core layer)
static struct i2c_device_id at24_id[] = {
	{ "24c32", 0x50 }
	{},
};
//I2C driver structue 
static struct i2c_driver at24_driver = {
	.driver = {
		.name="at24",
		.owner=THIS_MODULE,
	},
	.probe=at24_probe,
	.remove=at24_remove,
	.id_device=at24_driver,	
};

module_i2c_driver(at24_driver);

MODULE_AUTHOR("jitenderanga@gmail.com");
MODULE_DESCRIPTION("I2C driver for at24c32");
MODULE_LICENCE("GPL");
