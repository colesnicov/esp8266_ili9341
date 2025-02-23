/**
 @file posix.c

 @par Copyright &copy; 2014-2017 Mike Gore, All rights reserved. GPL  License
 @see http://github.com/magore/hp85disk
 @see http://github.com/magore/hp85disk/COPYRIGHT.md for specific Copyright details

 @par You are free to use this code under the terms of GPL
   please retain a copy of this notice in any code you use it in.

This is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option)
any later version.

This software is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

 @brief POSIX wrapper for FatFS
   - Provides many of the common Posix/linux functions 
   - POSIX character I/O functions
        - isatty
        - fgetc
        - fputc
        - getchar
        - putc
        - putchar
        - ungetc

   - POSIX string I/O functions
        - fgets
        - fputs
        - puts

   - POSIX file position functions
        - feof
        - fgetpos
        - fseek
        - fsetpos
        - ftell
        - lseek
        - rewind

   - POSIX file functions
        - close
        - fileno
        - fileno_to_stream  NOT POSIX
        - fopen
        - fread
        - ftruncate
        - fwrite
        - open
        - read
        - sync
        - syncfs
        - truncate
        - write
        - fclose

   - POSIX file information functions
        - dump_stat - NOT POSIX
        - fstat
        - mctime    - NOT POSIX
        - stat

   - POSIX file and directory manipulation
        - basename
        - baseext   - NOT POSIX
        - chmod
        - chdir
        - dirname
        - getcwd
        - mkdir
        - rename
        - rmdir
        - unlink
        - utime

   - POSIX - directory scanning functions
        - closedir
        - opendir 
        - readdir

   - POSIX error functions
        - clrerror
        - ferror
        - perror
        - strerror
        - strerror_r

   - Device open functions
        - fdevopen  - NOT POSIX

   - FatFS to POSIX bridge functions - NOT POSIX
        - fatfs_getc
        - fatfs_putc
        - fatfs_to_errno
        - fatfs_to_fileno
        - fat_time_to_unix
        - fileno_to_fatfs
        - free_file_descriptor
        - new_file_descriptor
        - mkfs
        - posix_fopen_modes_to_open
        - unix_time_to_fat

*/

#include "user_config.h"

#include "stringsup.h"
#include "fatfs.h"

#include "posix.h"

#ifdef ESP8266
// FIXME ESP8266 library conflict
#undef strerror_r
#endif

///  Note: fdevopen assigns stdin,stdout,stderr

///@brief POSIX errno.
int errno;


///@brief POSIX fileno to POSIX FILE stream table
///
/// - Note: the index of __iob[] is reffered to "fileno".
/// - Reference: libc/avr-libc-1.8.0/libc/stdio.
/// - stdin = __iob[0].
/// - __iob[1] = stdout.
/// - __iob[2] = stderr.
FILE *__iob[MAX_FILES];

/// @brief POSIX error messages for each errno value.
///
/// - man page errno (3)
const char *sys_errlist[] =
{
    "OK",
    "Operation not permitted",
    "No such file or directory",
    "No such process",
    "Interrupted system call",
    "I/O error",
    "No such device or address",
    "Argument list too long",
    "Exec format error",
    "Bad file number",
    "No child processes",
    "Try again",
    "Out of memory",
    "Permission denied",
    "Bad address",
    "Block device required",
    "Device or resource busy",
    "File exists",
    "Cross-device link",
    "No such device",
    "Not a directory",
    "Is a directory",
    "Invalid argument",
    "File table overflow",
    "Too many open files",
    "Not a typewriter",
    "Text file busy",
    "File too large",
    "No space left on device",
    "Illegal seek",
    "Read-only file system",
    "Too many links",
    "Broken pipe",
    "Math argument out of domain of func",
    "Math result not representable",
    "Bad Message",
    NULL
};

// =============================================
/// - POSIX character I/O functions
/// @brief  Test POSIX fileno if it is a Serial Console/TTY.
///
///  - man page isatty (3).
///
/// @param[in] fileno: POSIX fileno of open file.
///
/// @return 1 if fileno is a serial TTY/Console (uart in avr-libc terms).
/// @return 0 if POSIX fileno is NOT a Serial TTY.
MEMSPACE
int isatty(int fileno)
{
/// @todo  Perhaps we should verify console functions have been added ?
    if(fileno >= 0 && fileno <= 2)
        return(1);
    return 0;
}

/// @brief Get byte from a TTY device or FatFs file stream
/// open() or fopen() sets stream->get = fatfs_getc() for FatFs functions
/// See fdevopen()        sets stream->get for TTY devices
///
/// - man page fgetc (3).
///
/// @param[in] stream: POSIX stream pointer.
///
/// @return character.
/// @return EOF on error with errno set.
MEMSPACE
int
fgetc(FILE *stream)
{
    int c;

    if(stream == NULL)
    {
        errno = EBADF;                            // Bad File Number
        return(EOF);
    }

#ifdef ESP8266
    optimistic_yield(1000);
    wdt_reset();
#endif

    if ((stream->flags & __SRD) == 0)
        return EOF;

    if ((stream->flags & __SUNGET) != 0) {
        stream->flags &= ~__SUNGET;
        stream->len++;
        return stream->unget;
    }

    if (stream->flags & __SSTR) {
        c = *stream->buf;
        if (c == '\0') {
            stream->flags |= __SEOF;
            return EOF;
        } else {
            stream->buf++;
        }
    } else {
        if(!stream->get)
        {
            printf("fgetc stream->get NULL\n");
            return(EOF);
        }
        // get character from device or file
        c = stream->get(stream);
        if (c < 0) {
            /* if != _FDEV_ERR, assume its _FDEV_EOF */
            stream->flags |= (c == _FDEV_ERR)? __SERR: __SEOF;
            return EOF;
        }
    }

    stream->len++;
    return (c);
}

/// @brief Put a byte to TTY device or FatFs file stream
/// open() or fopen() sets stream->put = fatfs_outc() for FatFs functions
/// See fdevopen()        sets stream->put get for TTY devices
///
/// - man page fputc (3).
///
/// @param[in] stream: POSIX stream pointer.
///
/// @return character.
MEMSPACE
int
fputc(int c, FILE *stream)
{
    errno = 0;
    int ret;

    if(stream == NULL)
    {
        errno = EBADF;                            // Bad File Number
        return(EOF);
    }

#ifdef ESP8266
    optimistic_yield(1000);
    wdt_reset();
#endif

    if(stream != stdout && stream != stderr)
    {
        return(fatfs_putc(c,stream));
    }

    // TTY outputs

    if ((stream->flags & __SWR) == 0)
        return EOF;

    if (stream->flags & __SSTR) {
        if (stream->len < stream->size)
            *stream->buf++ = c;
        stream->len++;
        return c;
    } else {
        if(!stream->put)
        {
            printf("fputc stream->put NULL\n");
            return(EOF);
        }
        ret = stream->put(c, stream);
        if(ret != EOF)
            stream->len++;
        return(ret);
    }
}



///@brief functions normally defined as macros
#ifndef IO_MACROS
/// @brief get a character from stdin
/// See fdevopen()   sets stream->get for TTY devices
///
/// - man page getchar (3).
///
/// @param[in] stream: POSIX stream pointer.
///
/// @return character.
MEMSPACE
int
getchar()
{
    return(fgetc(stdin));
}

