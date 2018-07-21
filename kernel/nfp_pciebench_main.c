/*
 * Copyright (C) 2015 Rolf Neugebauer. All rights reserved.
 * Copyright (C) 2015 Netronome Systems, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * A full copy of the GNU General Public License version 2 is
 * available at:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html.
 */

/*
 * This kernel module allocates and DMA maps buffers for use with the
 * PCIe micro-benchmarks implemented on the NFP.
 *
 * For the test we need a largish region of host memory to DMA to.
 * For systems with DDIO, the region needs to be larger than the last
 * level cache.  However, on most linux kernels one can only allocate
 * up to 4MB of physically contiguous memory.  We therefor allocate
 * and DMA map a number of physically contiguous memory in
 * @NFP_PCIEBENCH_CHUNKSZ chunks.  Memory is explicitly allocated from
 * a specified NUMA mode to allow measurements of DMA access to
 * "remote" memory.
 *
 * For kernel version >= 3.5 we could use CMA (Contiguous Memory Allocator).
 *
 * A procfs interface is exported to allow a userspace app to extract
 * the DMA addresses of each chunk as well as total memory
 * available. Another procfs interface is provided allowing userspace
 * to read/write to the buffer.  Userspace can use this for debugging
 * and try to warm the caches with the buffer contents.
 *
 * The buffers are DMA mapped to the NFP PCI device.  We obtain the
 * device handle by calling into the main NFP PCI device driver.
 *
 * This module is kept as simple as possible.  We make no attempt to
 * control concurrency or other such things.  It's up to the user to
 * ensure that only one user is using it at a time.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>


#include "nfpcore/nfp.h"
#include "nfpcore/nfp_cpp.h"
#include "nfpcore/nfp_dev_cpp.h"
#include "nfpcore/nfp3200_pcie.h"
#include "nfpcore/nfp6000_pcie.h"


static const char npb_driver_name[] = "nfp-pciebench";
static const char npb_driver_version[] = "0.2";
static const struct pci_device_id npb_pci_device_ids[] = {
	{ PCI_VENDOR_ID_NETRONOME, PCI_DEVICE_NFP4000,
	  PCI_VENDOR_ID_NETRONOME, PCI_ANY_ID,
	  PCI_ANY_ID, 0,
	},
	{ PCI_VENDOR_ID_NETRONOME, PCI_DEVICE_NFP6000,
	  PCI_VENDOR_ID_NETRONOME, PCI_ANY_ID,
	  PCI_ANY_ID, 0,
	},
	{ PCI_VENDOR_ID_NETRONOME, PCI_DEVICE_NFP3200,
	  PCI_VENDOR_ID_NETRONOME, PCI_DEVICE_NFP3200,
	  PCI_ANY_ID, 0,
	},
	{ PCI_VENDOR_ID_NETRONOME, PCI_DEVICE_NFP3200,
	  PCI_VENDOR_ID_NETRONOME, PCI_DEVICE_NFP3240,
	  PCI_ANY_ID, 0,
	},
	{ 0, } /* Required last entry. */
};

MODULE_DEVICE_TABLE(pci, npb_pci_device_ids);
MODULE_AUTHOR("Rolf Neugebauer <rolf.neugebauer@netronome.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NFP PCIe benchmarking driver");

/*
 * Module parameters
 */
static int node;
module_param(node, int, 0);
MODULE_PARM_DESC(node, "NUMA Node index to allocate memory from");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
#ifndef __devinit
#define __devinit
#endif
#ifndef __devexit
#define __devexit
#endif
#ifndef __devexit_p
#define __devexit_p(x) x
#endif
#endif

/*
 * Macros for host buffers. These need to be kept in sync with the ME code
 */
#define NFP_PCIEBENCH_MAX_MEM (64 * 1024 * 1024)
#define NFP_PCIEBENCH_CHUNK_SZ (4 * 1024 * 1024)
#define NFP_PCIEBENCH_CHUNK_PO (10)  /* PAGE ORDER (assuming 4K pages) */
#define NFP_PCIEBENCH_CHUNKS (NFP_PCIEBENCH_MAX_MEM / NFP_PCIEBENCH_CHUNK_SZ)

