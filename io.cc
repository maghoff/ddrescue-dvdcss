/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004-2016 Antonio Diaz Diaz.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _FILE_OFFSET_BITS 64

#include <cerrno>
#include <climits>
#include <csignal>
#include <string>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "block.h"
#include "mapbook.h"

#ifdef DDRESCUE_USE_DVDREAD
extern "C" {
#include <dvdread/dvd_reader.h>
}
#endif


namespace {

int volatile signum_ = 0;		// user pressed Ctrl-C or similar
extern "C" void sighandler( int signum )
  { if( signum_ == 0 && signum > 0 ) signum_ = signum; }


int set_signal( const int signum, void (*handler)( int ) )
  {
  struct sigaction new_action;

  new_action.sa_handler = handler;
  sigemptyset( &new_action.sa_mask );
  new_action.sa_flags = SA_RESTART;
  return sigaction( signum, &new_action, 0 );
  }

} // end namespace


// Returns the number of bytes really read.
// If (returned value < size) and (errno == 0), means EOF was reached.
//
int readblock( const int fd, uint8_t * const buf, const int size,
               const long long pos )
  {
  int sz = 0;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
    while( sz < size )
      {
      errno = 0;
      const int n = read( fd, buf + sz, size - sz );
      if( n > 0 ) sz += n;
      else if( n == 0 ) break;				// EOF
      else if( errno != EINTR ) break;
      }
  return sz;
  }

// #define READBLOCK_DVDREAD_DEBUG

#ifdef DDRESCUE_USE_DVDREAD
// Returns the number of bytes really read.
// If (returned value < size) and (errno == 0), means EOF was reached.
//
int readblock_dvdread( dvd_reader_t *dvd, uint32_t dvd_blocks, uint8_t * const buf, const int size,
                       const long long pos ) {
  uint32_t lb, n, n_read;
  /* Reset errno */
  errno = 0;
  /* We can only seek to LBs */
  if (pos % 2048 != 0) {
#ifdef READBLOCK_DVDREAD_DEBUG
    printf("readblock_dvdread(): pos %lld not a multiple of block size (size %d)\n",
           pos, size);
#endif
    errno = EINVAL;
    return -1;
  }
  /* We can only read an integer number of blocks */
  if (size % 2048 != 0) {
#ifdef READBLOCK_DVDREAD_DEBUG
    printf("readblock_dvdread(): size %d not a multiple of block size (pos %lld)\n",
           size, pos);
#endif
    errno = EINVAL;
    return -1;
  }
  lb = pos / 2048;
  n = size / 2048;
  n_read = DVDReadRawBlocks(dvd, buf, lb, n, 1);
  if (n_read < n) {
    if (lb + n_read < dvd_blocks) {
      /* Set errno so caller knows this isn't EOF */
      errno = EIO;
    }
    /* else EOF */
  }
#ifdef READBLOCK_DVDREAD_DEBUG
  if (n_read == 0) {
    if (errno) {
      printf("readblock_dvdread(%u/%u): returning zero length but errno %d\n", lb, n, errno);
    } else {
      printf("readblock_dvdread(%u/%u): returning zero length/zero errno\n", lb, n);
    }
  } else {
    printf("readblock_dvdread(%u/%u): returning %u / %d\n", lb, n, n_read, errno);
  }
#endif
  return n_read * 2048;
}
#endif


// Returns the number of bytes really written.
// If (returned value < size), it is always an error.
//
int writeblock( const int fd, const uint8_t * const buf, const int size,
                const long long pos )
  {
  int sz = 0;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
    while( sz < size )
      {
      errno = 0;
      const int n = write( fd, buf + sz, size - sz );
      if( n > 0 ) sz += n;
      else if( n < 0 && errno != EINTR ) break;
      }
  return sz;
  }


bool interrupted() { return ( signum_ > 0 ); }


void set_signals()
  {
  signum_ = 0;
  set_signal( SIGHUP, sighandler );
  set_signal( SIGINT, sighandler );
  set_signal( SIGTERM, sighandler );
  set_signal( SIGUSR1, SIG_IGN );
  set_signal( SIGUSR2, SIG_IGN );
  }

int signaled_exit()
  {
  set_signal( signum_, SIG_DFL );
  std::raise( signum_ );
  return 128 + signum_;			// in case std::raise fails to exit
  }