/// @brief put a character to stdout
/// See fdevopen()        sets stream->put get for TTY devices
///
/// - man page putchar (3).
///
/// @param[in] stream: POSIX stream pointer.
///
/// @return character.
MEMSPACE
int
putchar(int c)
{
    return(fputc(c,stdout));
}
#endif

/// @brief Un-Get byte from a TTY device or FatFs file stream
///
/// - man page ungetc (3).
///
/// @param[in] c: Character to unget
/// @param[in] stream: POSIX stream pointer.
///
/// @return character.
/// @return EOF on error with errno set.
MEMSPACE
int
ungetc(int c, FILE *stream)
{
    int fd = fileno(stream);
    if(!isatty(fd))
        return(EOF);

    if(c == EOF)
        return EOF;
    if((stream->flags & __SUNGET) != 0 )
        return EOF;
    if ((stream->flags & __SRD) == 0 )
        return EOF;

    stream->flags |= __SUNGET;
    stream->flags &= ~__SEOF;

    stream->unget = c;
    stream->len--;

    return (c);
}

#ifndef IO_MACROS
// =============================================
/// @brief Put a character to a stream
/// See fdevopen()  sets stream->put get for TTY devices
///
/// - man page putc (3).
///
/// @param[in] stream: POSIX stream pointer.
///
MEMSPACE
int
putc(int c, FILE *stream)
{
    return(fputc(c, stream));
}

#endif

// =============================================
///  POSIX string I/O
/// @brief get a string from stdin
/// See fdevopen()        sets stream->put get for TTY devices
///
/// - man page fgets (3).
///
/// @param[in] stream: POSIX stream pointer.
///
/// @return character.
MEMSPACE
char *
fgets(char *str, int size, FILE *stream)
{
    int c;
    int ind = 0;
    while(size--)
    {
        c = fgetc(stream);
        if(c == EOF)
        {
            if( ind == 0)
                return(NULL);
            break;
        }
        if(c == '\n')
            break;
        if(c == 0x08)
        {
             if(ind > 0)
                --ind;
            continue;
        }
        str[ind++] = c;
    }
    str[ind] = 0;
    return(str);
}

/// @brief put a string to stdout
/// See fdevopen()        sets stream->put get for TTY devices
///
/// - man page fputs (3).
///
/// @param[in] stream: POSIX stream pointer.
///
/// @return character.
MEMSPACE
int
fputs(const char *str, FILE *stream)
{
    while(*str)
    {
        if(fputc(*str, stream) == EOF)
            return(EOF);
        ++str;
    }
    return(0);
}


#ifndef IO_MACROS
/// @brief put a string to stdout
/// See fdevopen()        sets stream->put get for TTY devices
///
/// - man page puts (3).
///
/// @param[in] stream: POSIX stream pointer.
///
/// @return character.
MEMSPACE
int
puts(const char *str)
{
    while(*str)
    {
        if(fputc(*str, stdout) == EOF)
            return(EOF);
        ++str;
    }
    return ( fputc('\n',stdout) );
}

#endif


// =============================================
// =============================================
///  - POSIX file position functions
// =============================================
// =============================================
/// @brief feof reports if the stream is at EOF 
/// - man page feof (3).
///
/// @param[in] stream: POSIX stream pointer.
/// @return 1 if EOF set, 0 otherwise.
MEMSPACE
int feof(FILE *stream)
{
    if(stream->flags & __SEOF)
        return(1);
    return(0);
}

/// @brief POSIX get position of file stream.
///
/// - man page fgetpos (3).
///
/// @param[in] stream: POSIX file stream.
/// @param[in] pos: position pointer for return.
///
/// @return 0 on sucess.
/// @return -1 on error with errno set.
MEMSPACE
int fgetpos(FILE *stream, size_t *pos)
{
    long offset = ftell(stream);
    *pos = offset;
    if(offset == -1)
        return(-1);
    return( 0 );
}

/// @brief POSIX seek to file possition.
///
/// - man page fseek (3).
///
/// @param[in] stream: POSIX file stream.
/// @param[in] offset: offset to seek to.
/// @param[in] whence:
///  - SEEK_SET The offset is set to offset bytes.
///  - SEEK_CUR The offset is set to its current location plus offset bytes.
///  - SEEK_END The offset is set to the size of the file plus offset bytes.
///
/// @return file position on sucess.
/// @return -1 on error.
MEMSPACE
int fseek(FILE *stream, long offset, int whence)
{
    long ret;

    int fn = fileno(stream);
    if(fn < 0)
        return(-1);

    ret  = lseek(fn, offset, whence);

    if(ret == -1)
        return(-1);

    return(0);
}

/// @brief POSIX set position of file stream.
///
/// - man page fsetpos (3).
///
/// @param[in] stream: POSIX file stream.
/// @param[in] pos: position pointer.
///
/// @return 0 with *pos set to position on sucess.
/// @return -1 on error with errno set.
MEMSPACE
int fsetpos(FILE *stream, size_t *pos)
{
    return (fseek(stream, (size_t) *pos, SEEK_SET) );
}

/// @brief POSIX file position of open stream.
///
/// - man page fteel (3).
///
/// @param[in] stream: POSIX file stream.
///
/// @return file position on sucess.
/// @return -1 on error with errno set.
MEMSPACE
long ftell(FILE *stream)
{
    errno = 0;

    int fn = fileno(stream);
    if(isatty(fn))
        return(-1);
    // fileno_to_fatfs checks for fd out of bounds
    FIL *fh = fileno_to_fatfs(fn);
    if ( fh == NULL )
    {
        errno = EBADF;
        return(-1);
    }

    return( fh->fptr );
}

/// @brief POSIX seek to file position.
///
/// - man page lseek (2).
///
/// @param[in] fileno: POSIX fileno of open file.
/// @param[in] position: offset to seek to.
/// @param[in] whence
///  - SEEK_SET The offset is set to offset bytes.
///  - SEEK_CUR The offset is set to its current location plus offset bytes.
///  - SEEK_END The offset is set to the size of the file plus offset bytes.
///
/// @return file position on sucess.
/// @return -1 on error.
MEMSPACE
off_t lseek(int fileno, off_t position, int whence)
{
    FRESULT res;
    FIL *fh;
    errno = 0;
    FILE *stream;


    // fileno_to_fatfs checks for fd out of bounds
    fh = fileno_to_fatfs(fileno);
    if(fh == NULL)
    {
        errno = EMFILE;
        return(-1);
    }
    if(isatty(fileno))
        return(-1);
    

    stream = fileno_to_stream(fileno);
    stream->flags |= __SUNGET;

    if(whence == SEEK_END)
        position += f_size(fh);
    else if(whence==SEEK_CUR)
        position += fh->fptr;

    res = f_lseek(fh, position);
    if(res)
    {
        errno = fatfs_to_errno(res);
        return -1;
    }
    if(position != f_tell(fh) )
    {
        printf("Seek failed");
        return(-1L);
    }
    return (fh->fptr);
}

/// @brief POSIX  rewind file to the beginning.
///
/// - man page rewind (3).
///
/// @param[in] stream: POSIX file stream.
///
/// @return  void.
MEMSPACE
void rewind( FILE *stream)
{
    fseek(stream, 0L, SEEK_SET);
}