/*
 * Names for procfs entries
 */
#define NFP_PCIEBENCH_PROC_DMA_ADDRS  "pciebench_dma_addrs-%d"
#define NFP_PCIEBENCH_PROC_BUF_SZ     "pciebench_buf_sz-%d"
#define NFP_PCIEBENCH_PROC_BUFFER     "pciebench_buffer-%d"

/*
 * Global state
 */
struct nfp_pciebench {
	struct pci_dev *pdev;
	struct nfp_cpp *cpp;
	struct platform_device *nfp_dev_cpp;

	void *buf[NFP_PCIEBENCH_CHUNKS];
	dma_addr_t buf_dma_addrs[NFP_PCIEBENCH_CHUNKS];
	int id;

	struct proc_dir_entry *proc_dma_addrs;
	struct proc_dir_entry *proc_buf_sz;
	struct proc_dir_entry *proc_buffer;	
};

/*
 * procfs interface for userspace to get the DMA addresses
 */
static int npb_dma_addrs_show(struct seq_file *m, void *v)
{
	struct nfp_pciebench *npb = (struct nfp_pciebench *)m->private;
	int i;

	for (i = 0; i < NFP_PCIEBENCH_CHUNKS; i++)
		seq_printf(m, "0x%llx\n", npb->buf_dma_addrs[i]);

	return 0;
}

static int npb_dma_addrs_open(struct inode *inode, struct  file *file)
{
	struct nfp_pciebench *npb = PDE_DATA(inode);
	return single_open(file, npb_dma_addrs_show, npb);
}

static const struct file_operations npb_dma_addrs_fops = {
	.owner = THIS_MODULE,
	.open = npb_dma_addrs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * procfs interface for userspace to get buffer size
 */
static int npb_buf_sz_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", NFP_PCIEBENCH_MAX_MEM);
	return 0;
}

static int npb_buf_sz_open(struct inode *inode, struct  file *file)
{
	return single_open(file, npb_buf_sz_show, NULL);
}

static const struct file_operations npb_buf_sz_fops = {
	.owner = THIS_MODULE,
	.open = npb_buf_sz_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * procfs interface to read/write the buffer
 */
static int npb_buf_open(struct inode *inode, struct file *file)
{
	struct nfp_pciebench *npb = PDE_DATA(inode);
	file->private_data = npb;
	return 0;
}

static int npb_buf_release(struct inode *inode, struct file *file)
{
	return 0;
};

static ssize_t npb_buf_op(struct file *file, char __user *buf,
			  size_t count, loff_t *offp, int write)
{
	struct nfp_pciebench *npb = file->private_data;
	ssize_t ret = 0;
	int chunk_idx;
	int chunk_off;
	void __user *udata;
	void *ldata;
	int pos, len;
	long err;

	if (count == 0)
		return 0;

	if (*offp >= NFP_PCIEBENCH_MAX_MEM)
		return 0;

	if (*offp + count > NFP_PCIEBENCH_MAX_MEM)
		count = NFP_PCIEBENCH_MAX_MEM - *offp;

	for (pos = 0; pos < count; pos += len) {
		chunk_idx = (*offp + pos) / NFP_PCIEBENCH_CHUNK_SZ;
		chunk_off = (*offp + pos) % NFP_PCIEBENCH_CHUNK_SZ;

		len = count - pos;
		if  (chunk_off + len > NFP_PCIEBENCH_CHUNK_SZ)
			len = NFP_PCIEBENCH_CHUNK_SZ - chunk_off;

		udata = (void __user *)buf + pos;
		ldata = (void *)npb->buf[chunk_idx] + chunk_off;

		if (write)
			err = copy_from_user(ldata, udata, len);
		else
			err = copy_to_user(udata, ldata, len);
		if (err) {
			ret = -EFAULT;
			goto err_out;
		}
	}

	*offp += pos;
	ret = pos;

err_out:
	return ret;
}

static ssize_t npb_buf_read(struct file *file, char __user *buf,
			    size_t count, loff_t *offp)
{
	return npb_buf_op(file, buf, count, offp, 0);
}

static ssize_t npb_buf_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *offp)
{
	return npb_buf_op(file, (char __user *)buf, count, offp, 1);
}

