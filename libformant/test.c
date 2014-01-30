#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "jkFormant.h"

int main() {
    struct stat st;
    int fd;
    void *mem;

    fd = open("out.raw", O_RDONLY);
    fstat(fd, &st);
    mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

    sound_t *s = Snack_NewSound(44100, 2);
    /* 1720 */
    LoadSound(s, mem, 44100);

    formantCmd(s);

    Snack_DeleteSound(s);
}