// =============================================
// =============================================
///  - POSIX file functions
// =============================================
// =============================================
/// @brief POSIX Close a file with fileno handel.
///
/// - man page close (2).
///
/// @param[in] fileno: fileno of file.
///
/// @return 0 on sucess.
/// @return -1 on error with errno set.
MEMSPACE
int close(int fileno)
{
    FILE *stream;
    FIL *fh;
    int res;

    errno = 0;

    // checks if fileno out of bounds
    stream = fileno_to_stream(fileno);
    if(stream == NULL)
    {
        return(-1);
    }

    // fileno_to_fatfs checks for fileno out of bounds
    fh = fileno_to_fatfs(fileno);
    if(fh == NULL)
    {
        return(-1);
    }
    res = f_close(fh);
    free_file_descriptor(fileno);
    if (res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
    return(0);
}

/// @brief Convert POSIX stream pointer to POSIX fileno (index of __iob[])
///
/// - man page fileno (3)
/// @param[in] stream: stream pointer
///
/// @return  int fileno on success
/// @return -1 with errno = EBAFD if stream is NULL or not found
MEMSPACE
int fileno(FILE *stream)
{
    int fileno;

    if(stream == NULL)
    {
        errno = EBADF;
        return(-1);
    }

    for(fileno=0; fileno<MAX_FILES; ++fileno)
    {
        if ( __iob[fileno] == stream)
            return(fileno);
    }
    return(-1);
}

/// @brief Convert POSIX fileno to POSIX FILE stream pointer.
/// NOT POSIX
///
/// - inverse of POSIX fileno()
/// - man page fileno (3)
///
/// @param[in] fileno: POSIX fileno is the index of __iob[].
///
/// @see fileno()
/// @return FILE * on success
/// @return NULL on error with errno set,  NULL if fileno out of bounds
MEMSPACE
FILE *fileno_to_stream(int fileno)
{
    FILE *stream;
    if(fileno < 0 || fileno >= MAX_FILES)
    {
        errno = EBADF;
        return(NULL);
    }

    stream = __iob[fileno];
    if(stream == NULL)
    {
        errno = EBADF;
        return(NULL);
    }
    return(stream);
}

///@brief POSIX Open a file with path name and ascii file mode string.
///
/// - man page fopen(3).
///
/// @param[in] path: filename string.
/// @param[in] mode: POSIX open mode strings.
///
/// @return stream * on success.
/// @return NULL on error with errno set.
MEMSPACE
FILE *fopen(const char *path, const char *mode)
{
    int flags = posix_fopen_modes_to_open(mode);
    int fileno = open(path, flags);

    // checks if fileno out of bounds
    return( fileno_to_stream(fileno) );
}

/// @brief POSIX read nmemb elements from buf, size bytes each, to the stream fd.
///
/// - man page fread (3).
///
/// @param[in] ptr: buffer.
/// @param[in] nmemb: number of items to read.
/// @param[in] size: size of each item in bytes.
/// @param[in] stream: POSIX file stream.
///
/// @return count on sucess.
/// @return 0 or < size on error with errno set.
MEMSPACE
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t count = size * nmemb;
    int fn = fileno(stream);
    ssize_t ret;

    // read() checks for fn out of bounds
    ret = read(fn, ptr, count);
    if(ret < 0)
        return(0);

    return((size_t) ret);
}

/// @brief POSIX truncate open file to length.
///
/// - man page ftruncate (3).
///
/// @param[in] fd: open file number.
/// @param[in] length: length to truncate to.
///
/// @return 0 on success.
/// @return -1 on fail.
MEMSPACE
int ftruncate(int fd, off_t length)
{
    errno = 0;
    FIL *fh;
    FRESULT rc;

    if(isatty(fd))
        return(-1);
    // fileno_to_fatfs checks for fd out of bounds
    fh = fileno_to_fatfs(fd);
    if(fh == NULL)
    {
        return(-1);
    }
    rc = f_lseek(fh, length);
    if (rc != FR_OK)
    {
        errno = fatfs_to_errno(rc);
        return(-1);
    }
    rc = f_truncate(fh);
    if (rc != FR_OK)
    {
        errno = fatfs_to_errno(rc);
        return(-1);
    }
    return(0);
}

/// @brief POSIX write nmemb elements from buf, size bytes each, to the stream fd.
///
/// - man page write (2).
///
/// @param[in] ptr: buffer.
/// @param[in] nmemb: number of items to write.
/// @param[in] size: size of each item in bytes.
/// @param[in] stream: POSIX file stream.
///
/// @return count written on sucess.
/// @return 0 or < size on error with errno set.
MEMSPACE
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t count = size * nmemb;
    int fn = fileno(stream);
    ssize_t ret;
    
    // write () checks for fn out of bounds
    ret =  write(fn, ptr, count);

    if(ret < 0)
        return(0);

    return((size_t) ret);
}



/// @brief POSIX Open a file with integer mode flags.
///
/// - man page open (2).
///
/// @param[in] pathname: filename string.
/// @param[in] flags: POSIX open modes.
///
/// @return fileno on success.
/// @return -1 on error with errno set.
MEMSPACE
int open(const char *pathname, int flags)
{
    int fileno;
    int fatfs_modes;
    FILE *stream;
    FIL *fh;
    int res;

    errno = 0;

// FIXME Assume here that the disk interface mmc_init was already called 
#if 0
// Checks Disk status
    res = mmc_init(0);

    if(res != RES_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
#endif

    if((flags & O_ACCMODE) == O_RDWR)
        fatfs_modes = FA_READ | FA_WRITE;
    else if((flags & O_ACCMODE) == O_RDONLY)
        fatfs_modes = FA_READ;
    else
        fatfs_modes = FA_WRITE;

    if(flags & O_CREAT)
    {
        if(flags & O_TRUNC)
            fatfs_modes |= FA_CREATE_ALWAYS;
        else
            fatfs_modes |= FA_OPEN_ALWAYS;
    }

    fileno = new_file_descriptor();

    // checks if fileno out of bounds
    stream = fileno_to_stream(fileno);
    if(stream == NULL)
    {
        free_file_descriptor(fileno);
        return(-1);
    }

    // fileno_to_fatfs checks for fileno out of bounds
    fh = fileno_to_fatfs(fileno);
    if(fh == NULL)
    {
        free_file_descriptor(fileno);
        errno = EBADF;
        return(-1);
    }
    res = f_open(fh, pathname, (BYTE) (fatfs_modes & 0xff));
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        free_file_descriptor(fileno);
        return(-1);
    }
    if(flags & O_APPEND)
    {
///  Seek to end of the file
        res = f_lseek(fh, f_size(fh));
        if (res != FR_OK)
        {
            errno = fatfs_to_errno(res);
            f_close(fh);
            free_file_descriptor(fileno);
            return(-1);
        }
    }

    if((flags & O_ACCMODE) == O_RDWR)
    {
        // FIXME fdevopen should do this
        stream->put = fatfs_putc;
        stream->get = fatfs_getc;
        stream->flags = _FDEV_SETUP_RW;
    }
    else if((flags & O_ACCMODE) == O_RDONLY)
    {
        // FIXME fdevopen should do this
        stream->put = NULL;
        stream->get = fatfs_getc;
        stream->flags = _FDEV_SETUP_READ;
    }
    else
    {
        // FIXME fdevopen should do this
        stream->put = fatfs_putc;
        stream->get = NULL;
        stream->flags = _FDEV_SETUP_WRITE;
    }

    return(fileno);
}

