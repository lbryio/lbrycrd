#include <unistd.h>
int main() {
  // This program needs to run with setuid == root
  // This needs to be in a compiled language because you cannot setuid bash scripts
  setuid(0);
  execle("/bin/bash", "bash", "-c",
         "/bin/chown -R lbrycrd:lbrycrd /data && /bin/chmod -R 755 /data/",
         (char*) NULL, (char*) NULL);
}
