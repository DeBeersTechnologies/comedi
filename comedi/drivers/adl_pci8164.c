/*
    comedi/drivers/adl_pci8164.c

    Hardware comedi driver fot PCI-8164 Adlink card
    Copyright (C) 2004 Michel Lachine <mike@mikelachaine.ca>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
/*
Driver: adl_pci8164.o
Description: Driver for the Adlink PCI-8164 4 Axes Motion Control board
Devices: [ADLink] PCI-8164 (pci8164)
Author: Michel Lachaine <mike@mikelachaine.ca>
Status: experimental

Configuration Options:
  none
*/

#include <linux/comedidev.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include "comedi_fc.h"
#include "8253.h"

#define PCI8164_AXIS_X  0x00
#define PCI8164_AXIS_Y  0x08
#define PCI8164_AXIS_Z  0x10
#define PCI8164_AXIS_U  0x18

#define PCI8164_MSTS	0x00
#define PCI8164_SSTS    0x02
#define PCI8164_BUF0    0x04
#define PCI8164_BUF1    0x06

#define PCI8164_CMD     0x00
#define PCI8164_OTP     0x02

#define PCI_DEVICE_ID_PCI8164 0x8164

typedef struct {
	char *name;
	int  vendor_id;
	int  device_id;
} adl_pci8164_board;

static adl_pci8164_board adl_pci8164_boards[] = {
	{ "pci8164", PCI_VENDOR_ID_ADLINK, PCI_DEVICE_ID_PCI8164 },
};

static struct pci_device_id adl_pci8164_pci_table[] __devinitdata = {
	{ PCI_VENDOR_ID_ADLINK, PCI_DEVICE_ID_PCI8164, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, adl_pci8164_pci_table);

#define thisboard ((adl_pci8164_board *)dev->board_ptr)

typedef struct{
	int data;
	struct pci_dev *pci_dev;
} adl_pci8164_private;

#define devpriv ((pci8164_private *)dev->private)

static int adl_pci8164_attach(comedi_device *dev,comedi_devconfig *it);
static int adl_pci8164_detach(comedi_device *dev);
static comedi_driver driver_adl_pci8164={
	driver_name:	"adl_pci8164",
	module:		THIS_MODULE,
	attach:		adl_pci8164_attach,
	detach:		adl_pci8164_detach,
	num_names:  1,
	board_name: adl_pci8164_boards,
	offset:     sizeof(adl_pci8164_board),
};

static int adl_pci8164_insn_read_msts(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data);

static int adl_pci8164_insn_read_ssts(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data);

static int adl_pci8164_insn_read_buf0(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data);

static int adl_pci8164_insn_read_buf1(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data);

static int adl_pci8164_insn_write_cmd(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data);

static int adl_pci8164_insn_write_otp(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data);

static int adl_pci8164_insn_write_buf0(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data);

static int adl_pci8164_insn_write_buf1(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data);

static int adl_pci8164_attach(comedi_device *dev,comedi_devconfig *it)
{
	struct pci_dev *pcidev;
	comedi_subdevice *s;

	printk("comedi: attempt to attach...\n");
	printk("comedi%d: adl_pci8164: board=%s\n",dev->minor, thisboard->name);

  	dev->board_name = thisboard->name;

	if(alloc_private(dev,sizeof(adl_pci8164_private))<0)
		return -ENOMEM;

	if(alloc_subdevices(dev, 4)<0)
		return -ENOMEM;

	for(pcidev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, NULL); pcidev != NULL;
		pcidev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, pcidev))
	{

		if ( pcidev->vendor == PCI_VENDOR_ID_ADLINK &&
		     pcidev->device == PCI_DEVICE_ID_PCI8164 ) {
			dev->iobase = pci_resource_start ( pcidev, 2 );
			printk ( "comedi: base addr %4lx\n", dev->iobase );

			dev->board_ptr = adl_pci8164_boards + 0;

			s=dev->subdevices+0;
			s->type = COMEDI_SUBD_PROC;
			s->subdev_flags = SDF_READABLE|SDF_WRITABLE;
			s->n_chan = 4;
			s->maxdata = 0xffff;
			s->len_chanlist = 4;
			//s->range_table = &range_axis;
			s->insn_read = adl_pci8164_insn_read_msts;
			s->insn_write = adl_pci8164_insn_write_cmd;

			s=dev->subdevices+1;
			s->type = COMEDI_SUBD_PROC;
			s->subdev_flags = SDF_READABLE|SDF_WRITABLE;
			s->n_chan = 4;
			s->maxdata = 0xffff;
			s->len_chanlist = 4;
			//s->range_table = &range_axis;
			s->insn_read = adl_pci8164_insn_read_ssts;
			s->insn_write = adl_pci8164_insn_write_otp;

			s=dev->subdevices+2;
			s->type = COMEDI_SUBD_PROC;
			s->subdev_flags = SDF_READABLE|SDF_WRITABLE;
			s->n_chan = 4;
			s->maxdata = 0xffff;
			s->len_chanlist = 4;
			//s->range_table = &range_axis;
			s->insn_read = adl_pci8164_insn_read_buf0;
			s->insn_write = adl_pci8164_insn_write_buf0;

			s=dev->subdevices+3;
			s->type = COMEDI_SUBD_PROC;
			s->subdev_flags = SDF_READABLE|SDF_WRITABLE;
			s->n_chan = 4;
			s->maxdata = 0xffff;
			s->len_chanlist = 4;
			//s->range_table = &range_axis;
			s->insn_read = adl_pci8164_insn_read_buf1;
			s->insn_write = adl_pci8164_insn_write_buf1;

		}
	}

	printk("comedi: attached\n");

	return 1;
}