/// @brief POSIX read count bytes from *buf to fileno fd.
///
/// - man page read (2).
///
/// @param[in] fd: POSIX fileno.
/// @param[in] buf: buffer.
/// @param[in] count: number of bytes to write.
///
/// @return count on sucess.
/// @return -1 on error with errno set.
MEMSPACE
ssize_t read(int fd, const void *buf, size_t count)
{
    UINT size;
    UINT bytes = count;
    int res;
    int ret;
    FIL *fh;
    FILE *stream;

    //FIXME
    *(char *) buf = 0;

    errno = 0;

    // TTY read function
    // FIXME should we really be blocking ???
    stream = fileno_to_stream(fd);
    if(stream == stdin)
    {
        char *ptr = (char *) buf;
        // ungetc is undefined for read
        stream->flags |= __SUNGET;
        size = 0;
        while(count--)
        {
            ret = fgetc(stream);
            if(ret < 0)
                break;
            
            *ptr++ = ret;
            ++size;
        }
        return(size);
    }
    if(stream == stdout || stream == stderr)
    {
        return(-1);
    }

    // fileno_to_fatfs checks for fd out of bounds
    fh = fileno_to_fatfs(fd);
    if ( fh == NULL )
    {
        errno = EBADF;
        return(-1);
    }

    res = f_read(fh, (void *) buf, bytes, &size);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
    return ((ssize_t) size);
}


/// @brief POSIX Sync all pending file changes and metadata on ALL files.
///
/// - man page sync (2).
///
/// @return  void.
MEMSPACE
void sync(void)
{
    FIL *fh;
    int i;

    for(i=0;i<MAX_FILES;++i)
    {
        if(isatty(i))
            continue;

        // fileno_to_fatfs checks for i out of bounds
        fh = fileno_to_fatfs(i);
        if(fh == NULL)
            continue;

        (void ) syncfs(i);
    }
}

