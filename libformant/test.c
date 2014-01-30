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

    sound_t sound;
    sound_init(&sound, 44100, 2);
    /* 1720 */
    sound_load_samples(&sound, mem, 5000);

    sound_calc_formants(&sound);

    sound_destroy(&sound);
}
