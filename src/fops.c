#include "../include/fops.h"
#include "../include/vtfs.h"
#include <linux/uio.h>

struct file_operations vtfs_dir_ops = {
  .iterate_shared = vtfs_iterate,
};

struct file_operations vtfs_file_ops = {
  .read_iter = vtfs_read,
  .write_iter = vtfs_write,
};

int vtfs_iterate(struct file* filp, struct dir_context* ctx) {
  struct inode* inode;
  unsigned long offset;
  struct vtfs_inode_info *dir_info;
  
  inode = filp->f_inode;
  offset = ctx->pos;
  dir_info = (struct vtfs_inode_info *)inode->i_private;
  
  if (!dir_info) {
    return 0;
  }
  
  while(offset < dir_info->entries_count){
    struct entry *ent = &dir_info->entries[offset];
    if(!dir_emit(ctx, ent->name, strlen(ent->name), ent->ino, ent->entry_type)){
      break;
    }
    ctx->pos++;
    offset++;
  }
  return offset;
}

ssize_t vtfs_read(struct kiocb *iocb, struct iov_iter *to) {
  struct inode *inode = file_inode(iocb->ki_filp);
  struct vtfs_inode_info *info = (struct vtfs_inode_info *)inode->i_private;
  size_t count = iov_iter_count(to);
  loff_t pos = iocb->ki_pos;

  if (!info) return -EFAULT;

  if (pos >= info->content_len) return 0;
  if (pos + count > info->content_len) count = info->content_len - pos;

  if (copy_to_iter(info->content + pos, count, to) != count)
    return -EFAULT;

  iocb->ki_pos += count;
  return count;
}

ssize_t vtfs_write(struct kiocb *iocb, struct iov_iter *from) {
  struct inode *inode = file_inode(iocb->ki_filp);
  struct vtfs_inode_info *info = (struct vtfs_inode_info *)inode->i_private;
  size_t count = iov_iter_count(from);
  loff_t pos = iocb->ki_pos;

  if (!info) return -EFAULT;

  if (pos + count > 4096) return -ENOSPC;

  if (copy_from_iter(info->content + pos, count, from) != count)
    return -EFAULT;

  if (pos + count > info->content_len) {
    info->content_len = pos + count;
    info->size = info->content_len;
    i_size_write(inode, info->size);
  }

  iocb->ki_pos += count;
  return count;
}