/// @brief POSIX Sync pending file changes and metadata for specified fileno.
///
/// - man page syncfs (2).
///
/// @param[in] fd: POSIX fileno to sync.
/// @return 0.
/// @return -1 on error witrh errno set.
MEMSPACE
int syncfs(int fd)
{
    FIL *fh;
    FRESULT res;
    FILE *stream;

    errno = 0;

    if(isatty(fd))
    {
        errno = EBADF;
        return(-1);
    }
    stream = fileno_to_stream(fd);
    // reset unget on sync
    stream->flags |= __SUNGET;

    // fileno_to_fatfs checks for fd out of bounds
    fh = fileno_to_fatfs(fd);
    if(fh == NULL)
    {
        errno = EBADF;
        return(-1);
    }

    res  = f_sync ( fh );
    if (res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
    return(0);
}



/// @brief POSIX truncate named file to length.
///
/// - man page truncate (2).
///
/// @param[in] path: file name to truncate.
/// @param[in] length: length to truncate to.
///
/// @return 0 on sucess.
/// @return -1 n fail.
MEMSPACE
int truncate(const char *path, off_t length)
{
    errno = 0;
    FIL fh;
    FRESULT rc;

    rc = f_open(&fh , path, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
    if (rc != FR_OK)
    {
        errno = fatfs_to_errno(rc);
        return(-1);
    }
    rc = f_lseek(&fh, length);
    if (rc != FR_OK)
    {
        errno = fatfs_to_errno(rc);
        return(-1);
    }
    rc = f_truncate(&fh);
    if (rc != FR_OK)
    {
        errno = fatfs_to_errno(rc);
        return(-1);
    }
    return(0);
}


/// @brief POSIX Write count bytes from *buf to fileno fd.
///
/// - man page write (2).
///
/// @param[in] fd: POSIX fileno.
/// @param[in] buf: buffer.
/// @param[in] count: number of bytes to write.
/// @return count on sucess.
/// @return -1 on error with errno set.
MEMSPACE
ssize_t write(int fd, const void *buf, size_t count)
{
    UINT size;
    UINT bytes = count;
    FRESULT res;
    FIL *fh;
    FILE *stream;
    errno = 0;

    // TTY read function
    stream = fileno_to_stream(fd);
    if(stream == stdout || stream == stderr)
    {
        char *ptr = (char *) buf;   
        size = 0;
        while(count--)
        {
            int c,ret;
            c = *ptr++;
            ret = fputc(c, stream);
            if(c != ret)
                break;

            ++size;
        }
        return(size);
    }
    if(stream == stdin)
    {
        return(-1);
    }

    // fileno_to_fatfs checks for fd out of bounds
    fh = fileno_to_fatfs(fd);
    if ( fh == NULL )
    {
        errno = EBADF;
        return(-1);
    }

    res = f_write(fh, buf, bytes, &size);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
    return ((ssize_t) size);
}


/// @brief POSIX close a file stream.
///
/// - man page flose (3).
///
/// @param[in] stream: POSIX stream pointer.

/// @return  0 on sucess.
/// @return  -1 on error witrh errno set.
MEMSPACE
int fclose(FILE *stream)
{
    int fn = fileno(stream);
    if(fn < 0)
        return(EOF);

    return( close(fn) );
}

// =============================================
// =============================================
///  - POSIX file information functions
// =============================================
// =============================================

/// @brief Display struct stat, from POSIX stat(0 or fstat(), in ASCII.
/// NOT POSIX
/// @param[in] sp: struct stat pointer.
///
/// @return  void.
MEMSPACE
void dump_stat(struct stat *sp)
{
    mode_t mode = sp->st_mode;

    printf("\tSize:  %lu\n", (uint32_t)sp->st_size);

    printf("\tType:  ");
    if(S_ISDIR(mode))
        printf("DIR\n");
    else if(S_ISREG(mode))
        printf("File\n");
    else
        printf("Unknown\n");


    printf("\tMode:  %lo\n", (uint32_t)sp->st_mode);
    printf("\tUID:   %lu\n", (uint32_t)sp->st_uid);
    printf("\tGID:   %lu\n", (uint32_t)sp->st_gid);
    printf("\tatime: %s\n",mctime((time_t)sp->st_atime));
    printf("\tmtime: %s\n",mctime((time_t)sp->st_mtime));
    printf("\tctime: %s\n",mctime((time_t)sp->st_ctime));
}

#if 0
/// @brief POSIX fstat of open file.
/// FatFS does not have a function that will map to this
///
/// - man page (2).
///
/// @param[in] fd: POSIX fileno of open file.
/// @param[in] buf: struct stat buffer to return results in.
///
/// @return 0 on success.
/// @return -1 on error with errno set.
///
/// @todo needs fileno to filename lookup in order to work.
/// - We may be able to work out the directory pointer from the FatFS data?
/// - or - cache the filename on open ???
MEMSPACE
int fstat(int fd, struct stat *buf)
{
    FIL *fh;
    FRESULT rc;

    if(isatty(fd))
        return(-1);

    //FIXME TODO
    return(-1);

}
#endif

/// @brief Display Ascii formatted time from timev seconds 
/// NOT POSIX
///
/// - Assumes no timezone offset.
/// - man page ctime (3).
///
/// @param[in] timev: epoch time in seconds
/// @return  ascii string pointer of POSIX ctime()
/// @see ctime()
MEMSPACE
char *mctime(time_t timev)
{
    errno = 0;
    char *ptr = (char *)ctime_gm(&timev);
    int len;
    len = strlen(ptr);
    if(len && ptr[len-1] == '\n')
        ptr[len-1] = 0;
    return(ptr);
}

/// @brief POSIX stat - get file status of named file.
///
/// - man page (2).
///
/// @param[in] name: file name.
/// @param[in] buf: struct stat buffer to return results in.
///
/// @return 0 on success.
/// @return -1 on error with errno set.
MEMSPACE
int stat(char *name, struct stat *buf)
{
    FILINFO info;
    int res;
    time_t epoch;
    uint16_t mode;
    errno = 0;

    // f_stat does not handle / or . as root directory
    if (strcmp(name,"/") == 0 || strcmp(name,".") == 0)
    {
        buf->st_atime = 0;
        buf->st_mtime = 0;
        buf->st_ctime = 0;
        buf->st_uid= 0;
        buf->st_gid= 0;
        buf->st_size = 0;
        buf->st_mode = S_IFDIR;
        return(0);
    }

    res = f_stat(name, &info);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }

    buf->st_size = info.fsize;
    epoch = fat_time_to_unix(info.fdate, info.ftime);
    buf->st_atime = epoch;                        // Access time
    buf->st_mtime = epoch;                        // Modification time
    buf->st_ctime = epoch;                        // Creation time

    // We only handle read only case
    mode = (FATFS_R | FATFS_X);
    if( !(info.fattrib & AM_RDO))
        mode |= (FATFS_W);                        // enable write if NOT read only

    if(info.fattrib & AM_SYS)
    {
        buf->st_uid= 0;
        buf->st_gid= 0;
    }
    {
        buf->st_uid=1000;
        buf->st_gid=1000;
    }

    if(info.fattrib & AM_DIR)
        mode |= S_IFDIR;
    else
        mode |= S_IFREG;
    buf->st_mode = mode;

    return(0);
}

///@brief Set Modification and Access time of a file
///@param[in] filename: file name
///@param[in *times:  access and modication utimbuf structure, if NULL use current time
///@return 0 if ok, -1 on error
MEMSPACE
int utime(const char *filename, const struct utimbuf *times)
{

    FILINFO fno;
    uint16_t fdate,ftime;
    time_t ut;
    int res;

    if(times)
        ut = times->modtime;
    else
        ut = time(0);
    
    unix_time_to_fat(ut, (uint16_t *) &fdate, (uint16_t *) &ftime);
    

    fno.fdate = fdate;
    fno.ftime = ftime;

    res = f_utime(filename, (FILINFO *) &fno);

    return( fatfs_to_errno(res) );
}



// =============================================
// =============================================
///  - POSIX file and directory manipulation
// =============================================
// =============================================
/// @brief POSIX Basename of filename.
///
/// - man page (3).
///
/// @param[in] str: string to find basename in.
///
/// @return pointer to basename of string.
MEMSPACE
char *basename(char *str)
{
    char *base = str;
    if(!str)
        return("");
    while(*str)
    {
        if(*str++ == '/')
            base = str;
    }
    return(base);
}

/// @brief File extention of a file name.
/// NOT POSIX
///
/// @param[in] str: string to find extension in.
///
/// @return  pointer to basename extension.
MEMSPACE
char *baseext(char *str)
{
    char *ext = "";

    while(*str)
    {
        if(*str++ == '.')
            ext = str;
    }
    return(ext);
}


/// @brief POSIX change directory.
///
/// - man page chdir (2).
///
/// @param[in] pathname: directory to change to
///
/// @return 0 on sucess.
/// @return -1 on error with errno set.
MEMSPACE
int chdir(const char *pathname)
{
    errno = 0;

    int res = f_chdir(pathname);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
    return(0);
}

/// @brief POSIX chmod function - change file access permission
/// Unfortunately file f_open modes and f_chmod modes are not the same
/// Files that are open have way more options - but only while the file is open.
///  - so this is a bit of a hack - we can only set read only  - if on one has write perms 
///
/// - man page chmod (2).
///
/// @param[in] pathname: filename string.
/// @param[in] mode: POSIX chmod modes.
///
/// @return fileno on success.
MEMSPACE
int chmod(const char *pathname, mode_t mode)
{
    int rc;
    errno = 0;

    // FIXME for now we combine user,group and other perms and ask if anyone has write perms ?

    // Read only ???
    if ( !( mode & ( S_IWUSR | S_IWGRP | S_IWOTH)))
    {
        // file is read only
        rc = f_chmod(pathname, AM_RDO, AM_RDO);
        if (rc != FR_OK)
        {
            errno = fatfs_to_errno(rc);
            return(-1);
        }
    }
    
    return(0);
}

/// @brief POSIX directory name of a filename.
///  Return the index of the last '/' character.
///
/// - Example:
/// @code
///  dir[0] = 0;
///  ret = dirname(path)
///  if(ret)
///   strncpy(dir,path,ret);
/// @endcode
///
/// @param[in] str: string to examine.
/// @return  0 if no directory part.
/// @return index of last '/' character.
///
MEMSPACE
int dirname(char *str)
{
    int end = 0;
    int ind = 0;

    if(!str)
        return(0);

    while(*str)
    {
        if(*str == '/')
            end = ind;
        ++str;
        ++ind;
    }
    return(end);
}

#if 0
/// @brief POSIX fchmod function - change file access permission
/// FatFS does not have a function that will map to this
///
/// - man page fchmod (2).
///
/// @param[in] fd: file handle
/// @param[in] mode: POSIX chmod modes.
///
/// @return fileno on success.
MEMSPACE
int fchmod(int fd, mode_t mode)
{
    //FIXME TODO
    return (-1);
}
#endif

/// @brief POSIX get current working directory
///
/// - man page getcwd (2).
///
/// @param[in] pathname: directory to change to
///
/// @return 0 on sucess.
/// @return -1 on error with errno set.
MEMSPACE
char *getcwd(char *pathname, int len)
{
    int res;
    errno = 0;

    res = f_getcwd(pathname, len);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(NULL);
    }
    return(pathname);
}

/// @brief POSIX make a directory.
///
/// - man page mkdir (2).
///
/// @param[in] pathname: directory to create
///
/// @return 0 on sucess.
/// @return -1 on error with errno set.
MEMSPACE
int mkdir(const char *pathname, mode_t mode)
{
    errno = 0;

    if(mode)
    {
        if(chmod(pathname, mode))
            return(-1);
    }

    int res = f_mkdir(pathname);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
    return(0);
}

/// @brief POSIX rename a file by name.
///
/// - man page (2).
///
/// @param[in] oldpath: original name.
/// @param[in] newpath: new name.
///
/// @return 0 on success.
/// @return -1 on error with errno set.
MEMSPACE
int rename(const char *oldpath, const char *newpath)
{
/* Rename an object */
    int rc;
    errno = 0;
    rc = f_rename(oldpath, newpath);
    if(rc)
    {
        errno = fatfs_to_errno(rc);
        return(-1);
    }
    return(0);
}

/// @brief POSIX delete a directory.
///
/// - man page rmdir (2).
///
/// @param[in] pathname: directory to delete.
///
/// @return 0 on sucess.
/// @return -1 on error with errno set.
MEMSPACE
int rmdir(const char *pathname)
{
    errno = 0;
    int res = f_unlink(pathname);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
    return(0);
}