static const struct file_operations npb_buf_fops = {
	.owner          = THIS_MODULE,
	.open           = npb_buf_open,
	.release        = npb_buf_release,
	.read           = npb_buf_read,
	.write          = npb_buf_write,
};


static void npb_remove(struct nfp_pciebench *npb)
{
	int i;

	if (npb->proc_buffer)
		proc_remove(npb->proc_buffer);
	if (npb->proc_buf_sz)
		proc_remove(npb->proc_buf_sz);
	if (npb->proc_dma_addrs)
		proc_remove(npb->proc_dma_addrs);

	for (i = 0; i < NFP_PCIEBENCH_CHUNKS; i++) {
		if (npb->buf_dma_addrs[i])
			dma_unmap_single(&npb->pdev->dev, npb->buf_dma_addrs[i],
					 NFP_PCIEBENCH_CHUNK_SZ,
					 DMA_BIDIRECTIONAL);

		if (npb->buf[i])
			free_pages((unsigned long)npb->buf[i],
				   NFP_PCIEBENCH_CHUNK_PO);
	}
}

static int npb_init(struct nfp_pciebench *npb)
{
	struct proc_dir_entry *pe;
	struct page *pages;
	char buf[128];
	gfp_t flags;
	int i, j;
	int id;
	u32 *tmp;
	int err;

	flags = GFP_KERNEL;
	for (i = 0; i < NFP_PCIEBENCH_CHUNKS; i++) {
		/* TODO: node index should be based on system topology */
		if (node < 0 || node > 1)
			node = 0;

		/* Allocating memory from node */
		pages = alloc_pages_node(node, flags, NFP_PCIEBENCH_CHUNK_PO);
		npb->buf[i] = page_address(pages);
		if (!npb->buf[i]) {
			pr_err("Failed to allocate chunk %d on node %d",
			       i, node);
			err = -ENOMEM;
			goto err;
		}
		/* DMA map the pages */
		npb->buf_dma_addrs[i] = dma_map_single(
			&npb->pdev->dev, npb->buf[i], NFP_PCIEBENCH_CHUNK_SZ,
			DMA_BIDIRECTIONAL);
		if (!npb->buf_dma_addrs[i]) {
			pr_err("Failed to map chunk %d", i);
			err = -ENOMEM;
			goto err;
		}

		tmp = npb->buf[i];
		for (j = 0; j < NFP_PCIEBENCH_CHUNK_SZ / sizeof(u32); j++)
			tmp[j] = 0xc0de0000 + j;
	}

	id = nfp_cpp_device_id(npb->cpp);

	/* Create proc interfaces */
	scnprintf(buf, sizeof(buf), NFP_PCIEBENCH_PROC_DMA_ADDRS, id);
	pe = proc_create_data(buf, 0, NULL, &npb_dma_addrs_fops, npb);
	if (!pe) {
		pr_err("Failed to create dma_addr entry");
		err = -ENODEV;
		goto err;
	}
	npb->proc_dma_addrs = pe;

	scnprintf(buf, sizeof(buf), NFP_PCIEBENCH_PROC_BUF_SZ, id);
	pe = proc_create_data(buf, 0, NULL, &npb_buf_sz_fops, npb);
	if (!pe) {
		pr_err("Failed to create buf_sz entry");
		err = -ENODEV;
		goto err;
	}
	npb->proc_buf_sz = pe;

	scnprintf(buf, sizeof(buf), NFP_PCIEBENCH_PROC_BUFFER, id);
	pe = proc_create_data(buf, 0, NULL, &npb_buf_fops, npb);
	if (!pe) {
		pr_err("Failed to create buffer entry");
		err = -ENODEV;
		goto err;
	}
	npb->proc_buffer = pe;
	return 0;

err:
	npb_remove(npb);
	return err;
}

