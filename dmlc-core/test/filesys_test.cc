#include <cstdio>
#include <cstdlib>
#include <dmlc/logging.h>
#include <dmlc/io.h>
#include "../src/io/filesys.h"

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: command files\n");
    printf("Possible commands: all path can start with hdfs:// s3:// file:// or no protocol(file:// is used)\n");
    printf("\tcat file\n");
    printf("\tls path\n");
    printf("\tcp file1 file2\n");
    return 0;
  }
  using namespace dmlc;
  using namespace dmlc::io;
  if(!strcmp(argv[1], "getpath")) {
    URI path(argv[2]);
    FileSystem *fs = FileSystem::GetInstance(path.protocol);
    FileInfo info = fs->GetPathInfo(path);
    printf("%s\t%lu\tis_dir=%d\n", info.path.name.c_str(), info.size,info.type == kDirectory);
    return 0;
  }

  if (!strcmp(argv[1], "ls")) {
    URI path(argv[2]);
    FileSystem *fs = FileSystem::GetInstance(path.protocol);
    std::vector<FileInfo> info;
    fs->ListDirectory(path, &info);
    for (size_t i = 0; i < info.size(); ++i) {
      printf("%s\t%lu\tis_dir=%d\n", info[i].path.name.c_str(), info[i].size,
             info[i].type == kDirectory);
    }
    return 0;
  }
  if (!strcmp(argv[1], "cat")) {
    URI path(argv[2]);
    FileSystem *fs = FileSystem::GetInstance(path.protocol);
    dmlc::Stream *fp = fs->OpenForRead(path);
    size_t wanted_bytes = 1024 * 1024 * 1024 * 1;
    char* buf = (char*)malloc(wanted_bytes);

    memset(buf, 0, wanted_bytes);
    size_t nread = fp->Read(buf, wanted_bytes);
    fprintf(stdout, "%s", std::string(buf, nread).c_str());
    
    //while (true) {
      //memset(buf, 0, 1024 * 100);
     //size_t nread = fp->Read(buf, 1024 * 100);
     //if (nread == 0) break;
     //fprintf(stdout, "%s", std::string(buf, nread).c_str());
    //}
    fflush(stdout);
    delete fp;
    return 0;
  }
  if (!strcmp(argv[1], "cp")) {
    CHECK(argc >= 4) << "cp requres source and dest";
    Stream *src = Stream::Create(argv[2], "r");
    Stream *dst = Stream::Create(argv[3], "w");
    char buf[1024];
    size_t nread;
    while ((nread = src->Read(buf, 1024)) != 0) {
      // printf("%s\n", buf);
      dst->Write(buf, nread);
    }
    delete src; delete dst;
    printf("copy %s to %s finished\n", argv[2], argv[3]);
    return 0;
  }
  LOG(FATAL) << "unknown command " << argv[1];
  return 0;
}