/// @brief POSIX delete a file.
///
/// - man page unlink (2).
///
/// @param[in] pathname: filename to delete.
///
/// @return 0 on sucess.
/// @return -1 on error with errno set.
MEMSPACE
int unlink(const char *pathname)
{
    errno = 0;
    int res = f_unlink(pathname);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
    return(0);
}

// =============================================
// =============================================
///   - POSIX - directory scanning functions
// =============================================
// =============================================
/// @brief POSIX closedir
/// - man page closedir (2).
///
/// @param[in] dirp: DIR * directory handle
///
/// @return 0 on sucess.
/// @return -1 on error with errno set.
int closedir(DIR *dirp)
{
    int res = f_closedir (dirp);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(-1);
    }
    return(0);
}

/// @brief POSIX opendir
/// - man page opendir (2).
///
/// @param[in] pathname: directory to delete.
///
/// @return DIR * on sucess.
/// @return NULL on error with errno set.
static DIR _dp;
DIR *opendir(const char *pathdir)
{
    int res = f_opendir((DIR *) &_dp, pathdir);
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(NULL);
    }
    return ((DIR *) &_dp);
}

/// @brief POSIX opendir
/// - man page readdir(2).
///
/// @param[in] dirp: DIR * directory handle
///
/// @return DIR * on sucess.
/// @return NULL on error with errno set.
static  dirent_t _de;
dirent_t * readdir(DIR *dirp)
{
    FILINFO fno;
    int len;
    int res;

    _de.d_name[0] = 0;
    res = f_readdir ( dirp, &fno );
    if(res != FR_OK)
    {
        errno = fatfs_to_errno(res);
        return(NULL);
    }
    len = strlen(fno.fname);
    strncpy(_de.d_name,fno.fname,len);
    _de.d_name[len] = 0;
    return( (dirent_t *) &_de);
}

// =============================================
// =============================================
///  - POSIX error functions
// =============================================
// =============================================

/// @brief clrerror resets stream EOF and error flags
/// - man page clrerror(3).
///
/// @param[in] stream: POSIX stream pointer.
/// @return EOF on error with errno set.
MEMSPACE
void clrerror(FILE *stream)
{
    stream->flags &= ~__SEOF;
    stream->flags &= ~__SERR;
}

/// @brief ferror reports if the stream has an error flag set
/// - man page ferror (3).
///
/// @param[in] stream: POSIX stream pointer.
/// @return 1 if EOF set, 0 otherwise.
MEMSPACE
int ferror(FILE *stream)
{
    if(stream->flags & __SERR)
        return(1);
    return(0);
}

/// @brief POSIX perror() -  convert POSIX errno to text with user message.
///
/// - man page errno (3).
///
/// @param[in] s: User message displayed before the error message
///
/// @see sys_errlist[].
/// @return  void.
MEMSPACE
void perror(const char *s)
{
    const char *ptr = NULL;


    if(errno >=0 && errno < EBADMSG)
        ptr = sys_errlist[errno];
    else
        ptr = sys_errlist[EBADMSG];

    if(s && *s)
        printf("%s: %s\n", s, ptr);
    else
        printf("%s\n", ptr);
}

/// @brief POSIX strerror() -  convert POSIX errno to text with user message.
///
/// - man page strerror (3).
///
/// @param[in] errnum: index for sys_errlist[]
///
/// @see sys_errlist[].
/// @return  char *
MEMSPACE
char 
WEAK_ATR *strerror(int errnum)
{
    return( (char *)sys_errlist[errnum] );
}


/// @brief POSIX strerror_r() -  convert POSIX errno to text with user message.
///
/// - man page strerror (3).
///
/// @param[in] errnum: index for sys_errlist[]
/// @param[in] buf: user buffer for error message
/// @param[in] buflen: length of user buffer for error message
///
/// @see sys_errlist[].
/// @return  char *
MEMSPACE
char *strerror_r(int errnum, char *buf, size_t buflen)
{
        strncpy(buf, sys_errlist[errnum], buflen);
        return(buf);
}


// =============================================
// =============================================
///  Device open functions
// =============================================
// =============================================
/// @brief  Assign stdin,stdout,stderr or any use defined I/O
/// NOT POSIX
///
/// @param[in] *put: putc function pointer
/// @param[in] *get: gutc function pointer
MEMSPACE
FILE *
fdevopen(int (*put)(char, FILE *), int (*get)(FILE *))
{
    FILE *s;

    if (put == 0 && get == 0)
        return 0;

    if ((s = safecalloc(1, sizeof(FILE))) == 0)
        return 0;

    s->flags = __SMALLOC;

    if (get != 0) {
        s->get = get;
        s->flags |= __SRD;
        // We assign the first device with a read discriptor to stdin
        // Only assign once
        if (stdin == 0)
            stdin = s;
    }

    if (put != 0) {
        s->put = put;
        s->flags |= __SWR;
        // NOTE: We assign the first device with a write to both STDOUT andd STDERR

        // Only assign in unassigned
        if (stdout == 0) 
            stdout = s;
        if (stderr == 0)
            stderr = s;
    }

    return s;
}


// =============================================
// =============================================
/// - FatFS to POSIX bridge functions
// =============================================
// =============================================

/// @brief Formt SD card
/// @param[in] *name: device name
/// @retrun void
int mkfs(char *name)
{
    FATFS fs;
    uint8_t *mem;
    int res;
    int len;
    int c;
    char dev[4];

    len = MATCH(name,"/dev/sd");
    if(!len)
    {
        printf("Expected /dev/sda .. /dev/sdj\n");
        return(0);
    }
    // Convert /dev/sd[a-j] to 0: .. 9:
    dev[1] = ':';
    dev[2] = 0;
    c = tolower( name[len-1] );
    if(c >= 'a' && c <= ('a' + 9))
        dev[0] = (c - 'a');
    dev[3] = 0;

    // Register work area to the logical drive 0:
    res = f_mount(&fs, dev, 0);                    
    if(!res)
    {
        put_rc(res);
        return(0);
    }

    // Allocate memory for mkfs function
    mem = safemalloc(1024);
    if(!mem)
        return(0);

    // Create FAT volume on the logical drive 0
    // 2nd argument is ignored. */
    res = f_mkfs(dev, FM_FAT32, 0, mem, 1024);
    if(res)
    {
        put_rc(res);
        safefree(mem);
        return(0);
    }
    safefree(mem);
    return(1);
}