static int adl_pci8164_detach(comedi_device *dev)
{
	printk("comedi%d: pci8164: remove\n",dev->minor);

	return 0;
}

static int adl_pci8164_insn_read_msts(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data)
{
  int axis, axis_reg;
  char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis)
	{
      		case 0: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
			break;
      		case 1: axis_reg = PCI8164_AXIS_Y;
			axisname = "Y";
			break;
      		case 2: axis_reg = PCI8164_AXIS_Z;
			axisname = "Z";
			break;
      		case 3: axis_reg = PCI8164_AXIS_U;
			axisname = "U";
			break;
      		default: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
	}

	data[0] = inw(dev->iobase + axis_reg + PCI8164_MSTS);
	printk("comedi: pci8164 MSTS read -> %04X:%04X on axis %s\n", data[0], data[1], axisname);

	return 2;
}

static int adl_pci8164_insn_read_ssts(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data)
{
	int axis, axis_reg;
	char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis)
	{
      		case 0: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
			break;
      		case 1: axis_reg = PCI8164_AXIS_Y;
			axisname = "Y";
			break;
      		case 2: axis_reg = PCI8164_AXIS_Z;
			axisname = "Z";
			break;
      		case 3: axis_reg = PCI8164_AXIS_U;
			axisname = "U";
			break;
      		default: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
	}

	data[0] = inw(dev->iobase + axis_reg + PCI8164_SSTS);
  	printk("comedi: pci8164 SSTS read -> %04X:%04X on axis %s\n", data[0], data[1], axisname);

	return 2;
}

static int adl_pci8164_insn_read_buf0(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data)
{
	int axis, axis_reg;
	char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis)
	{
      		case 0: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
			break;
      		case 1: axis_reg = PCI8164_AXIS_Y;
			axisname = "Y";
			break;
      		case 2: axis_reg = PCI8164_AXIS_Z;
			axisname = "Z";
			break;
      		case 3: axis_reg = PCI8164_AXIS_U;
			axisname = "U";
			break;
      		default: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
	}

	data[0] = inw(dev->iobase + axis_reg + PCI8164_BUF0);
	printk("comedi: pci8164 BUF0 read -> %04X:%04X on axis %s\n", data[0], data[1], axisname);

	return 2;
}

