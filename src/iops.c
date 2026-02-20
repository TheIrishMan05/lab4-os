#include "../include/iops.h"
#include "../include/vtfs.h"
#include <linux/string.h>
#include <linux/stat.h>

static bool name_exists(struct vtfs_inode_info *p_info, const char *name) {
    for (size_t i = 0; i < p_info->entries_count; i++) {
        if (strcmp(p_info->entries[i].name, name) == 0)
            return true;
    }
    return false;
}

struct inode_operations vtfs_inode_ops = {
  .lookup = vtfs_lookup,
  .create = vtfs_create,
  .unlink = vtfs_unlink,
  .mkdir = vtfs_mkdir,
  .rmdir = vtfs_rmdir,
  .link = vtfs_link,
};

static int vtfs_allocate_inode(struct vtfs_sb_info *sb_info) {
    int i;
    spin_lock(&sb_info->lock);
    for (i = VTFS_ROOT_INO + 1; i < MAX_INODE; i++) {
        if (!test_bit(i, sb_info->valid_inodes)) {
            set_bit(i, sb_info->valid_inodes);
            spin_unlock(&sb_info->lock);
            return i;
        }
    }
    spin_unlock(&sb_info->lock);
    return -ENOSPC;
}

void vtfs_free_inode(struct vtfs_sb_info *sb_info, int ino) {
    spin_lock(&sb_info->lock);
    clear_bit(ino, sb_info->valid_inodes);
    memset(&sb_info->inodes[ino], 0, sizeof(struct vtfs_inode_info));
    spin_unlock(&sb_info->lock);
}

struct dentry* vtfs_lookup(
  struct inode* parent_inode,
  struct dentry* child_dentry,
  unsigned int flag) {
  
  struct inode* inode = NULL;
  const char* name = child_dentry->d_name.name;
  size_t len = child_dentry->d_name.len;
  struct vtfs_inode_info *p_info = (struct vtfs_inode_info*)parent_inode->i_private;
  
  if(!p_info){
    return NULL;
  }

  for(size_t i = 0; i < p_info->entries_count; i++){
    struct entry *ent = &p_info->entries[i];
    if(strlen(ent->name) == len && strncmp(ent->name, name, len) == 0){
      inode = vtfs_get_inode(parent_inode->i_sb, parent_inode, 0, ent->ino);
      break;
    }
  }

  return d_splice_alias(inode, child_dentry);
}

int vtfs_create(struct mnt_idmap *idmap, struct inode *parent_inode, 
                struct dentry *child_dentry, umode_t mode, bool b) {
  const char *name = child_dentry->d_name.name;
  struct inode *inode;
  struct vtfs_inode_info *p_info = parent_inode->i_private;
  struct vtfs_sb_info *sb_info = VTFS_SB(parent_inode->i_sb);
  struct vtfs_inode_info *new_info;
  int new_ino;

  if(!p_info) return -ENOENT;
  if(p_info->entries_count >= 100) return -ENOSPC;

  if (name_exists(p_info, name)) return -EEXIST;

  new_ino = vtfs_allocate_inode(sb_info);
  if (new_ino < 0) return new_ino;

  new_info = &sb_info->inodes[new_ino];
  new_info->mode = S_IFREG | 0777;
  new_info->ino = new_ino;
  new_info->nlink = 1;
  new_info->size = 0;
  new_info->content_len = 0;

  inode = vtfs_get_inode(parent_inode->i_sb, NULL, S_IFREG | 0777, new_ino);
  if (!inode) {
    vtfs_free_inode(sb_info, new_ino);
    return -ENOMEM;
  }

  struct entry *new_entry = &p_info->entries[p_info->entries_count];
  new_entry->ino = new_ino;
  new_entry->entry_type = DT_REG;
  strncpy(new_entry->name, name, 255);
  new_entry->name[255] = '\0';
  p_info->entries_count++; 

  d_instantiate(child_dentry, inode);
  return 0;        
}

int vtfs_unlink(struct inode *parent_inode, struct dentry *child_dentry) {
  const char* name = child_dentry->d_name.name;
  size_t len = child_dentry->d_name.len;
  struct vtfs_inode_info *p_info = parent_inode->i_private;
  struct vtfs_sb_info *sb_info = VTFS_SB(parent_inode->i_sb);
  struct inode *inode = d_inode(child_dentry);
  
  if(!p_info) return -ENOENT;

  for(size_t i = 0; i < p_info->entries_count; i++){
    struct entry *ent = &p_info->entries[i];
    if(strlen(ent->name) == len && strncmp(ent->name, name, len) == 0){
      int ino = ent->ino;
      struct vtfs_inode_info *child_info = &sb_info->inodes[ino];
      
      if (child_info->nlink > 0)
          child_info->nlink--;
      
      for(size_t j = i; j < p_info->entries_count - 1; j++){
        p_info->entries[j] = p_info->entries[j + 1];
      }
      p_info->entries_count--;
      
      if (inode) {
          drop_nlink(inode);
      }
      
      return 0;
    }
  } 
  return -ENOENT;
}

int vtfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry) {
    struct inode *inode = d_inode(old_dentry);
    struct vtfs_inode_info *dir_info = dir->i_private;
    struct vtfs_inode_info *inode_info = inode->i_private;
    
    if (!dir_info || !inode_info) return -ENOENT;
    if (dir_info->entries_count >= 100) return -ENOSPC;
    
    inode_info->nlink++;
    inc_nlink(inode);
    
    struct entry *new_entry = &dir_info->entries[dir_info->entries_count];
    new_entry->ino = inode->i_ino;
    new_entry->entry_type = DT_REG;
    strncpy(new_entry->name, dentry->d_name.name, 255);
    new_entry->name[255] = '\0';
    dir_info->entries_count++;
    
    ihold(inode);
    d_instantiate(dentry, inode);
    return 0;
}

struct dentry *vtfs_mkdir(struct mnt_idmap *idmap, struct inode *parent_inode,
                    struct dentry *child_dentry, umode_t mode) {
  const char* name = child_dentry->d_name.name;
  struct inode *inode;
  struct vtfs_inode_info *p_info = parent_inode->i_private;
  struct vtfs_sb_info *sb_info = VTFS_SB(parent_inode->i_sb);
  struct vtfs_inode_info *new_info;
  int new_ino;

  if(!p_info) return (struct dentry*)ERR_PTR(-ENOENT);
  if (name_exists(p_info, name)) return (struct dentry*)ERR_PTR(-EEXIST);
  if(p_info->entries_count >= 100) return (struct dentry*)ERR_PTR(-ENOSPC);

  new_ino = vtfs_allocate_inode(sb_info);
  if (new_ino < 0) return (struct dentry*)ERR_PTR(new_ino);

  new_info = &sb_info->inodes[new_ino];
  new_info->mode = S_IFDIR | 0777;
  new_info->ino = new_ino;
  new_info->nlink = 2;
  new_info->parent_ino = parent_inode->i_ino;
  new_info->entries_count = 2;

  new_info->entries[0].ino = new_ino;
  new_info->entries[0].entry_type = DT_DIR;
  strcpy(new_info->entries[0].name, ".");
  
  new_info->entries[1].ino = parent_inode->i_ino;
  new_info->entries[1].entry_type = DT_DIR;
  strcpy(new_info->entries[1].name, "..");

  inode = vtfs_get_inode(parent_inode->i_sb, parent_inode, S_IFDIR | 0777, new_ino);
  if(!inode){
    vtfs_free_inode(sb_info, new_ino);
    return (struct dentry*)ERR_PTR(-ENOMEM);
  }

  struct entry *new_entry = &p_info->entries[p_info->entries_count];
  new_entry->ino = new_ino;
  new_entry->entry_type = DT_DIR;
  strncpy(new_entry->name, name, 255);
  new_entry->name[255] = '\0';
  p_info->entries_count++; 

  inc_nlink(parent_inode);
  d_instantiate(child_dentry, inode);
  return child_dentry;
}

int vtfs_rmdir(struct inode *parent_inode, struct dentry *child_dentry) {
  const char* name = child_dentry->d_name.name;
  size_t len = child_dentry->d_name.len;
  struct vtfs_inode_info *p_info = parent_inode->i_private;
  struct vtfs_sb_info *sb_info = VTFS_SB(parent_inode->i_sb);
  struct inode *inode = d_inode(child_dentry);

  if(!p_info) return -ENOENT;

  for(size_t i = 0; i < p_info->entries_count; i++){
    struct entry *ent = &p_info->entries[i];
    if(strlen(ent->name) == len && strncmp(ent->name, name, len) == 0){
      int ino = ent->ino;
      struct vtfs_inode_info *child_info = &sb_info->inodes[ino];

      if(ent->entry_type != DT_DIR) return -ENOTDIR;
      if(child_info->entries_count > 2) return -ENOTEMPTY;

      for(size_t j = i; j < p_info->entries_count - 1; j++){
        p_info->entries[j] = p_info->entries[j + 1];
      }
      p_info->entries_count--;

      drop_nlink(parent_inode);
      
      if (inode) {
         clear_nlink(inode);
         /* Free the child directory inode */
      /* Note: Deferred to evict_inode */
      }

      return 0;
    }
  }
  return -ENOENT;
}