/// @brief Private FatFs function called by fgetc() to get a byte from file stream
/// FIXME buffer this function call
/// NOT POSIX
/// open() assigns stream->get = fatfs_getc() 
///
/// - man page fgetc (3).
/// - Notes: fgetc does all tests prior to caling us, including ungetc.
///
/// @param[in] stream: POSIX stream pointer.
///
/// @return character.
/// @return EOF on error with errno set.
MEMSPACE
int  fatfs_getc(FILE *stream)
{
    FIL *fh;
    UINT size;
    int res;
    uint8_t c;
    long pos;

    errno = 0;

    if(stream == NULL)
    {
        errno = EBADF;                            // Bad File Number
        return(EOF);
    }

    fh = (FIL *) fdev_get_udata(stream);
    if(fh == NULL)
    {
        errno = EBADF;                            // Bad File Number
        return(EOF);
    }

    res = f_read(fh, &c, 1, (UINT *) &size);
    if( res != FR_OK || size != 1)
    {
        errno = fatfs_to_errno(res);
        stream->flags |= __SEOF;
        return(EOF);
    }

    // AUTOMATIC end of line METHOD detection
    // ALWAYS return '\n' for ALL methods
    // History: End of line (EOL) characters sometimes differ, mostly legacy systems, and modern UNIX (which uses just '\n')
    //    '\r' ONLY 
    //    '\r\n' 
    //    '\n' 
    // The difference was mostly from the way old mechanical printers were controlled.
    //    '\n' (New Line = NL) advanced the line
    //    '\r' (Charage Return = CR) moved the print head to start of line 
    //    '\t' (Tabstop = TAB)
    //    '\f' (Form feed = FF)
    // The problem with mechanical devices is that each had differing control and time delays to deal with.
    //  (TAB, CR, NL and FF) did different things and took differing times depending on the device.
    //
    // Long before DOS UNIX took the position that controlling physical devices must be a device drivers problem only.
    // They reasoned if users had to worry about all the ugly controll and timing issues no code would be portable.
    // Therefore they made NL just a SYMBOL for the driver to determine what to do.
    // This design philosophy argued if you needed better control its better to use a real designed purposed tool for it.
    // (ie. like curses or termcap).

    // Here to deal with those other old ugly stupid pointless EOL methods we convert to just a symbol '\n'
    // FROM '\n' OR '\r'char OR '\r\n' TO '\n'
    // Note: char != '\n'
    if(c == '\r')
    {
        // PEEK forward 1 character
        pos = f_tell(fh);
        // Check for trailing '\n' or EOF
        res = f_read(fh, &c, 1, (UINT *) &size);
        if(res != FR_OK || size != 1)
        {
            // '\r' with EOF impiles '\n'
            return('\n');
        }
        // This file must be '\r' ONLY for end of line
        if(c != '\n')
        {
            // Not '\n' or EOF o move file pointer back to just after the '\r'
            f_lseek(fh, pos);
            return('\n');
        }
        c = '\n';
    }
    return(c & 0xff);
}

/// @brief Private FatFs function called by fputc() to put a byte from file stream
/// NOT POSIX
/// open() assigns stream->put = fatfs_putc() 
///
/// - man page fputc (3).
/// - Notes: fputc does all tests prior to caling us.
///
/// @param[in] c: character.
/// @param[in] stream: POSIX stream pointer.
///
/// @return character 
/// @return EOF on error with errno set.
MEMSPACE
int fatfs_putc(char c, FILE *stream)
{
    int res;
    FIL *fh;
    UINT size;

    errno = 0;
    if(stream == NULL)
    {
        errno = EBADF;                            // Bad File Number
        return(EOF);
    }

    fh = (FIL *) fdev_get_udata(stream);
    if(fh == NULL)
    {
        errno = EBADF;                            // Bad File Number
        return(EOF);
    }

    res = f_write(fh, &c, 1, (UINT *)  &size);
    if( res != FR_OK || size != 1)
    {
        errno = fatfs_to_errno(res);
        stream->flags |= __SEOF;
        return(EOF);
    }
    return(c);
}

/// @brief Convert FafFs error result to POSIX errno.
/// NOT POSIX
///
/// - man page errno (3).
///
/// @param[in] Result: FatFs Result code.
///
/// @return POSIX errno.
/// @return EBADMSG if no conversion possible.
MEMSPACE
int fatfs_to_errno( FRESULT Result )
{
    switch( Result )
    {
        case FR_OK:              /* FatFS (0) Succeeded */
            return (0);          /* POSIX OK */
        case FR_DISK_ERR:        /* FatFS (1) A hard error occurred in the low level disk I/O layer */
            return (EIO);        /* POSIX Input/output error (POSIX.1) */

        case FR_INT_ERR:         /* FatFS (2) Assertion failed */
            return (EPERM);      /* POSIX Operation not permitted (POSIX.1) */

        case FR_NOT_READY:       /* FatFS (3) The physical drive cannot work */
            return (EBUSY);      /* POSIX Device or resource busy (POSIX.1) */

        case FR_NO_FILE:         /* FatFS (4) Could not find the file */
            return (ENOENT);     /* POSIX No such file or directory (POSIX.1) */

        case FR_NO_PATH:         /* FatFS (5) Could not find the path */
            return (ENOENT);     /* POSIX No such file or directory (POSIX.1) */

        case FR_INVALID_NAME:    /* FatFS (6) The path name format is invalid */
            return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

        case FR_DENIED:          /* FatFS (7) Access denied due to prohibited access or directory full */
            return (EACCES);     /* POSIX Permission denied (POSIX.1) */
        case FR_EXIST:           /* FatFS (8) Access denied due to prohibited access */
            return (EACCES);     /* POSIX Permission denied (POSIX.1) */

        case FR_INVALID_OBJECT:  /* FatFS (9) The file/directory object is invalid */
            return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

        case FR_WRITE_PROTECTED: /* FatFS (10) The physical drive is write protected */
            return(EROFS);       /* POSIX Read-only filesystem (POSIX.1) */

        case FR_INVALID_DRIVE:   /* FatFS (11) The logical drive number is invalid */
            return(ENXIO);       /* POSIX No such device or address (POSIX.1) */

        case FR_NOT_ENABLED:     /* FatFS (12) The volume has no work area */
            return (ENOSPC);     /* POSIX No space left on device (POSIX.1) */

        case FR_NO_FILESYSTEM:   /* FatFS (13) There is no valid FAT volume */
            return(ENXIO);       /* POSIX No such device or address (POSIX.1) */

        case FR_MKFS_ABORTED:    /* FatFS (14) The f_mkfs() aborted due to any parameter error */
            return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

        case FR_TIMEOUT:         /* FatFS (15) Could not get a grant to access the volume within defined period */
            return (EBUSY);      /* POSIX Device or resource busy (POSIX.1) */

        case FR_LOCKED:          /* FatFS (16) The operation is rejected according to the file sharing policy */
            return (EBUSY);      /* POSIX Device or resource busy (POSIX.1) */


        case FR_NOT_ENOUGH_CORE: /* FatFS (17) LFN working buffer could not be allocated */
            return (ENOMEM);     /* POSIX Not enough space (POSIX.1) */

        case FR_TOO_MANY_OPEN_FILES:/* FatFS (18) Number of open files > _FS_SHARE */
            return (EMFILE);     /* POSIX Too many open files (POSIX.1) */

        case FR_INVALID_PARAMETER:/* FatFS (19) Given parameter is invalid */
            return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

    }
    return (EBADMSG);            /* POSIX Bad message (POSIX.1) */
}


/// @brief Convert FatFS file handle to POSIX fileno.
/// NOT POSIX
///
/// @param[in] fh: FatFS file pointer.
///
/// @return fileno on success.
/// @return -1 on error with errno set to EBADF.
MEMSPACE
int fatfs_to_fileno(FIL *fh)
{
    int i;

    FILE *stream;

    if(fh == NULL)
    {
        errno = EBADF;
        return(-1);
    }

    for(i=0;i<MAX_FILES;++i)
    {
        stream = __iob[i];
        if(stream)
        {
            if( fh == (FIL *) fdev_get_udata(stream) )
                return(i);
        }
    }
    errno = EBADF;
    return(-1);
}

