#include "fs/fat32fs.hpp"

#include "fs/log.hpp"
#include "io.hpp"
#include "string.hpp"

#define FS_TYPE "fat32fs"

namespace fat32fs {

FileSystem* Vnode::fs() const {
  return static_cast<FileSystem*>(this->mount_root->fs);
}

string Vnode::_get_name(const FAT32_DirEnt* dirent) {
  string name{};
  name.reserve(sizeof(dirent->DIR_Name) + 1);

  auto name_end = dirent->file_name + sizeof(dirent->file_name) - 1;
  while (name_end != dirent->file_name and *name_end == ' ')
    name_end--;

  auto ext_end = dirent->file_ext + sizeof(dirent->file_ext) - 1;
  while (ext_end != dirent->file_ext and *ext_end == ' ')
    ext_end--;

  name += {dirent->file_name, name_end - dirent->file_name + 1};
  if (ext_end != dirent->file_ext) {
    name += ".";
    name += {dirent->file_ext, ext_end - dirent->file_ext + 1};
  }

  return name;
}

void Vnode::_load_childs(size_t cluster) {
  auto buf = new char[BLOCK_SIZE];
  auto idx = fs()->cluster2sector(cluster);

  auto beg = (FAT32_DirEnt*)(buf);
  auto end = (FAT32_DirEnt*)(buf + BLOCK_SIZE);
  for (auto it = end;; ++it) {
    if (it == end) {
      fs()->read_data(idx++, buf);
      it = beg;
    }
    if (it->end())
      break;
    if (it->invalid())
      continue;
    if (it->is_long_name()) {
      // khexdump(it, sizeof(*it), "long name");
      continue;
    }
    // khexdump(it->DIR_Name, sizeof(it->DIR_Name), "name");
    auto filename = _get_name(it);
    // kprintf("name: ");
    // kprint(filename);
    // kprintf("\n");
    if (filename == "." or filename == "..")
      continue;
    auto new_vnode = new Vnode{this->mount_root, it};
    this->set_child(filename.data(), new_vnode);
  }

  delete[] buf;
}

const char* Vnode::_load_content() {
  if (not _load) {
    _load = true;
    _content.resize(_filesize);
    auto idx = fs()->cluster2sector(_cluster);
    FS_INFO("_load_content: size %u from idx %u\n", _filesize, idx);
    fs()->read_data(idx, _content.data(), _filesize);
  }
  return _content.data();
}

Vnode::Vnode(const ::Mount* mount) : ::VnodeImpl<Vnode, File>{mount, kDir} {
  _load_childs(fs()->bpb->BPB_RootClus);
}

Vnode::Vnode(const ::Mount* mount, FAT32_DirEnt* dirent)
    : ::VnodeImpl<Vnode, File>{
          mount,
          has(dirent->DIR_Attr, FILE_Attrs::ATTR_DIRECTORY) ? kDir : kFile} {
  switch (type) {
    case kFile:
      _filesize = dirent->DIR_FileSize;
      _load = false;
      _cluster = dirent->FstClus();
      break;
    case kDir:
      _load_childs(dirent->FstClus());
      break;
  }
}

bool FileSystem::init = false;

FileSystem::FileSystem() {
  if (not init) {
    init = true;
    sd_init();
  }

  block_buf = new char[BLOCK_SIZE];

  mbr = new MBR;
  sector_0_off = 0;
  read_data(0, mbr);
  FS_HEXDUMP("MBR", mbr, sizeof(MBR));

  if (not mbr->valid()) {
    FS_WARN("disk is invalid MBR (sig = %x%x)\n", mbr->sig[0], mbr->sig[1]);
    return;
  }

  if (mbr->entry[0].active) {
    FS_INFO("first partition entry is active\n");
  }

  if (mbr->entry[0].type != PARTITION_ID_FAT32_CHS) {
    FS_WARN("first partition entry isn't FAT32 with CHS (type = 0x%x)\n",
            mbr->entry[0].type);
    return;
  }

  FS_HEXDUMP("first partition entry", &mbr->entry[0], 16);

  FS_INFO("first sector 0x%x\n", mbr->entry[0].first_sector_LBA);
  auto print_CHS = [](const char* name, CHS chs) {
    FS_INFO("%s %u / %u / %u\n", name, chs.C, chs.H, chs.S);
  };
  print_CHS("first sector", mbr->entry[0].first_sector_CHS);
  print_CHS("last sector", mbr->entry[0].last_sector_CHS);

  sector_0_off = mbr->entry[0].first_sector_LBA;

  bpb = new FAT_BPB;
  read_data(0, bpb);
  FS_HEXDUMP("FAT_BPB", bpb, sizeof(FAT_BPB));

  strncpy(label, bpb->BS_VolLab, sizeof(bpb->BS_VolLab));
  FS_INFO("volumn label: '%s'\n", label);

  if (strncmp("FAT32   ", bpb->BS_FilSysType, sizeof(bpb->BS_FilSysType))) {
    FS_WARN("invalid type '%s'\n", bpb->BS_FilSysType);
    return;
  }

  if (bpb->BPB_RootEntCnt != 0) {
    FS_WARN("BPB_RootEntCnt is not zero (%d)\n", bpb->BPB_RootEntCnt);
    return;
  }

  RootDirSectors = ((bpb->BPB_RootEntCnt * 32) + (bpb->BPB_BytsPerSec - 1)) /
                   bpb->BPB_BytsPerSec;
  FS_INFO("RootDirSectors = %d\n", RootDirSectors);

  if (bpb->BPB_FATSz16 != 0) {
    FS_WARN("BPB_FATSz16 is not zero (%d)\n", bpb->BPB_FATSz16);
    return;
  }

  FATSz = bpb->BPB_FATSz32;

  read_block(bpb->BPB_RsvdSecCnt);
  FS_INFO("first FAT\n");
  FS_HEXDUMP("FAT", block_buf, BLOCK_SIZE);

  FirstDataSector =
      bpb->BPB_RsvdSecCnt + (bpb->BPB_NumFATs * FATSz) + RootDirSectors;
  FS_INFO("FirstDataSector = %d\n", FirstDataSector);

  if (bpb->BPB_TotSec16 != 0) {
    FS_WARN("BPB_TotSec16 is not zero (%d)\n", bpb->BPB_TotSec16);
    return;
  }

  TotSec = bpb->BPB_TotSec32;
  FS_INFO("TotSec = %d\n", TotSec);
  DataSec = TotSec -
            (bpb->BPB_RsvdSecCnt + (bpb->BPB_NumFATs * FATSz) + RootDirSectors);
  FS_INFO("DataSec = %d\n", DataSec);
  CountofClusters = DataSec / bpb->BPB_SecPerClus;
  FS_INFO("CountofClusters = %d\n", CountofClusters);

  if (CountofClusters < 4085) {
    FS_WARN("Volume is FAT12\n");
    return;
  } else if (CountofClusters < 65525) {
    FS_WARN("Volume is FAT16\n");
    return;
  } else {
    FS_INFO("Volume is FAT32\n");
  }

  fsinfo = new FAT32_FSInfo;
  read_data(1, fsinfo);

  if (not fsinfo->valid()) {
    FS_WARN("fsinfo is invalid (sig = 0x%x)\n", fsinfo->FSI_LeadSig);
    return;
  }

  FS_INFO("root cluster %d -> %d\n", bpb->BPB_RootClus,
          cluster2sector(bpb->BPB_RootClus));
  read_block(cluster2sector(bpb->BPB_RootClus));
  FS_HEXDUMP("root cluster", block_buf, BLOCK_SIZE);
}

::Vnode* FileSystem::mount(const ::Mount* mount_root) {
  return new Vnode{mount_root};
}

};  // namespace fat32fs
