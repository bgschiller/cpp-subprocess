/* The following code declares classes to read from and write to
 * file descriptore or file handles.
 *
 * See
 *      http://www.josuttis.com/cppcode
 * for details and the latest version.
 *
 * - open:
 *      - integrating BUFSIZ on some systems?
 *      - optimized reading of multiple characters
 *      - stream for reading AND writing
 *      - i18n
 *
 * (C) Copyright Nicolai M. Josuttis 2001.
 * Permission to copy, use, modify, sell and distribute this software
 * is granted provided this copyright notice appears in all copies.
 * This software is provided "as is" without express or implied
 * warranty, and with no claim as to its suitability for any purpose.
 *
 * Version: Jul 28, 2002
 * History:
 *  Jul 28, 2002: bugfix memcpy() => memmove()
 *                fdinbuf::underflow(): cast for return statements
 *  Aug 05, 2001: first public version
 */
#ifndef BOOST_FDSTREAM_HPP
#define BOOST_FDSTREAM_HPP

#include <istream>
#include <ostream>
#include <streambuf>
#include <sstream>
// for EOF:
#include <cstdio>
// for memmove():
#include <cstring>


// low-level read and write functions
#ifdef _MSC_VER
# include <io.h>
#else
# include <unistd.h>
//extern "C" {
//    int write (int fd, const char* buf, int num);
//    int read (int fd, char* buf, int num);
//}
#endif


// BEGIN namespace BOOST
namespace boost {


/************************************************************
 * fdostream
 * - a stream that writes on a file descriptor
 ************************************************************/


class fdoutbuf : public std::streambuf {
  protected:
    int fd;    // file descriptor
    bool _is_open;
  public:
    // constructor
    fdoutbuf (int _fd)
    : fd(_fd)
    , _is_open{true}
    { }

    fdoutbuf (fdoutbuf&& other)
    : fd(other.fd)
    , _is_open{other._is_open}
    {
      if (this != &other) {
        other._is_open = false; // prevent other's dtor from closing this fd.
      }
    }

    virtual ~fdoutbuf() {
      close();
    }

    fdoutbuf& operator=(fdoutbuf&& other) {
      fd = other.fd;
      _is_open = other._is_open;
      if (this != &other) {
        other._is_open = false; // prevent other's dtor from closing this fd.
      }
      return *this;
    }

    bool is_open() const { return _is_open; }

    void close() {
      if (is_open()) {
        ::close(fd);
      }
      _is_open = false;
    }

  protected:
    // write one character
    virtual int_type overflow (int_type c) {
        if (c != EOF) {
            char z = static_cast<char>(c);
            if (write (fd, &z, 1) != 1) {
                return EOF;
            }
        }
        return c;
    }
    // write multiple characters
    virtual
    std::streamsize xsputn (const char* s,
                            std::streamsize num) {
        return write(fd, s, static_cast<size_t>(num));
    }
};

class fdostream : public std::ostream {
  protected:
    fdoutbuf buf;
  public:
    fdostream (int fd) : std::ostream(0), buf(fd) {
        rdbuf(&buf);
    }

    fdostream (fdostream&& other)
    : std::ostream(0)
      , buf{std::move(other.buf)} {
        rdbuf(&buf);
      }

    fdostream& operator=(fdostream&& other) {
      buf = std::move(other.buf);
      return *this;
    }

    void close() {
      buf.close();
    }

    bool is_open() const {
      return buf.is_open();
    }
};


/************************************************************
 * fdistream
 * - a stream that reads on a file descriptor
 ************************************************************/

class fdinbuf : public std::streambuf {
  protected:
    int fd;    // file descriptor
    bool _is_open;
  protected:
    /* data buffer:
     * - at most, pbSize characters in putback area plus
     * - at most, bufSize characters in ordinary read buffer
     */
    static const int pbSize = 4;        // size of putback area
    static const int bufSize = 1024;    // size of the data buffer
    char buffer[bufSize+pbSize];        // data buffer

  public:
    /* constructor
     * - initialize file descriptor
     * - initialize empty data buffer
     * - no putback area
     * => force underflow()
     */
    fdinbuf (int _fd)
    : fd(_fd)
    , _is_open{true}
    {
        setg (buffer+pbSize,     // beginning of putback area
              buffer+pbSize,     // read position
              buffer+pbSize);    // end position
    }

    fdinbuf (fdinbuf&& other)
    : fd{other.fd}
    , _is_open{other._is_open}
    {
      if (this != &other) {
        std::move(other.eback(), other.egptr(), buffer);
        setg(other.eback(), other.gptr(), other.egptr());
        other._is_open = false;
      }
    }

    virtual ~fdinbuf() {
      close();
    }

    fdinbuf& operator=(fdinbuf&& other) {
      if (this != &other) {
        fd = other.fd;
        _is_open = other._is_open;
        std::move(other.eback(), other.egptr(), buffer);
        setg(other.eback(), other.gptr(), other.egptr());
        other._is_open = false;
      }
      return *this;
    }

    bool is_open() const {
      return _is_open;
    }

    void close() {
      if (is_open()) ::close(fd);
      _is_open = false;
    }

  protected:
    // insert new characters into the buffer
    virtual int_type underflow () {
#ifndef _MSC_VER
        using std::memmove;
#endif
        // is read position before end of buffer?
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }

        /* process size of putback area
         * - use number of characters read
         * - but at most size of putback area
         */
        size_t numPutback = static_cast<size_t>(gptr() - eback());
        if (numPutback > pbSize) {
            numPutback = pbSize;
        }

        /* copy up to pbSize characters previously read into
         * the putback area
         */
        memmove (buffer+(pbSize-numPutback), gptr()-numPutback,
                numPutback);
        // read at most bufSize new characters
        auto num = read (fd, buffer+pbSize, bufSize);
        if (num <= 0) {
            // ERROR or EOF
            return EOF;
        }

        // reset buffer pointers
        setg (buffer+(pbSize-numPutback),   // beginning of putback area
              buffer+pbSize,                // read position
              buffer+pbSize+num);           // end of buffer

        // return next character
        return traits_type::to_int_type(*gptr());
    }
};

class fdistream : public std::istream {
  protected:
    fdinbuf buf;
  public:
    fdistream (int fd) : std::istream(0), buf(fd) {
        rdbuf(&buf);
    }

    fdistream(fdistream&& other)
    : std::istream(0)
    , buf{std::move(other.buf)} {
       rdbuf(&buf);
    }

    fdistream& operator=(fdistream&& other) {
      buf = std::move(other.buf);
      return *this;
    }

    std::string slurp() {
      std::stringstream buffer;
      buffer << rdbuf();
      return buffer.str();
    }

    void close() {
      dynamic_cast<fdoutbuf*>(rdbuf())->close();
    }
};


} // END namespace boost

#endif /*BOOST_FDSTREAM_HPP*/