/*
 * PCI device functions
 */
static int npb_pci_probe(struct pci_dev *pdev,
			 const struct pci_device_id *pci_id)
{
	struct platform_device *dev_cpp = NULL;
	struct nfp_pciebench *npb;
	struct nfp_cpp *cpp;
	int err;

	err = pci_enable_device(pdev);
	if (err < 0)
		return err;

	pci_set_master(pdev);

	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err < 0) {
		dev_err(&pdev->dev, "Cannot set DMA mask\n");
		goto err_dma_mask;
	}

	err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err < 0) {
		dev_err(&pdev->dev, "Cannot set consistent DMA mask\n");
		goto err_dma_mask;
	}

	err = pci_request_regions(pdev, npb_driver_name);
	if (err < 0) {
		dev_err(&pdev->dev, "Unable to reserve pci resources.\n");
		goto err_request_regions;
	}

	switch (pdev->device) {
	case PCI_DEVICE_NFP3200:
		cpp = nfp_cpp_from_nfp3200_pcie(pdev, -1);
		break;
	case PCI_DEVICE_NFP4000:
	case PCI_DEVICE_NFP6000:
		cpp = nfp_cpp_from_nfp6000_pcie(pdev, -1);
		break;
	default:
		err = -ENODEV;
		goto err_nfp_cpp;
	}

	if (IS_ERR_OR_NULL(cpp)) {
		err = PTR_ERR(cpp);
		if (err >= 0)
			err = -ENOMEM;
		goto err_nfp_cpp;
	}

	dev_cpp = nfp_platform_device_register(cpp, NFP_DEV_CPP_TYPE);
	if (!dev_cpp) {
		dev_err(&pdev->dev, "Failed to enable user space access.");
		err = - ENODEV;
		goto err_reg_dev;
	}
	

	npb = kzalloc(sizeof(*npb), GFP_KERNEL);
	if (!npb) {
		err = -ENOMEM;
		goto err_kzalloc;
	}

	npb->pdev = pdev;
	npb->cpp = cpp;
	npb->nfp_dev_cpp = dev_cpp;

	err = npb_init(npb);
	if (err)
		goto err_init;

	pci_set_drvdata(pdev, npb);

	return 0;

err_init:
	kfree(npb);
err_kzalloc:
	nfp_platform_device_unregister(dev_cpp);
err_reg_dev:
	nfp_cpp_free(cpp);
err_nfp_cpp:
	pci_release_regions(pdev);
err_request_regions:
err_dma_mask:
	pci_disable_device(pdev);
	return err;
}

static void npb_pci_remove(struct pci_dev *pdev)
{
	struct nfp_pciebench *npb = pci_get_drvdata(pdev);

	npb_remove(npb);

	nfp_platform_device_unregister(npb->nfp_dev_cpp);
	nfp_cpp_free(npb->cpp);

	kfree(npb);

	pci_set_drvdata(pdev, NULL);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}


static struct pci_driver npb_pci_driver = {
	.name        = npb_driver_name,
	.id_table    = npb_pci_device_ids,
	.probe       = npb_pci_probe,
	.remove      = npb_pci_remove,
};

static int __init nfp_pciebench_init(void)
{
	int err;

	pr_info("%s: NFP PCIe benchmark driver\n", npb_driver_name);

	err = nfp_cppcore_init();
	if (err < 0)
		goto fail_cppcore_init;

	err = nfp_dev_cpp_init();
	if (err < 0)
		goto fail_dev_cpp_init;

	err = pci_register_driver(&npb_pci_driver);
	if (err < 0)
		goto fail_pci_init;

	return err;

fail_pci_init:
	nfp_dev_cpp_exit();
fail_dev_cpp_init:
	nfp_cppcore_exit();
fail_cppcore_init:
	return err;
}


static void __exit nfp_pciebench_exit(void)
{
	pci_unregister_driver(&npb_pci_driver);
	nfp_dev_cpp_exit();
	nfp_cppcore_exit();
}

module_init(nfp_pciebench_init);
module_exit(nfp_pciebench_exit);

/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
