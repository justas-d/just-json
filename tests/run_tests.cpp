#include <filesystem>
#include <cstdio>
#include <cstring>

int main() {
  char buf[2048];

  for(auto &f : std::filesystem::directory_iterator(".")) {
    auto * cstr = f.path().c_str();
    if(strstr(cstr, "cpp") == 0) continue;
    if(strcmp(cstr, "./run_tests.cpp") == 0) continue;
    
    snprintf(buf,sizeof(buf)/sizeof(buf[0]), "clang -ggdb %s", cstr);

    {
      auto result = system(buf);
      if(result != 0) {
        printf("COMPILATION FAILED: %d\n", result);
        printf("File: %s\n", cstr);
        break;
      }
    }

    {
      auto result = system("./a.out");
      if(result != 0) {
        printf("BINARY FAILED: %d\n", result);
        printf("File: %s\n", cstr);
        break;
      }
    }
  }

  return 0;
}
