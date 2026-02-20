#include "../include/vtfs.h"
#include "../include/super.h"

struct super_operations vtfs_super_ops = {
  .evict_inode = vtfs_evict_inode,
};

void vtfs_evict_inode(struct inode *inode) {
    struct vtfs_sb_info *sb_info = VTFS_SB(inode->i_sb);
    struct vtfs_inode_info *i_info = inode->i_private;

    truncate_inode_pages_final(&inode->i_data);
    clear_inode(inode);

    if (inode->i_nlink == 0 && i_info) {
        vtfs_free_inode(sb_info, i_info->ino);
    }
}