#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/time.h>
#include <linux/namei.h>
#include <linux/vmalloc.h>

#include "../include/vtfs.h"

#define MODULE_NAME "dfs"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TheIrishMan05");
MODULE_DESCRIPTION("A simple FS kernel module");

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

struct file_system_type vtfs_fs_type = {
    .name = "vtfs",
    .mount = vtfs_mount,
    .kill_sb = vtfs_kill_sb,
};

static int __init vtfs_init(void) {
  int ret;
  LOG("VTFS joined the kernel\n");
  ret = register_filesystem(&vtfs_fs_type);
  if(ret){
    LOG("Failed to register filesystem: %d\n", ret);
    return ret;
  }
  return 0;
}


struct inode* vtfs_get_inode(
  struct super_block* sb,
  const struct inode* dir,
  umode_t mode,
  int i_ino
) {
  struct inode *inode;
  struct timespec64 ts;
  struct vtfs_sb_info *sb_info = VTFS_SB(sb);
  struct vtfs_inode_info *i_info;

  if (i_ino >= MAX_INODE)
      return NULL;
  
  i_info = &sb_info->inodes[i_ino];

  inode = new_inode(sb);
  if(inode){
    inode->i_ino = i_ino;
    inode->i_mode = i_info->mode;
    inode->i_op = &vtfs_inode_ops;
    
    set_nlink(inode, i_info->nlink);
    i_size_write(inode, i_info->size);

    if (S_ISDIR(inode->i_mode)) {
        inode->i_fop = &vtfs_dir_ops;
    } else {
        inode->i_fop = &vtfs_file_ops;
    }
    
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    ts = current_time(inode);
    inode_set_atime_to_ts(inode, ts);
    inode_set_mtime_to_ts(inode, ts);
    inode_set_ctime_to_ts(inode, ts);

    inode->i_private = i_info;
  }
  
  return inode;
}

int vtfs_fill_super(struct super_block *sb, void *data, int silent) {
  struct vtfs_sb_info *sb_info;
  struct vtfs_inode_info *root_info;
  struct inode* inode;

  sb_info = vzalloc(sizeof(struct vtfs_sb_info));
  if (!sb_info)
    return -ENOMEM;
  
  sb->s_fs_info = sb_info;
  spin_lock_init(&sb_info->lock);

  root_info = &sb_info->inodes[VTFS_ROOT_INO];
  root_info->mode = S_IFDIR | 0777;
  root_info->ino = VTFS_ROOT_INO;
  root_info->nlink = 2;
  root_info->parent_ino = VTFS_ROOT_INO;
  set_bit(VTFS_ROOT_INO, sb_info->valid_inodes);

  root_info->entries[0].entry_type = DT_DIR;
  root_info->entries[0].ino = VTFS_ROOT_INO;
  strcpy(root_info->entries[0].name, ".");

  root_info->entries[1].entry_type = DT_DIR;
  root_info->entries[1].ino = VTFS_ROOT_INO;
  strcpy(root_info->entries[1].name, "..");
  
  root_info->entries_count = 2;

  inode = vtfs_get_inode(sb, NULL, S_IFDIR | 0777, VTFS_ROOT_INO);
  if (!inode) {
    vfree(sb_info);
    return -ENOMEM;
  }

  sb->s_op = &vtfs_super_ops;
  sb->s_root = d_make_root(inode);
  if (!sb->s_root) {
    iput(inode);
    vfree(sb_info);
    return -ENOMEM;
  }

  LOG("Super block filled successfully.\n");
  return 0;
}

struct dentry* vtfs_mount(
  struct file_system_type* fs_type,
  int flags,
  const char* token,
  void* data) {
  struct dentry* ret = mount_nodev(fs_type, flags, data, vtfs_fill_super);
  if (ret == NULL) {
    printk(KERN_ERR "Can't mount the system");
  } else {
    printk(KERN_INFO "Mounted successfully");
  }
  return ret;
}

void vtfs_kill_sb(struct super_block *sb) {
  struct vtfs_sb_info *sb_info = VTFS_SB(sb);
  if (sb_info) {
      vfree(sb_info);
  }
  printk(KERN_INFO "vtfs super block is destroyed. Unmount successfully.\n");
}

static void __exit vtfs_exit(void) {
  unregister_filesystem(&vtfs_fs_type);
  LOG("VTFS left the kernel\n");
}

module_init(vtfs_init);
module_exit(vtfs_exit);
