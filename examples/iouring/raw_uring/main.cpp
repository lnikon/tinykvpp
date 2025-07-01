#include <algorithm>
#include <print>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/io_uring.h>

#define QUEUE_DEPTH 1
#define BLOCK_SZ 4096

#define read_barrier() __asm__ __volatile__("" ::: "memory")
#define write_barrier() __asm__ __volatile__("" ::: "memory")

struct app_io_sq_ring
{
    unsigned char *head;
    unsigned char *tail;
    unsigned char *ring_mask;
    unsigned char *ring_entries;
    unsigned char *flags;
    unsigned char *array;
};

struct app_io_cq_ring
{
    unsigned char       *head;
    unsigned char       *tail;
    unsigned char       *ring_mask;
    unsigned char       *ring_entries;
    struct io_uring_cqe *cqes;
};

struct submitter
{
    int                   ring_fd;
    struct app_io_sq_ring sq_ring;
    struct io_uring_sqe  *sqes;
    struct app_io_cq_ring cq_ring;
};

struct file_info
{
    off_t        file_sz;
    struct iovec iovecs[];
};

auto get_file_size(int fd) -> off_t
{
    struct stat st{};

    if (fstat(fd, &st) < 0)
    {
        perror("fstat");
        return -1;
    }

    if (S_ISBLK(st.st_mode))
    {
        unsigned long long bytes = 0;
        if (ioctl(fd, BLKGETSIZE64, &bytes) != 0)
        {
            perror("ioctl");
            return -1;
        }
        return bytes;
    }
    else if (S_ISREG(st.st_mode))
    {
        return st.st_size;
    }

    return -1;
}

auto app_setup_uring(struct submitter *s) -> int
{
    struct app_io_sq_ring *sring = &s->sq_ring;
    struct app_io_cq_ring *cring = &s->cq_ring;
    struct io_uring_params p;
    void                  *sq_ptr, *cq_ptr;

    memset(&p, 0, sizeof(p));
    s->ring_fd = io_uring_setup(QUEUE_DEPTH, &p);
    if (s->ring_fd < 0)
    {
        perror("io_uring_setup");
        return 1;
    }

    int sring_sz = p.sq_off.array + p.sq_entries * sizeof(unsigned);
    int cring_sz = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

    if (p.features & IORING_FEAT_SINGLE_MMAP)
    {
        if (cring_sz > sring_sz)
        {
            sring_sz = cring_sz;
        }
        cring_sz = sring_sz;
    }

    sq_ptr = mmap(
        0,
        sring_sz,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_POPULATE,
        s->ring_fd,
        IORING_OFF_SQ_RING
    );
    if (sq_ptr == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }

    if (p.features & IORING_FEAT_SINGLE_MMAP)
    {
        cq_ptr = sq_ptr;
    }
    else
    {
        cq_ptr = mmap(
            0,
            cring_sz,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_POPULATE,
            s->ring_fd,
            IORING_OFF_CQ_RING
        );
        if (cq_ptr == MAP_FAILED)
        {
            perror("mmap");
            return 1;
        }
    }

    sring->head = (unsigned char *)sq_ptr + p.sq_off.head;
    sring->tail = (unsigned char *)sq_ptr + p.sq_off.tail;
    sring->ring_mask = (unsigned char *)sq_ptr + p.sq_off.ring_mask;
    sring->ring_entries = (unsigned char *)sq_ptr + p.sq_off.ring_entries;
    sring->flags = (unsigned char *)sq_ptr + p.sq_off.flags;
    sring->array = (unsigned char *)sq_ptr + p.sq_off.array;

    s->sqes = (io_uring_sqe *)mmap(
        0,
        p.sq_entries * sizeof(struct io_uring_sqe),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_POPULATE,
        s->ring_fd,
        IORING_OFF_SQES
    );
    if (s->sqes == MAP_FAILED)
    {
        perror("mmap");
        return 1;
    }

    cring->head = (unsigned char *)cq_ptr + p.cq_off.head;
    cring->tail = (unsigned char *)cq_ptr + p.cq_off.tail;
    cring->ring_mask = (unsigned char *)cq_ptr + p.cq_off.ring_mask;
    cring->ring_entries = (unsigned char *)cq_ptr + p.cq_off.ring_entries;
    cring->cqes = (io_uring_cqe *)cq_ptr + p.cq_off.cqes;

    return 0;
}

void output_to_console(char *buf, int len)
{
    while (len--)
    {
        fputc(*buf++, stdout);
    }
}

void read_from_cq(struct submitter *s)
{
    struct file_info      *fi;
    struct app_io_cq_ring *cring = &s->cq_ring;
    struct io_uring_cqe   *cqe;
    unsigned               head, reaped = 0;

    head = *cring->head;

    do
    {
        read_barrier();

        if (head == *cring->tail)
        {
            break;
        }

        cqe = &cring->cqes[head & *s->cq_ring.ring_mask];
        fi = (struct file_info *)cqe->user_data;
        if (cqe->res < 0)
        {
            fprintf(stderr, "Error: %s\n", strerror(abs(cqe->res)));
        }

        int blocks = (int)fi->file_sz / BLOCK_SZ;
        if (fi->file_sz % BLOCK_SZ)
            blocks++;

        for (int i = 0; i < blocks; i++)
        {
            output_to_console((char *)fi->iovecs[i].iov_base, fi->iovecs[i].iov_len);
        }

        head++;
    } while (true);

    *cring->head = head;
    write_barrier();
}

