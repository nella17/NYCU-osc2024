#include "fs/tmpfs.hpp"

namespace tmpfs {

::FileSystem* init() {
  static FileSystem* fs = nullptr;
  if (not fs)
    fs = new FileSystem;
  return fs;
}

long Vnode::size() const {
  return content.size();
}

int Vnode::create(const char* component_name, ::Vnode*& vnode) {
  vnode = new Vnode{kFile};
  if (vnode == nullptr)
    return -1;
  set_child(component_name, vnode);
  vnode->set_parent(this);
  return 0;
}

int Vnode::mkdir(const char* component_name, ::Vnode*& vnode) {
  vnode = new Vnode{kDir};
  if (vnode == nullptr)
    return -1;
  set_child(component_name, vnode);
  vnode->set_parent(this);
  return 0;
}

int Vnode::open(const char* component_name, ::FilePtr& file, fcntl flags) {
  return _open<File>(component_name, file, flags);
}

int File::write(const void* buf, size_t len) {
  auto& content = get()->content;
  if (content.size() < f_pos + len)
    content.resize(f_pos + len);
  memcpy(content.data() + f_pos, buf, len);
  f_pos += len;
  return len;
}

int File::read(void* buf, size_t len) {
  return _read(get()->content.data(), buf, len);
}

FileSystem::FileSystem() : ::FileSystem{"tmpfs"} {}

::Vnode* FileSystem::mount() {
  return new Vnode{kDir};
}

}  // namespace tmpfs
