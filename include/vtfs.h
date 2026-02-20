#ifndef VTFS_H
#define VTFS_H

#include <linux/stat.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/fs.h>

#define VTFS_ROOT_INO 1000
#define MAX_INODE 2000

extern struct inode_operations vtfs_inode_ops;
extern struct file_operations vtfs_dir_ops;
extern struct super_operations vtfs_super_ops;
extern struct file_operations vtfs_file_ops;

struct entry {
  unsigned char entry_type;
  ino_t ino;
  char name[256];
  struct inode* inode;
};

struct vtfs_inode_info {
    umode_t mode;
    uid_t uid;
    gid_t gid;
    u64 size;
    u64 nlink;
    ino_t ino;
    union {
        struct { /* for directories */
            struct entry entries[100];
            size_t entries_count;
            ino_t parent_ino;
        };
        struct { /* for files */
            char content[4096];
            u64 content_len;
        };
    };
};

struct vtfs_sb_info {
    struct vtfs_inode_info inodes[MAX_INODE];
    unsigned long valid_inodes[MAX_INODE / (sizeof(unsigned long) * 8) + 1]; /* bitmap */
    spinlock_t lock;
};

/* Helper macros/functions */
static inline struct vtfs_sb_info *VTFS_SB(struct super_block *sb) {
    return sb->s_fs_info;
}

struct inode* vtfs_get_inode(struct super_block* sb, const struct inode* dir, umode_t mode, int i_ino);
struct dentry* vtfs_mount(struct file_system_type* fs_type, int flags, const char* token, void* data);
int vtfs_fill_super(struct super_block *sb, void *data, int silent);
void vtfs_kill_sb(struct super_block* sb);
void vtfs_free_inode(struct vtfs_sb_info *sb_info, int ino);

#endif