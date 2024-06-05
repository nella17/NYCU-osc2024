#pragma once

#include "ds/bitmask_enum.hpp"
#include "ds/list.hpp"
#include "util.hpp"

class Vnode;
class File;
class FileSystem;

enum filetype {
  kDir = 1,
  kFile,
};

enum fcntl {
  O_RDONLY = 00000000,
  O_WRONLY = 00000001,
  O_RDWR = 00000002,
  O_ACCMODE = 00000003,
  O_CREAT = 00000100,

  MARK_AS_BITMASK_ENUM(O_CREAT)
};

enum seek_type {
  SEEK_SET = 0,
};

class Vnode {
  struct child {
    char* name;
    Vnode* node;
    child(const char* name, Vnode* node);
    child(const child&);
    child& operator=(const child&);
    ~child();
    bool operator==(const char* name) const;
  };

 private:
  // TODO: ref cnt
  // TODO: hard link
  Vnode* _parent = nullptr;
  list<child> _childs{};
  char* _name;

 protected:
  int add_child(const char* name, Vnode* vnode);
  int del_child(const char* name);

 public:
  const filetype type;

  void set_parent(Vnode* parent);
  Vnode(filetype type, const char* name);

  const char* name() const {
    return _name;
  }
  Vnode* parent() const {
    return _parent;
  }
  const list<child>& childs() const {
    return _childs;
  }

  bool isDir() const {
    return type == kDir;
  }
  bool isFile() const {
    return type == kFile;
  }

  virtual ~Vnode();
  int lookup(const char* component_name, Vnode*& vnode);
  virtual int create(const char* component_name, Vnode*& vnode);
  virtual int mkdir(const char* component_name, Vnode*& vnode);
  virtual int open(File*& file, fcntl flags);
  virtual int close(File* file);
};

// file handle
class File {
 protected:
  Vnode* vnode;
  size_t f_pos = 0;  // RW position of this file handle
  const fcntl flags;

 public:
  const char* name() const {
    return vnode->name();
  }
  fcntl accessmode() const {
    return flags & O_ACCMODE;
  }
  bool canRead() const {
    return accessmode() == O_RDONLY or accessmode() == O_RDWR;
  }
  bool canWrite() const {
    return accessmode() == O_WRONLY or accessmode() == O_RDWR;
  }

  File(Vnode* vnode, fcntl flags) : vnode{vnode}, flags{flags} {}
  virtual ~File() = default;
  virtual long size() const;
  virtual bool can_seek() const;

  virtual int write(const void* buf, size_t len);
  virtual int read(void* buf, size_t len);
  virtual long lseek64(long offset, seek_type whence);
  int close();
};

class FileSystem {
 public:
  const char* const name;
  FileSystem* next = nullptr;

  FileSystem(const char* name) : name(name) {}

  virtual Vnode* mount(const char* name);
};

extern Vnode* root_node;
void init_vfs();

extern FileSystem* filesystems;
FileSystem** find_filesystem(const char* name);
FileSystem* get_filesystem(const char* name);
int register_filesystem(FileSystem* fs);

int vfs_open(const char* pathname, fcntl flags, File*& target);
int vfs_close(File* file);
int vfs_write(File* file, const void* buf, size_t len);
int vfs_read(File* file, void* buf, size_t len);
int vfs_mkdir(const char* pathname);
int vfs_mount(const char* target, const char* filesystem);
int vfs_lookup(const char* pathname, Vnode*& target);
int vfs_lookup(const char* pathname, Vnode*& target, char*& basename);