static int adl_pci8164_insn_read_buf1(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data)
{
	int axis, axis_reg;

	char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis)
	{
      		case 0: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
			break;
      		case 1: axis_reg = PCI8164_AXIS_Y;
			axisname = "Y";
			break;
      		case 2: axis_reg = PCI8164_AXIS_Z;
			axisname = "Z";
			break;
      		case 3: axis_reg = PCI8164_AXIS_U;
			axisname = "U";
			break;
      		default: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
	}

	data[0] = inw(dev->iobase + axis_reg + PCI8164_BUF1);
	printk("comedi: pci8164 BUF1 read -> %04X:%04X on axis %s\n", data[0], data[1], axisname);

	return 2;
}

static int adl_pci8164_insn_write_cmd(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data)
{
	unsigned int axis, axis_reg;

	char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis)
	{
      		case 0: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
			break;
      		case 1: axis_reg = PCI8164_AXIS_Y;
			axisname = "Y";
			break;
      		case 2: axis_reg = PCI8164_AXIS_Z;
			axisname = "Z";
			break;
      		case 3: axis_reg = PCI8164_AXIS_U;
			axisname = "U";
			break;
      		default: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
	}

  	outw(data[0], dev->iobase + axis_reg + PCI8164_CMD);
    	printk("comedi: pci8164 CMD write -> %04X:%04X on axis %s\n", data[0], data[1], axisname);

	return 2;
}

static int adl_pci8164_insn_write_otp(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data)
{
	int axis, axis_reg;

	char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis)
	{
      		case 0: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
			break;
      		case 1: axis_reg = PCI8164_AXIS_Y;
			axisname = "Y";
			break;
      		case 2: axis_reg = PCI8164_AXIS_Z;
			axisname = "Z";
			break;
      		case 3: axis_reg = PCI8164_AXIS_U;
			axisname = "U";
			break;
      		default: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
	}

	outw(data[0], dev->iobase + axis_reg + PCI8164_OTP);
  	printk("comedi: pci8164 OTP write -> %04X:%04X on axis %s\n", data[0], data[1], axisname);

	return 2;
}

static int adl_pci8164_insn_write_buf0(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data)
{
	int axis, axis_reg;

	char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis)
	{
      		case 0: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
			break;
      		case 1: axis_reg = PCI8164_AXIS_Y;
			axisname = "Y";
			break;
      		case 2: axis_reg = PCI8164_AXIS_Z;
			axisname = "Z";
			break;
      		case 3: axis_reg = PCI8164_AXIS_U;
			axisname = "U";
			break;
      		default: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
	}

	outw(data[0], dev->iobase + axis_reg + PCI8164_BUF0);
	printk("comedi: pci8164 BUF0 write -> %04X:%04X on axis %s\n", data[0], data[1], axisname);

  	return 2;
}

static int adl_pci8164_insn_write_buf1(comedi_device *dev,comedi_subdevice *s,comedi_insn *insn, lsampl_t *data)
{
	int axis, axis_reg;

	char *axisname;

	axis = CR_CHAN(insn->chanspec);

	switch (axis)
	{
      		case 0: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
			break;
      		case 1: axis_reg = PCI8164_AXIS_Y;
			axisname = "Y";
			break;
      		case 2: axis_reg = PCI8164_AXIS_Z;
			axisname = "Z";
			break;
      		case 3: axis_reg = PCI8164_AXIS_U;
			axisname = "U";
			break;
      		default: axis_reg = PCI8164_AXIS_X;
			axisname = "X";
	}

  	outw(data[0], dev->iobase + axis_reg + PCI8164_BUF1);
    	printk("comedi: pci8164 BUF1 write -> %04X:%04X on axis %s\n", data[0], data[1], axisname);

	return 2;
}

COMEDI_INITCLEANUP(driver_adl_pci8164);