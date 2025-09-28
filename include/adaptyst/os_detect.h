#ifndef ADAPTYST_OS_DETECT_H_
#define ADAPTYST_OS_DETECT_H_

/**
   \def ADAPTYST_UNIX
   Defined if Adaptyst is compiled for a Unix-based system such as Linux.
*/

#if defined(unix) || defined(__unix) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE)
#define ADAPTYST_UNIX
#endif

#endif
