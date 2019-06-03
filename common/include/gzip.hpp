/* zpipe.c: example of proper use of zlib's inflate() and deflate()
   Not copyrighted -- provided to the public domain
   Version 1.4  11 December 2005  Mark Adler */

/* Version history:
   1.0  30 Oct 2004  First version
   1.1   8 Nov 2004  Add void casting for unused return values
                     Use switch statement for inflate() return values
   1.2   9 Nov 2004  Add assertions to document zlib guarantees
   1.3   6 Apr 2005  Remove incorrect assertion in inf()
   1.4  11 Dec 2005  Add hack to avoid MSDOS end-of-line conversions
                     Avoid some compiler warnings for input and output buffers
 */
#pragma once

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <zlib.h>
#include <iostream>

#define CHUNK 16384

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
int def(FILE *source, FILE *dest, int level)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, level);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf(FILE *source, FILE *dest)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;
                (void)inflateEnd(&strm);
                return ret;
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

/* report a zlib or i/o error */
void throw_zlib_error(int ret) {
    switch (ret) {
        case Z_ERRNO:
            throw std::runtime_error("Z_ERRNO");
        case Z_STREAM_ERROR:
            throw std::runtime_error("invalid compression level");
        case Z_DATA_ERROR:
            throw std::runtime_error("invalid or incomplete deflate data");
        case Z_MEM_ERROR:
            throw std::runtime_error("out of memory");
        case Z_VERSION_ERROR:
            throw std::runtime_error("zlib version mismatch!");
        default:
            throw std::runtime_error("Unknown error: " + std::to_string(ret));
    }
}

class Gzip {
  public:
    static void compress(const std::string& src_path, const int level = 6, std::string dst_path = "") {
        FILE *src = fopen(src_path.c_str(), "rb");
        if (src == nullptr)
            throw std::runtime_error(std::string("Cannot open ") + src_path);
        if (dst_path.empty())
            dst_path = src_path + ".gzip";
        FILE *dst = fopen(dst_path.c_str(), "wb");
        if (dst == nullptr)
            throw std::runtime_error(std::string("Cannot open ") + dst_path);
        
        int ret = def(src, dst, level);
        
        if (fclose(src) != 0)
            perror((std::string("fclose: ") + src_path).c_str());
        if (fclose(dst) != 0)
            perror((std::string("fclose: ") + dst_path).c_str());

        if (ret != Z_OK)
            throw_zlib_error(ret);
    }

    static void decompress(const std::string& src_path, std::string dst_path="") {
        FILE *src = fopen(src_path.c_str(), "rb");
        if (src == nullptr)
            throw std::runtime_error(std::string("Cannot open ") + src_path);
        if (dst_path.empty())
            dst_path = src_path.substr(0, src_path.size() - 5);
        FILE *dst = fopen(dst_path.c_str(), "wb");
        if (dst == nullptr)
            throw std::runtime_error(std::string("Cannot open ") + dst_path);

        int ret = inf(src, dst);

        if (fclose(src) != 0)
            perror((std::string("fclose: ") + src_path).c_str());
        if (fclose(dst) != 0)
            perror((std::string("fclose: ") + dst_path).c_str());

        if (ret != Z_OK)
            throw_zlib_error(ret);
    }
};
