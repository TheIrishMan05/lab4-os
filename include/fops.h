#ifndef VTFS_FOPS_H
#define VTFS_FOPS_H


#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/dcache.h>

int vtfs_iterate(struct file* filp, struct dir_context* ctx);
ssize_t vtfs_read(struct kiocb *iocb, struct iov_iter *to);
ssize_t vtfs_write(struct kiocb *iocb, struct iov_iter *from);

#endif
