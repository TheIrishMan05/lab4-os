#ifndef VTFS_IOPS_H
#define VTFS_IOPS_H
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/dcache.h>

struct vtfs_dir_data;
struct dentry* vtfs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flag);
int vtfs_create(struct mnt_idmap *idmap, struct inode *parent_inode, struct dentry *child_dentry, umode_t mode, bool b);
int vtfs_unlink(struct inode *parent_inode, struct dentry *child_dentry);
struct dentry* vtfs_mkdir(struct mnt_idmap *idmap, struct inode *parent_inode, struct dentry *child_dentry, umode_t mode);
int vtfs_rmdir(struct inode *parent_inode, struct dentry *child_dentry);
int vtfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry);
int vtfs_getattr(struct mnt_idmap *idmap, const struct path *path, struct kstat *stat, u32 request_mask, unsigned int query_flags);

#endif