int submit_to_sq(char *file_path, struct submitter *s)
{
    struct file_info *fi;

    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0)
    {
        perror("open");
        return 1;
    }

    struct app_io_sq_ring *sring = &s->sq_ring;
    unsigned               index = 0, current_block = 0, tail = 0, next_tail = 0;

    off_t file_sz = get_file_size(file_fd);
    if (file_sz < 0)
        return 1;
    off_t bytes_remaining = file_sz;
    int   blocks = (int)file_sz / BLOCK_SZ;
    if (file_sz % BLOCK_SZ)
        blocks++;

    fi = (struct file_info *)malloc(sizeof(*fi) + sizeof(struct iovec) * blocks);
    if (!fi)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        return 1;
    }
    fi->file_sz = file_sz;

    /*
     * For each block of the file we need to read, we allocate an iovec struct
     * which is indexed into the iovecs array. This array is passed in as part
     * of the submission. If you don't understand this, then you need to look
     * up how the readv() and writev() system calls work.
     * */
    while (bytes_remaining)
    {
        off_t bytes_to_read = bytes_remaining;
        if (bytes_to_read > BLOCK_SZ)
            bytes_to_read = BLOCK_SZ;

        fi->iovecs[current_block].iov_len = bytes_to_read;

        void *buf;
        if (posix_memalign(&buf, BLOCK_SZ, BLOCK_SZ))
        {
            perror("posix_memalign");
            return 1;
        }
        fi->iovecs[current_block].iov_base = buf;

        current_block++;
        bytes_remaining -= bytes_to_read;
    }

    /* Add our submission queue entry to the tail of the SQE ring buffer */
    next_tail = tail = *sring->tail;
    next_tail++;
    read_barrier();
    index = tail & *s->sq_ring.ring_mask;
    struct io_uring_sqe *sqe = &s->sqes[index];
    sqe->fd = file_fd;
    sqe->flags = 0;
    sqe->opcode = IORING_OP_READV;
    sqe->addr = (unsigned long)fi->iovecs;
    sqe->len = blocks;
    sqe->off = 0;
    sqe->user_data = (unsigned long long)fi;
    sring->array[index] = index;
    tail = next_tail;

    /* Update the tail so the kernel can see it. */
    if (*sring->tail != tail)
    {
        *sring->tail = tail;
        write_barrier();
    }

    /*
     * Tell the kernel we have submitted events with the io_uring_enter() system
     * call. We also pass in the IOURING_ENTER_GETEVENTS flag which causes the
     * io_uring_enter() call to wait until min_complete events (the 3rd param)
     * complete.
     * */
    int ret = io_uring_enter(s->ring_fd, 1, 1, IORING_ENTER_GETEVENTS, nullptr);
    if (ret < 0)
    {
        perror("io_uring_enter");
        return 1;
    }

    return 0;
}

int read_and_print_file(char *file_name)
{
    struct iovec *iovecs;
    int           file_fd = open(file_name, O_RDONLY);
    if (file_fd < 0)
    {
        perror("open");
        return 1;
    }

    off_t file_sz = get_file_size(file_fd);
    off_t bytes_remaining = file_sz;
    int   blocks = (int)file_sz / BLOCK_SZ;
    if (file_sz % BLOCK_SZ)
    {
        blocks++;
    }

    iovecs = (iovec *)malloc(sizeof(struct iovec) * blocks);

    int current_block = 0;

    while (bytes_remaining)
    {
        const off_t bytes_to_read = std::min(bytes_remaining, (long)BLOCK_SZ);

        void *buf = nullptr;
        if (posix_memalign(&buf, BLOCK_SZ, BLOCK_SZ))
        {
            perror("posix_memalign");
            return 1;
        }

        iovecs[current_block].iov_base = buf;
        iovecs[current_block].iov_len = bytes_to_read;
        current_block++;
        bytes_remaining -= bytes_to_read;
    }

    int ret = readv(file_fd, iovecs, blocks);
    if (ret < 0)
    {
        perror("readv");
        return 1;
    }

    for (int i = 0; i < blocks; ++i)
    {
        output_to_console((char *)iovecs[i].iov_base, iovecs[i].iov_len);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct submitter *s;
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <filename1> [<filename2> ...]\n", argv[0]);
        return 1;
    }

    s = (struct submitter *)malloc(sizeof(*s));
    if (!s)
    {
        perror("malloc");
        return 1;
    }
    memset(s, 0, sizeof(*s));

    if (app_setup_uring(s))
    {
        std::println(stderr, "Unable to setup uring!");
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (submit_to_sq(argv[i], s))
        {
            fprintf(stderr, "Error reading file\n");
            return 1;
        }
        read_from_cq(s);
    }

    return 0;
}