/// @brief Convert FatFs file date and time to POSIX epoch seconds.
/// NOT POSIX
///
/// - man page timegm (3).
///
/// @param[in] date: FatFs date.
/// @param[in] time: FatFs time.
///
/// @see timegm()
///
/// @return epoch seconds 
MEMSPACE
time_t fat_time_to_unix(uint16_t date, uint16_t time)
{
    struct tm tp;
    time_t unix;

    memset(&tp, 0, sizeof(struct tm));

    tp.tm_sec = (time << 1) & 0x3e;               // 2 second resolution
    tp.tm_min = ((time >> 5) & 0x3f);
    tp.tm_hour = ((time >> 11) & 0x1f);
    tp.tm_mday = (date & 0x1f);
    tp.tm_mon = ((date >> 5) & 0x0f) - 1;
    tp.tm_year = ((date >> 9) & 0x7f) + 80;
    unix = timegm( &tp );
    return( unix );
}

/// @brief Convert Linux POSIX time_t to FAT32 date and time.
/// NOT POSIX
/// - man page gmtime (3).
/// @param[in] epoch: unix epoch seconds
/// @param[in] *date: fat32 date
/// @param[in] *time: fat32 time
/// @return  void
MEMSPACE
void unix_time_to_fat(time_t epoch, uint16_t *date, uint16_t *time)
{
    tm_t *t = gmtime((time_t *) &epoch);

/* Pack date and time into a uint32_t variable */
    *date = ((uint16_t)(t->tm_year - 80) << 9)
        | (((uint16_t)t->tm_mon+1) << 5)
        | (((uint16_t)t->tm_mday));

    *time = ((uint16_t)t->tm_hour << 11)
        | ((uint16_t)t->tm_min << 5)
        | ((uint16_t)t->tm_sec >> 1);
}

/// @brief  Convert POSIX fileno to FatFS handle
/// NOT POSIX
///
/// - FatFS file handle is pointed to by the avr-libc stream->udata.
///
/// @param[in] fileno: fileno of file
///
/// @return FIL * FatFS file handle on success.
/// @return NULL if POSIX fileno is invalid NULL 
MEMSPACE
FIL *fileno_to_fatfs(int fileno)
{
    FILE *stream;
    FIL *fh;

    if(isatty( fileno ))
    {
        errno = EBADF;
        return(NULL);
    }

    // checks if fileno out of bounds
    stream = fileno_to_stream(fileno);
    if( stream == NULL )
        return(NULL);

    fh = fdev_get_udata(stream);
    if(fh == NULL)
    {
        errno = EBADF;
        return(NULL);
    }
    return(fh);
}



/// @brief  Free POSIX fileno FILE descriptor.
/// NOT POSIX
///
/// @param[in] fileno: POSIX file number __iob[] index.
///
/// @return fileno on success.
/// @return -1 on failure.
MEMSPACE
int free_file_descriptor(int fileno)
{
    FILE *stream;
    FIL *fh;

    if(isatty( fileno ))
    {
        errno = EBADF;
        return(-1);
    }

    // checks if fileno out of bounds
    stream = fileno_to_stream(fileno);
    if(stream == NULL)
    {
        return(-1);
    }

    fh = fdev_get_udata(stream);

    if(fh != NULL)
    {
        safefree(fh);
    }

    if(stream->buf != NULL && stream->flags & __SMALLOC)
    {
        safefree(stream->buf);
    }

    __iob[fileno]  = NULL;
    safefree(stream);
    return(fileno);
}



// =============================================
/// @brief Allocate a POSIX FILE descriptor.
/// NOT POSIX
///
/// @return fileno on success.
/// @return -1 on failure with errno set.
MEMSPACE
int new_file_descriptor( void )
{
    int i;
    FILE *stream;
    FIL *fh;

    for(i=0;i<MAX_FILES;++i)
    {
        if(isatty(i))
            continue;
        if( __iob[i] == NULL)
        {
            stream = (FILE *) safecalloc(sizeof(FILE),1);
            if(stream == NULL)
            {
                errno = ENOMEM;
                return(-1);
            }
            fh = (FIL *) safecalloc(sizeof(FIL),1);
            if(fh == NULL)
            {
                safefree(stream);
                errno = ENOMEM;
                return(-1);
            }

            __iob[i]  = stream;
            fdev_set_udata(stream, (void *) fh);
            return(i);
        }
    }
    errno = ENFILE;
    return(-1);
}

/// @brief Convert POSIX fopen mode to POSIX open mode flags.
/// NOT POSIX
///
/// - man page fopen (3).
/// - man page open (2).
/// - Valid modes.
/// - Read
///  - "r", "rb"
/// - Read and Write
///  - "r+", "r+b", "rb+"
/// - Write
///  - "w", "wb"
/// - Write and Read.
///  - "w+", "w+b", "wb+"
///  - "w+" implies write/read access.
/// - Append
///  - "a", "ab"
/// - Append and Read
///  - "a+", "a+b", "ab+"
/// - Note: ORDER IS IMPORTANT! so w+ is NOT the same as r+.
/// - ALWAYS do a fflush or fseek between rear write operations if + is used..
///
/// @param[in] mode: POSIX file mode string.
///
/// @return open mode flags.
/// @return -1 on error.
/// @warning read and write BOTH share the same stream buffer and buffer index pointers.
MEMSPACE
int posix_fopen_modes_to_open(const char *mode)
{
    int flag = 0;

    if(modecmp(mode,"r") || modecmp(mode,"rb"))
    {
        flag = O_RDONLY;
        return(flag);
    }
    if(modecmp(mode,"r+") || modecmp(mode, "r+b" ) || modecmp(mode, "rb+" ))
    {
        flag = O_RDWR | O_TRUNC;
        return(flag);
    }
    if(modecmp(mode,"w") || modecmp(mode,"wb"))
    {
        flag = O_WRONLY | O_CREAT | O_TRUNC;
        return(flag);
    }
    if(modecmp(mode,"w+") || modecmp(mode, "w+b" ) || modecmp(mode, "wb+" ))
    {
        flag = O_RDWR | O_CREAT | O_TRUNC;
        return(flag);
    }
    if(modecmp(mode,"a") || modecmp(mode,"ab"))
    {
        flag = O_WRONLY | O_CREAT | O_APPEND;
        return(flag);
    }
    if(modecmp(mode,"a+") || modecmp(mode, "a+b" ) || modecmp(mode, "ab+" ))
    {
        flag = O_RDWR | O_CREAT | O_APPEND;
        return(-1);
    }
    return(-1);                                   // nvalid mode
}

// =============================================
// =============================================
/// - POSIX fprintf function
// =============================================
// =============================================



/// @brief fprintf character write function
/// @param[in] *p: printf user buffer
/// @param[in] ch: character
/// TODO if fputc fails we might want to also set an error in the *p structure - error in the stream will already be set
MEMSPACE
static void _fprintf_putc(struct _printf_t *p, char ch)
{
        p->sent++;
        fputc(ch, (FILE *) p->buffer);
}


/// @brief fprintf function
///  Example user defined printf function using fputc for I/O
///  This method allows I/O to devices and strings without typical C++ overhead
/// @param[in] *fp: FILE stream pointer
/// @param[in] fmt: printf forat string
/// @param[in] ...: vararg list or arguments
/// @return size of printed result
MEMSPACE
int
fprintf(FILE *fp, const char *format, ...)
{
    printf_t fn;
    va_list va;

    fn.put = _fprintf_putc;
    fn.sent = 0;
    fn.buffer = (void *) fp;

    va_start(va, format);
    _printf_fn(&fn, format, va);
    va_end(va);

    return ((int)fn.sent);
}

